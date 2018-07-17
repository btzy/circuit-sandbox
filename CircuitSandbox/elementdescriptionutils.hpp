#pragma once

#include <string>
#include <variant>
#include <type_traits>
#include "elements.hpp"
#include "fileinputcommunicator.hpp"
#include "canvasstate.hpp"

using namespace std::literals;

/**
 * Helper classes and functions for the element descriptions in the button bar.
 */

namespace Description {

    struct FileInputCommunicatorDescriptionElement : public LogicLevelElementBase<FileInputCommunicatorDescriptionElement>, public Element {
        std::string inputFilePath;

        FileInputCommunicatorDescriptionElement(const FileInputCommunicatorElement& el) : LogicLevelElementBase<FileInputCommunicatorDescriptionElement>(el.logicLevel, el.startingLogicLevel) {
            inputFilePath = el.communicator ? el.communicator->getFile() : ""s;
        }
        FileInputCommunicatorDescriptionElement(const FileInputCommunicatorDescriptionElement&) = default;
        FileInputCommunicatorDescriptionElement& operator=(const FileInputCommunicatorDescriptionElement&) = default;
        FileInputCommunicatorDescriptionElement(FileInputCommunicatorDescriptionElement&&) = default;
        FileInputCommunicatorDescriptionElement& operator=(FileInputCommunicatorDescriptionElement&&) = default;

        template <typename Callback>
        void setDescription(Callback&& callback) const {
            bool displayLogicLevel = this->getLogicLevel();
            if (!inputFilePath.empty()) {
                std::string str = " ["s + getFileName(inputFilePath.c_str()) + "]";
                std::forward<Callback>(callback)(
                    FileInputCommunicatorElement::displayName,
                    FileInputCommunicatorElement::displayColor,
                    displayLogicLevel ? " [HIGH]" : " [LOW]",
                    displayLogicLevel ? SDL_Color{ 0x66, 0xFF, 0x66, 0xFF } : SDL_Color{ 0x66, 0x66, 0x66, 0xFF },
                    str.c_str(),
                    SDL_Color{ 0xFF, 0xFF, 0xFF, 0xFF });
            }
            else {
                std::forward<Callback>(callback)(
                    FileInputCommunicatorElement::displayName,
                    FileInputCommunicatorElement::displayColor,
                    displayLogicLevel ? " [HIGH]" : " [LOW]",
                    displayLogicLevel ? SDL_Color{ 0x66, 0xFF, 0x66, 0xFF } : SDL_Color{ 0x66, 0x66, 0x66, 0xFF },
                    " [No file]",
                    SDL_Color{ 0x66, 0x66, 0x66, 0xFF });
            }
        }

        bool operator==(const FileInputCommunicatorDescriptionElement& other) const noexcept {
            return static_cast<const LogicLevelElementBase<FileInputCommunicatorDescriptionElement>&>(*this) == other && inputFilePath == other.inputFilePath;
        }

        bool operator!=(const FileInputCommunicatorDescriptionElement& other) const noexcept {
            return !(*this == other);
        }
    };

    template <typename T>
    struct ElementType {
        using type = T;
    };

    template <>
    struct ElementType<FileInputCommunicatorElement> {
        using type = FileInputCommunicatorDescriptionElement;
    };

    template <typename T>
    using ElementType_t = typename ElementType<T>::type;

    using ElementVariant_t = CanvasState::element_tags_t::transform<ElementType_t>::instantiate<std::variant>;

    inline ElementVariant_t fromElementVariant(const CanvasState::element_variant_t& elementVariant) {
        return std::visit([](const auto& element) {
            return ElementVariant_t(element);
        }, elementVariant);
    }

}
