#include "canvasstate.hpp"

using ReadResult = CanvasState::ReadResult;
using WriteResult = CanvasState::WriteResult;

// determine if it is CORRUPTED or IO_ERROR, use immediately after std::istream::read
static ReadResult resolveLoadError(std::istream& saveFile) {
    assert(!saveFile);
    return saveFile.eof() ? ReadResult::CORRUPTED : ReadResult::IO_ERROR;
}

ReadResult CanvasState::loadSave(std::istream& saveFile) {
    // read the magic sequence
    char data[4];
    if (!saveFile.read(data, 4)) return resolveLoadError(saveFile);
    if (!std::equal(data, data + 4, CCSB_FILE_MAGIC)) return ReadResult::CORRUPTED;

    // read the version number
    int32_t version;
    if (!saveFile.read(reinterpret_cast<char*>(&version), sizeof version)) return resolveLoadError(saveFile);
    boost::endian::little_to_native_inplace(version);
    if (version != 0) return ReadResult::OUTDATED;

    // read the width and height
    int32_t matrixWidth, matrixHeight;
    saveFile.read(reinterpret_cast<char*>(&matrixWidth), sizeof matrixWidth);
    saveFile.read(reinterpret_cast<char*>(&matrixHeight), sizeof matrixHeight);
    if (!saveFile) return resolveLoadError(saveFile);
    boost::endian::little_to_native_inplace(matrixWidth);
    boost::endian::little_to_native_inplace(matrixHeight);
    if (matrixWidth < 0 || matrixHeight < 0 || static_cast<int64_t>(matrixWidth) * matrixHeight > std::numeric_limits<int32_t>::max()) return ReadResult::CORRUPTED;

    // create the matrix
    CanvasState::matrix_t canvasData(matrixWidth, matrixHeight);

    for (int32_t y = 0; y != canvasData.height(); ++y) {
        for (int32_t x = 0; x != canvasData.width(); ++x) {
            uint8_t elementData;
            static_assert(sizeof elementData == 1);
            if (!saveFile.read(reinterpret_cast<char*>(&elementData), 1)) return resolveLoadError(saveFile);
            size_t element_index = elementData >> 2;
            bool logicLevel = elementData & 0b10;
            bool defaultLogicLevel = elementData & 0b01;

            CanvasState::element_variant_t& element = canvasData[{x, y}];
            if (!CanvasState::element_tags_t::get(element_index, [&](const auto element_tag) {
                using ElementType = typename decltype(element_tag)::type;
                if constexpr (std::is_base_of_v<CommunicatorElement, ElementType>) {
                    // TODO: store communicator transmit states in the save file?
                    element.emplace<ElementType>(logicLevel, defaultLogicLevel, false);
                }
                else if constexpr (std::is_base_of_v<LogicLevelElement, ElementType>) {
                    element.emplace<ElementType>(logicLevel, defaultLogicLevel);
                }
                else if constexpr (std::is_base_of_v<Relay, ElementType>) {
                    // TODO: store relay states in the save file!
                    element.emplace<ElementType>(false, false, logicLevel, defaultLogicLevel);
                }
                else if constexpr (std::is_base_of_v<Element, ElementType>) {
                    element.emplace<ElementType>();
                }
                return true;
            }, false)) {
                return ReadResult::OUTDATED; // maybe the new save format contains more elements?
            }
        }
    }

    // only overwriting state.dataMatrix here ensures it is only modified if we return ReadResult::OK.
    dataMatrix = std::move(canvasData);
    return ReadResult::OK;
}

WriteResult CanvasState::writeSave(std::ostream& saveFile) const {

    // write the magic sequence
    saveFile.write(CCSB_FILE_MAGIC, 4);

    // write the version number
    int32_t version = 0;
    boost::endian::native_to_little_inplace(version);
    saveFile.write(reinterpret_cast<char*>(&version), sizeof version);

    // write the width and height
    int32_t matrixWidth = width();
    int32_t matrixHeight = height();
    boost::endian::native_to_little_inplace(matrixWidth);
    boost::endian::native_to_little_inplace(matrixHeight);
    saveFile.write(reinterpret_cast<char*>(&matrixWidth), sizeof matrixWidth);
    saveFile.write(reinterpret_cast<char*>(&matrixHeight), sizeof matrixHeight);

    // write the matrix
    for (int32_t y = 0; y != height(); ++y) {
        for (int32_t x = 0; x != width(); ++x) {
            CanvasState::element_variant_t element = dataMatrix[{x, y}];
            size_t element_index = element.index();
            bool logicLevel = false;
            bool defaultLogicLevel = false;

            std::visit([&](const auto& element) {
                using ElementType = typename std::decay_t<decltype(element)>;
                if constexpr (std::is_base_of_v<CommunicatorElement, ElementType>) {
                    // TODO: store communicator transmit states in the save file?
                    logicLevel = element.logicLevel;
                    defaultLogicLevel = element.startingLogicLevel;
                }
                else if constexpr (std::is_base_of_v<Relay, ElementType>) {
                    // TODO: store relay states in the save file!
                    logicLevel = element.conductiveState;
                    defaultLogicLevel = element.startingConductiveState;
                }
                else if constexpr (std::is_base_of_v<RenderLogicLevelElement, ElementType>) {
                    // even though ElementType might not be a LogicLevelElement (i.e. with useful logic levels), we save the logic level for compatibility with saves in the v0.2 format.
                    // but when loaded, those non-useful logic levels will be ignored.
                    logicLevel = element.logicLevel;
                    defaultLogicLevel = element.startingLogicLevel;
                }
            }, element);

            uint8_t elementData = static_cast<uint8_t>((element_index << 2) | (logicLevel << 1) | defaultLogicLevel);
            saveFile.write(reinterpret_cast<char*>(&elementData), 1);
        }
    }

    // flush the stream, so failbit will be set if the stream cannot be written to.
    saveFile.flush();
    return saveFile ? WriteResult::OK : WriteResult::IO_ERROR;
}
