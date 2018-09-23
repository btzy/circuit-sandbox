/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#ifndef _WIN32

// use the usual shared memory object from boost, but remove the shared memory from the system in the destructor.

#include <string>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

namespace ext {
    /**
     * Represents a shared memory buffer that will be deleted when no longer referenced by any process.
     * New memory is zeroed by POSIX API ftruncate() - called by boost::interprocess::shared_memory_object::truncate.
     */
    class autoremove_shared_memory {
    private:
        boost::interprocess::shared_memory_object shm;
        boost::interprocess::mapped_region region;
        using counter_t = std::atomic<std::size_t>;
        static_assert(counter_t::is_always_lock_free, "Need always lock-free atomics!");
        std::size_t counter_offset;
        template <typename OpenMode>
        autoremove_shared_memory(const char* name, OpenMode open_mode, boost::interprocess::mode_t mode) :
            shm(open_mode, name, mode) {
        }
        std::size_t ref_increment() noexcept {
            counter_t* counter = reinterpret_cast<counter_t*>(static_cast<char*>(region.get_address()) + counter_offset);
            return counter->fetch_add(1, std::memory_order_acq_rel);
        }
        std::size_t ref_decrement() noexcept {
            counter_t* counter = reinterpret_cast<counter_t*>(static_cast<char*>(region.get_address()) + counter_offset);
            return counter->fetch_sub(1, std::memory_order_acq_rel);
        }
    public:
        using size_type = boost::interprocess::offset_t;
        template <typename OpenMode = boost::interprocess::open_or_create_t>
        autoremove_shared_memory(const char* name, size_type size, OpenMode open_mode = boost::interprocess::open_or_create, boost::interprocess::mode_t mode = boost::interprocess::read_write) :
            autoremove_shared_memory(name, open_mode, boost::interprocess::read_write) {
            // as this is a delegating constructor, the destructor will be called if this constructor throws.
            
            assert(mode == boost::interprocess::read_write || mode == boost::interprocess::read_only);
            // note: on posix, the mode is ignored because we need write access to do reference counting.

            while (true) {
                // add space for the reference counter after the data
                size = ((size - 1) / alignof(counter_t) + 1) * alignof(counter_t);
                counter_offset = static_cast<std::size_t>(size);
                size += sizeof(counter_t);

                size_type existing_size = 0;
                shm.get_size(existing_size);
                if (existing_size < size) {
                    shm.truncate(size);
                }
                region = boost::interprocess::mapped_region(shm, boost::interprocess::read_write);

                // increment the reference counter
                if (ref_increment() == 0 && existing_size > 0) {
                    // if we got here, it means that we are holding some old memory that's just about to be deleted
                    // so we remove the old memory and retry.
                    region = boost::interprocess::mapped_region();
                    shm = boost::interprocess::shared_memory_object();
                    boost::interprocess::shared_memory_object::remove(name);
                    // create a new shared memory object, now we should have fresh new memory.
                    shm = boost::interprocess::shared_memory_object(open_mode, name, boost::interprocess::read_write);
                    continue;
                }

                break;
            }
        }
        autoremove_shared_memory() noexcept {}
        ~autoremove_shared_memory() noexcept {
            // check if we are the last user of this shared memory, and if so we remove the memory.
            if (ref_decrement() == 1) {
                // unbind the region
                region = boost::interprocess::mapped_region();
                // get the name
                std::string name(shm.get_name());
                // destroy the shared memory
                shm = boost::interprocess::shared_memory_object();
                // remove the memory (if possible)
                boost::interprocess::shared_memory_object::remove(name.c_str());
            }
        }
        void* address() const noexcept {
            return region.get_address();
        }
    };
}

#else

// use the Windows shared memory object on Windows systems, so that it will be backed by kernel instead of filesystem, and have the desired lifetime.

#include <stdexcept>
#include <boost/interprocess/windows_shared_memory.hpp>
#include <boost/interprocess/mapped_region.hpp>

namespace ext {
    /**
     * Represents a shared memory buffer that will be deleted when no longer referenced by any process.
     * New memory is zeroed by Windows API CreateFileMappingA() - called by boost::interprocess::windows_shared_memory::windows_shared_memory().
     */
    class autoremove_shared_memory {
    private:
        boost::interprocess::windows_shared_memory shm;
        boost::interprocess::mapped_region region;
    public:
        using size_type = std::size_t;
        autoremove_shared_memory(const char* name, size_type size, boost::interprocess::open_or_create_t open_mode = boost::interprocess::open_or_create, boost::interprocess::mode_t mode = boost::interprocess::read_write) :
            shm(open_mode, name, mode, size) {
            if (static_cast<size_type>(shm.get_size()) < size) {
                throw std::runtime_error("Existing Windows shared memory too small!");
            }
            region = boost::interprocess::mapped_region(shm, mode);
        }
        autoremove_shared_memory(const char* name, size_type size, boost::interprocess::create_only_t open_mode, boost::interprocess::mode_t mode = boost::interprocess::read_write) :
            shm(open_mode, name, mode, size),
            region(shm, mode) {
            // TODO: fix similar issue with open_only mode for POSIX! Also check if open_or_create will throw exception if existing memory too small!
        }
        autoremove_shared_memory(const char* name, size_type size, boost::interprocess::open_only_t open_mode, boost::interprocess::mode_t mode = boost::interprocess::read_write) :
            shm(open_mode, name, mode) {
            if (static_cast<size_type>(shm.get_size()) < size) {
                throw std::runtime_error("Existing Windows shared memory too small!");
            }
            region = boost::interprocess::mapped_region(shm, mode);
        }
        autoremove_shared_memory() noexcept {}
        ~autoremove_shared_memory() noexcept {}
        void* address() const noexcept {
            return region.get_address();
        }
    };
}

#endif
