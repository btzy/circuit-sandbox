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

#include <string>
#include <variant>
#include <type_traits>
#include "elements.hpp"
#include "fileinputcommunicator.hpp"
#include "fileoutputcommunicator.hpp"
#include "canvasstate.hpp"

using namespace std::literals;

/**
 * Helper classes and functions for the element descriptions in the button bar.
 */

namespace Description {

    template <typename T>
    struct CommunicatorDescriptionElementBase : public LogicLevelElementBase<T>, public Element {
        template <typename T2, typename TCommunicator>
        CommunicatorDescriptionElementBase(const CommunicatorElementBase<T2, TCommunicator>& el) noexcept : LogicLevelElementBase<T>(el.logicLevel, el.startingLogicLevel) {}
        
        CommunicatorDescriptionElementBase(const CommunicatorDescriptionElementBase&) = default;
        CommunicatorDescriptionElementBase& operator=(const CommunicatorDescriptionElementBase&) = default;
        CommunicatorDescriptionElementBase(CommunicatorDescriptionElementBase&&) = default;
        CommunicatorDescriptionElementBase& operator=(CommunicatorDescriptionElementBase&&) = default;

        bool operator==(const T& other) const noexcept {
            return static_cast<const LogicLevelElementBase<T>&>(*this) == other;
        }

        bool operator!=(const T& other) const noexcept {
            return !(*this == other);
        }

        using LogicLevelElementBase<T>::setDescription;
    };

    struct ScreenCommunicatorDescriptionElement : public CommunicatorDescriptionElementBase<ScreenCommunicatorDescriptionElement> {
        ScreenCommunicatorDescriptionElement(const ScreenCommunicatorElement& el) noexcept : CommunicatorDescriptionElementBase<ScreenCommunicatorDescriptionElement>(el) {}
        ScreenCommunicatorDescriptionElement(const ScreenCommunicatorDescriptionElement&) = default;
        ScreenCommunicatorDescriptionElement& operator=(const ScreenCommunicatorDescriptionElement&) = default;
        ScreenCommunicatorDescriptionElement(ScreenCommunicatorDescriptionElement&&) = default;
        ScreenCommunicatorDescriptionElement& operator=(ScreenCommunicatorDescriptionElement&&) = default;
        
        static constexpr auto displayColor = ScreenCommunicatorElement::displayColor;
        static constexpr auto displayName = ScreenCommunicatorElement::displayName;
    };

    template <typename TElement>
    struct FileCommunicatorDescriptionElement : public CommunicatorDescriptionElementBase<FileCommunicatorDescriptionElement<TElement>> {
        std::string filePath;

        FileCommunicatorDescriptionElement(const TElement& el) : CommunicatorDescriptionElementBase<FileCommunicatorDescriptionElement<TElement>>(el) {
            filePath = el.communicator ? el.communicator->getFile() : ""s;
        }
        FileCommunicatorDescriptionElement(const FileCommunicatorDescriptionElement<TElement>&) = default;
        FileCommunicatorDescriptionElement& operator=(const FileCommunicatorDescriptionElement<TElement>&) = default;
        FileCommunicatorDescriptionElement(FileCommunicatorDescriptionElement<TElement>&&) = default;
        FileCommunicatorDescriptionElement& operator=(FileCommunicatorDescriptionElement<TElement>&&) = default;
        
        static constexpr auto displayColor = TElement::displayColor;
        static constexpr auto displayName = TElement::displayName;

        template <typename Callback>
        void setDescription(Callback&& callback) const {
            bool displayLogicLevel = this->getLogicLevel();
            if (!filePath.empty()) {
                std::string str = " ["s + getFileName(filePath.c_str()) + "]";
                std::forward<Callback>(callback)(
                    TElement::displayName,
                    TElement::displayColor,
                    displayLogicLevel ? " [HIGH]" : " [LOW]",
                    displayLogicLevel ? SDL_Color{ 0x66, 0xFF, 0x66, 0xFF } : SDL_Color{ 0x66, 0x66, 0x66, 0xFF },
                    str.c_str(),
                    SDL_Color{ 0xFF, 0xFF, 0xFF, 0xFF });
            }
            else {
                std::forward<Callback>(callback)(
                    TElement::displayName,
                    TElement::displayColor,
                    displayLogicLevel ? " [HIGH]" : " [LOW]",
                    displayLogicLevel ? SDL_Color{ 0x66, 0xFF, 0x66, 0xFF } : SDL_Color{ 0x66, 0x66, 0x66, 0xFF },
                    " [No file]",
                    SDL_Color{ 0x66, 0x66, 0x66, 0xFF });
            }
        }

        bool operator==(const FileCommunicatorDescriptionElement<TElement>& other) const noexcept {
            return static_cast<const CommunicatorDescriptionElementBase<FileCommunicatorDescriptionElement<TElement>>&>(*this) == other && filePath == other.filePath;
        }

        bool operator!=(const FileCommunicatorDescriptionElement<TElement>& other) const noexcept {
            return !(*this == other);
        }
    };

    template <typename T>
    struct ElementType {
        using type = T;
    };

    template <>
    struct ElementType<ScreenCommunicatorElement> {
        using type = ScreenCommunicatorDescriptionElement;
    };
    template <>
    struct ElementType<FileInputCommunicatorElement> {
        using type = FileCommunicatorDescriptionElement<FileInputCommunicatorElement>;
    };
    template <>
    struct ElementType<FileOutputCommunicatorElement> {
        using type = FileCommunicatorDescriptionElement<FileOutputCommunicatorElement>;
    };

    template <typename T>
    using ElementType_t = typename ElementType<T>::type;

    using ElementVariant_t = CanvasState::element_tags_t::transform<ElementType_t>::instantiate<std::variant>;

    inline ElementVariant_t fromElementVariant(const CanvasState::element_variant_t& elementVariant) {
        return std::visit([](const auto& element) {
            return ElementVariant_t(std::in_place_type_t<ElementType_t<std::decay_t<decltype(element)>>>(), element);
        }, elementVariant);
    }

}
