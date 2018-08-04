#pragma once

#include <atomic>
#include <array>
#include <optional>
#include <string>
#include <sstream>
#include <iostream>
#include <cassert>
#include <SDL.h>
#include "canvasstate.hpp"
#include "sdl_automatic.hpp"
#include "shared_memory.hpp"
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

/**
 * Manages storage of clipboards in a interprocess-shareable manner.
 */

namespace {
    template <bool AlwaysLockFree>
    struct TableEntryImpl;

    template <>
    struct TableEntryImpl<true> {
        struct alignas(std::atomic<uint64_t>) TableEntryType {
            uint32_t id;
            uint32_t size;
        };
        std::atomic<TableEntryType> data;

        static_assert(std::atomic<TableEntryType>::is_always_lock_free);

        std::pair<uint32_t, uint32_t> load() noexcept {
            auto[id, size] = data.load(std::memory_order_acquire);
            return { id, size };
        }
        void store(uint32_t id, uint32_t size) noexcept {
            data.store(TableEntryType{ id, size }, std::memory_order_release);
        }
    };

    template <>
    struct TableEntryImpl<false> {
        struct TableEntryType {
            boost::interprocess::interprocess_mutex mutex;
            uint32_t id;
            uint32_t size;
        };
        TableEntryType data;

        std::pair<uint32_t, uint32_t> load() noexcept {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(data.mutex);
            return { data.id, data.size };
        }
        void store(uint32_t id, uint32_t size) noexcept {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(data.mutex);
            data.id = id;
            data.size = size;
        }
    };

    using TableEntry = TableEntryImpl<std::atomic<typename TableEntryImpl<true>::TableEntryType>::is_always_lock_free>;
}

template <uint32_t NumClipboards>
class ClipboardStore {
private:
    constexpr inline static const char* memoryName = "CircuitSandbox_Clipboardv1";

    struct Clipboard {
        ext::autoremove_shared_memory memory;
        uint32_t id;
        std::string buffer;
        std::optional<UniqueTexture> thumbnail;
        template <typename OpenMode>
        Clipboard(const char* clipboardMemoryName, uint32_t size, uint32_t id, OpenMode open_mode = boost::interprocess::open_or_create, boost::interprocess::mode_t mode = boost::interprocess::read_write) : memory(clipboardMemoryName, size, open_mode, mode), id(id) {}
        Clipboard() noexcept {}
    };

    std::optional<ext::autoremove_shared_memory> indexTableMemory;
    std::array<std::optional<Clipboard>, NumClipboards> clipboards;

    SDL_Renderer* renderer;

    std::string getNameFromId(uint32_t id) {
        return std::string(memoryName) + '_' + std::to_string(id);
    }

    /**
     * Load a clipboard from the shared memory.
     * Returns true if a new clipboard was loaded.
     */
    bool reload(uint32_t index) {
        assert(index >= 0 && index < NumClipboards);
        if (indexTableMemory) {
            TableEntry* table = static_cast<TableEntry*>(indexTableMemory->address());
            auto[id, size] = table[index].load();
            if (!id) {
                bool ret = clipboards[index] != std::nullopt;
                clipboards[index] = std::nullopt;
                return ret;
            }
            else if (!clipboards[index] || clipboards[index]->id != id) {
                // replace outdated clipboard
                try {
                    auto memName = getNameFromId(id);
                    auto& clipboard = clipboards[index].emplace(memName.c_str(), size, id, boost::interprocess::open_only, boost::interprocess::read_only);
                    const char* memBuffer = static_cast<const char*>(clipboard.memory.address());
                    clipboard.buffer = std::string(memBuffer, size);
                    return true;
                }
                catch (std::exception& ex) {
                    // perhaps the memory doesn't exist, or has the wrong size, or something else
                    std::cout << "Cannot load clipboard " << index << " from shared memory: " << ex.what() << std::endl;
                    // set the memory to zero (since we can't read it)
                    table[index].store(0, 0);
                    bool ret = clipboards[index] != std::nullopt;
                    clipboards[index] = std::nullopt;
                    return ret;
                }
            }
            else {
                return false;
            }
        }
        return false;
    }

    /**
     * Generate the thumbnail to be used in the clipboard action interface.
     * Uses data from `buffer` only.
     */
    void generateThumbnail(Clipboard& clipboard) {
        assert(!clipboard.thumbnail);
        CanvasState state;
        std::istringstream stream(clipboard.buffer);
        if (state.loadSave(stream) == CanvasState::ReadResult::OK) {
            SDL_Surface* surface = SDL_CreateRGBSurface(0, state.width(), state.height(), 32, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0);
            state.fillSurface(reinterpret_cast<uint32_t*>(surface->pixels));
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
            clipboard.thumbnail.emplace(texture);
        }
    }

public:
    ClipboardStore() {
        try {
            indexTableMemory.emplace(memoryName, NumClipboards * sizeof(TableEntry));
        }
        catch (std::exception& ex) {
            // table cannot be created for some reason. Too bad, clipboards won't be shared.
            std::cout << "Shared clipboard memory failed to initialize: " << ex.what() << std::endl;
        }
    }

    void setRenderer(SDL_Renderer* renderer) {
        this->renderer = renderer;
    }

    /**
     * Read from a clipboard.
     */
    CanvasState read(uint32_t index) {
        reload(index);
        CanvasState state;
        if (clipboards[index]) {
            std::istringstream stream(clipboards[index]->buffer);
            state.loadSave(stream);
        }
        return state;
    }

    /**
     * Write to a clipboard.
     */
    void write(uint32_t index, const CanvasState& state) {
        std::ostringstream stream;
        if (state.writeSave(stream) == CanvasState::WriteResult::OK) {
            std::string buffer = stream.str();
            if (std::numeric_limits<uint32_t>::max() < buffer.size()) {
                // cannot write clipboard that is too big (this shouldn't happen in practice... a 4GB file?)
                return;
            }
            auto buffer_size = static_cast<uint32_t>(buffer.size());
            uint32_t id = 1; // zero is reserved to represent 'no clipboard'.
            static constexpr uint32_t ID_MAX = 4096;
            for (; id < ID_MAX; ++id) {
                try {
                    auto memName = getNameFromId(id);
                    auto& clipboard = clipboards[index].emplace(memName.c_str(), buffer_size, id, boost::interprocess::create_only);
                    std::copy(buffer.begin(), buffer.end(), static_cast<char*>(clipboard.memory.address()));
                    clipboard.buffer = std::move(buffer);
                    break;
                }
                catch (std::exception& ex) {
                    if (id + 1 == ID_MAX) {
                        std::cout << "Cannot save clipboard " << index << " to shared memory: " << ex.what() << std::endl;
                    }
                    continue;
                }
            }
            if (id == ID_MAX || !indexTableMemory) {
                // no shared memory, so we just save locally.
                auto& clipboard = clipboards[index].emplace();
                clipboard.buffer = std::move(buffer);
            }
            else {
                // add to the table
                TableEntry* table = static_cast<TableEntry*>(indexTableMemory->address());
                table[index].store(id, buffer_size);
            }
        }
    }

    /**
     * Get the thumbnail for the given clipboard (might be nullptr!).
     */
    SDL_Texture* getThumbnail(uint32_t index) {
        reload(index);
        if (clipboards[index]) {
            if (!clipboards[index]->thumbnail) {
                generateThumbnail(*clipboards[index]);
            }
            return (*(clipboards[index]->thumbnail)).get(); // could be nullptr (if clipboard file is corrupted), but okay
        }
        return nullptr;
    }
};
