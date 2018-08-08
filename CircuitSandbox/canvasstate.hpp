#pragma once

/**
* Represents the state of the saveable game data.
* All the game state that needs to be saved to disk should reside in this class.
* Exposes functions for resizing and editing the canvas
*/

#include <utility> // for std::pair
#include <variant> // for std::variant
#include <type_traits>
#include <stack>
#include <algorithm> // for std::min and std::max
#include <cstdint> // for int32_t and uint32_t
#include <limits>
#include <istream>
#include <ostream>
#include <boost/endian/conversion.hpp>
#include <cassert>

#include "heap_matrix.hpp"
#include "elements.hpp"
#include "point.hpp"
#include "visitor.hpp"
#include "tag_tuple.hpp"
#include "expandable_matrix.hpp"

class CanvasState {

public:
    // the possible elements that a pixel can represent
    // std::monostate is a 'default' state, which represents an empty pixel
    using element_tags_t = ext::tag_tuple<std::monostate, ConductiveWire, InsulatedWire, Signal, Source, PositiveRelay, NegativeRelay, AndGate, OrGate, NandGate, NorGate, ScreenCommunicatorElement, FileInputCommunicatorElement, FileOutputCommunicatorElement>;
    static_assert(element_tags_t::size <= (static_cast<size_t>(1) << (std::numeric_limits<uint8_t>::digits - 2)), "Number of elements cannot exceed number of available bits in file format.");

    using element_variant_t = element_tags_t::instantiate<std::variant>;

private:
    using matrix_t = ext::heap_matrix<element_variant_t>;

    matrix_t dataMatrix;

    friend class StateManager;
    friend class Simulator;
    friend class HistoryCanvasState;
    friend class SelectionAction;

    /**
     * Modifies the dataMatrix so that the x and y will be within the matrix.
     * Returns the translation that should be applied on {x,y}.
     */
    ext::point prepareDataMatrixForAddition(const ext::point& pt) {
        if (dataMatrix.empty()) {
            // special case for empty matrix
            dataMatrix = matrix_t(1, 1);
            return -pt;
        }

        if (dataMatrix.contains(pt)) { // check if inside the matrix
            return { 0, 0 }; // no preparation or translation needed
        }

        ext::point min_pt = ext::min(pt, { 0, 0 });
        ext::point max_pt = ext::min(pt + ext::point{ 1, 1 }, dataMatrix.size());
        ext::point translation = -min_pt;
        ext::point new_size = max_pt + translation;

        matrix_t new_matrix(new_size);
        ext::move_range(dataMatrix, new_matrix, 0, 0, translation.x, translation.y, dataMatrix.width(), dataMatrix.height());

        dataMatrix = std::move(new_matrix);

        return translation;
    }

    /**
     * Shrinks the dataMatrix to the minimal bounding rectangle.
     * As an optimization, will only shrink if {x,y} is along a border.
     * Returns the translation that should be applied on {x,y}.
     */
    ext::point shrinkDataMatrix(const ext::point& pt) {

        if (pt.x > 0 && pt.x + 1 < dataMatrix.width() && pt.y > 0 && pt.y + 1 < dataMatrix.height()) { // check if not on the border
            return { 0, 0 }; // no preparation or translation needed
        }

        return shrinkDataMatrix();
    }

public:

    /**
     * Shrink with no optimization.
     */
    ext::point shrinkDataMatrix() {

        // we simply iterate the whole matrix to get the min and max values
        int32_t x_min = std::numeric_limits<int32_t>::max();
        int32_t x_max = std::numeric_limits<int32_t>::min();
        int32_t y_min = std::numeric_limits<int32_t>::max();
        int32_t y_max = std::numeric_limits<int32_t>::min();

        // iterate height before width for cache-friendliness
        for (int32_t i = 0; i != dataMatrix.height(); ++i) {
            for (int32_t j = 0; j != dataMatrix.width(); ++j) {
                if (!std::holds_alternative<std::monostate>(dataMatrix[{j, i}])) {
                    x_min = std::min(x_min, j);
                    x_max = std::max(x_max, j);
                    y_min = std::min(y_min, i);
                    y_max = std::max(y_max, i);
                }
            }
        }

        if (x_min == 0 && x_max + 1 == dataMatrix.width() && y_min == 0 && y_max + 1 == dataMatrix.height()) {
            // no resizing needed
            return { 0, 0 };
        }

        if (x_min > x_max) {
            // if this happens, it means the game state is totally empty
            dataMatrix = matrix_t();

            // if the matrix is totally empty, returning a translation makes no sense
            // so we just return {0,0}
            return { 0, 0 };
        }

        int32_t new_width = x_max + 1 - x_min;
        int32_t new_height = y_max + 1 - y_min;

        matrix_t new_matrix(new_width, new_height);
        ext::move_range(dataMatrix, new_matrix, x_min, y_min, 0, 0, new_width, new_height);

        dataMatrix = std::move(new_matrix);

        return { -x_min, -y_min };
    }

    /**
     * Change the state of a pixel.
     * 'Element' should be one of the elements in 'element_variant_t', or std::monostate for the eraser
     * Returns a pair describing whether the canvas was actually modified, and the net translation change that the viewport should apply (such that the viewport will be at the 'same' position)
     */
    template <typename Element>
    std::pair<bool, ext::point> changePixelState(ext::point pt) {
        // note that this function performs linearly to the size of the matrix.  But since this is limited by how fast the user can click, it should be good enough

        if constexpr (std::is_same_v<std::monostate, Element>) {
            if (dataMatrix.contains(pt) && !std::holds_alternative<Element>(dataMatrix[pt])) {
                dataMatrix[pt] = Element{};

                // Element is std::monostate, so we have to see if we can shrink the matrix size
                // Inform the caller that the canvas was changed, and the translation required
                return { true, shrinkDataMatrix(pt) };

            }
            else {
                return { false, { 0, 0 } };
            }
        }
        else {
            // Element is not std::monostate, so we might need to expand the matrix size first
            ext::point translation = prepareDataMatrixForAddition(pt);
            pt += translation;

            if (!std::holds_alternative<Element>(dataMatrix[pt])) {
                dataMatrix[pt] = Element{};
                return { true, translation };
            }
            else {
                // note: if we reach here, `translation` is guaranteed to be {0,0} as well
                return { false, { 0, 0 } };
            }
        }
    }

    /**
     * indices is a pair of {x,y}
     * @pre indices must be within the bounds of width and height
     * For a version that does bounds checking and expansion/contraction of the underlying matrix, use changePixelState
     */
    element_variant_t& operator[](const ext::point& indices) noexcept {
        return dataMatrix[indices];
    }

    /**
     * indices is a pair of {x,y}
     * @pre indices must be within the bounds of width and height
     * For a version that does bounds checking and expansion/contraction of the underlying matrix, use changePixelState
     */
    const element_variant_t& operator[](const ext::point& indices) const noexcept {
        return dataMatrix[indices];
    }


    /**
     * returns true if the matrix is empty (i.e. has no width and height)
     */
    bool empty() const noexcept {
        return dataMatrix.empty();
    }

    /**
     * returns the width of the matrix
     */
    int32_t width() const noexcept {
        return dataMatrix.width();
    }

    /**
     * returns the height of the matrix
     */
    int32_t height() const noexcept {
        return dataMatrix.height();
    }

    /**
     * returns the size of the matrix
     */
    ext::point size() const noexcept {
        return dataMatrix.size();
    }

    /**
     * returns true if the point is within the bounds of the matrix
     */
    bool contains(const ext::point& pt) const noexcept {
        return dataMatrix.contains(pt);
    }

    /**
     * grow the underlying matrix to a larger size
     */
    ext::point extend(const ext::point& topLeft, const ext::point& bottomRight) {
        ext::point newTopLeft = empty() ? topLeft : min(topLeft, { 0, 0 });
        ext::point newBottomRight = empty() ? bottomRight : max(bottomRight, size());
        if (newTopLeft == ext::point{ 0, 0 } && newBottomRight == size()) {
            // optimization if we don't actually need to expand the current matrix
            return { 0, 0 };
        }
        else {
            ext::point newSize = newBottomRight - newTopLeft;
            matrix_t newMatrix(newSize);
            if (!empty()) ext::move_range(dataMatrix, newMatrix, 0, 0, -newTopLeft.x, -newTopLeft.y, width(), height());
            dataMatrix = std::move(newMatrix);
            return -newTopLeft;
        }
    }

    /**
     * Moves elements from an area of dataMatrix to a new CanvasState and returns them.
     * The area becomes empty on the current dataMatrix.
     * Returned matrix will be of the exact width and height; it will not be shrinked.
     * @pre Assumes that the parameters are within the bounds of this canvas state
     */
    CanvasState splice(int32_t x, int32_t y, int32_t width, int32_t height) {
        CanvasState newState;
        newState.dataMatrix = matrix_t(width, height);
        ext::swap_range(dataMatrix, newState.dataMatrix, x, y, 0, 0, width, height);
        return newState;
    }

    /**
     * Moves elements from an area of dataMatrix given by a mask to a new CanvasState and returns them.
     * The area becomes empty on the current dataMatrix.
     * Returned matrix will be of the exact width and height; it will not be shrinked.
     * @pre Assumes that the translated mask lies within the bounds of this canvas state
     */
    CanvasState spliceMask(const ext::point& offset, const ext::expandable_bool_matrix& mask) {
        CanvasState newState;
        newState.dataMatrix = matrix_t(mask.width(), mask.height());
        for (int32_t y = 0; y < mask.height(); ++y) {
            for (int32_t x = 0; x < mask.width(); ++x) {
                ext::point pt = offset + ext::point{ x, y };
                if (mask[pt]) {
                    std::swap(dataMatrix[pt], newState.dataMatrix[pt]);
                }
            }
        }
        return newState;
    }

    void flipHorizontal() {
        dataMatrix.flipHorizontal();
    }

    void flipVertical() {
        dataMatrix.flipVertical();
    }

    void rotateClockwise() {
        matrix_t newDataMatrix = matrix_t(dataMatrix.height(), dataMatrix.width());
        for (int32_t x = 0; x != dataMatrix.width(); ++x) {
            for (int32_t y = 0; y != dataMatrix.height(); ++y) {
                newDataMatrix[{dataMatrix.height()-y-1, x}] = dataMatrix[{x, y}];
            }
        }
        dataMatrix = std::move(newDataMatrix);
    }

    void rotateCounterClockwise() {
        matrix_t newDataMatrix = matrix_t(dataMatrix.height(), dataMatrix.width());
        for (int32_t x = 0; x != dataMatrix.width(); ++x) {
            for (int32_t y = 0; y != dataMatrix.height(); ++y) {
                newDataMatrix[{y, dataMatrix.width()-x-1}] = dataMatrix[{x, y}];
            }
        }
        dataMatrix = std::move(newDataMatrix);
    }

    /**
     * Merges a CanvasState with another, potentially modifying both of them.
     * Elements from the second CanvasState will be written over those from the first in the output.
     * Returns a matrix (not shrinked) and the translation required.
     * If first contains second totally, the implementation will not construct an additional CanvasState.
     * @pre Assumes that the parameters are within the bounds of this canvas state
     */
    static std::pair<CanvasState, ext::point> merge(CanvasState&& first, const ext::point& firstTrans, CanvasState&& second, const ext::point& secondTrans) {
        // special cases if we are merging with something empty
        if (first.empty()) return { std::move(second), -secondTrans };
        if (second.empty()) return { std::move(first), -firstTrans };

        ext::point newMin = min(secondTrans, firstTrans);
        ext::point newMax = max(secondTrans + second.size(), firstTrans + first.size());

        CanvasState newState;
        if (firstTrans == newMin && firstTrans + first.size() == newMax) {
            // optimization when `first` totally contains `second`; we can use the old matrix
            newState.dataMatrix = std::move(first.dataMatrix);
        }
        else {
            // create a new matrix
            newState.dataMatrix = matrix_t(newMax - newMin);
            // move first to newstate
            ext::move_range(first.dataMatrix, newState.dataMatrix, 0, 0, firstTrans.x - newMin.x, firstTrans.y - newMin.y, first.width(), first.height());
        }
        // move non-monostate elements from second to newstate
        for (int32_t y = 0; y < second.height(); ++y) {
            for (int32_t x = 0; x < second.width(); ++x) {
                ext::point pt{ x, y };
                if (!std::holds_alternative<std::monostate>(second.dataMatrix[pt])) {
                    newState[pt + secondTrans - newMin] = std::move(second[pt]);
                }
            }
        }

        return { std::move(newState), -newMin };
    }

    /**
     * Draw all elements onto a pixel buffer.
     * @pre Assumes that the pixel buffer has the same size as the datamatrix
     */
    void fillSurface(uint32_t* pixelBuffer) {
        for (int32_t y = 0; y < height(); ++y) {
            for (int32_t x = 0; x < width(); ++x) {
                SDL_Color computedColor{ 0, 0, 0, 0 };
                std::visit(visitor{
                    [](std::monostate) {},
                    [&computedColor](const auto& element) {
                        computedColor = element.template computeDisplayColor<false>();
                    },
                }, dataMatrix[{x, y}]);
                *pixelBuffer++ = computedColor.r | (computedColor.g << 8) | (computedColor.b << 16);
            }
        }
    }



    // Serialization functions
    enum class ReadResult : char {
        OK,
        OUTDATED, // file is saved in a newer format
        CORRUPTED, // file is corrupted
        IO_ERROR // file cannot be opened or read
    };

private:
    // determine if it is CORRUPTED or IO_ERROR, use immediately after std::istream::read
    inline static ReadResult resolveLoadError(std::istream& saveFile) {
        assert(!saveFile);
        return saveFile.eof() ? ReadResult::CORRUPTED : ReadResult::IO_ERROR;
    }

public:
    inline ReadResult loadSave(std::istream& saveFile) {
        // read the magic sequence
        char data[4];
        if (!saveFile.read(data, 4)) return resolveLoadError(saveFile);
        if (!std::equal(data, data + 4, CCSB_FILE_MAGIC)) return ReadResult::CORRUPTED;

        // read the version number
        int32_t version;
        if(!saveFile.read(reinterpret_cast<char*>(&version), sizeof version)) return resolveLoadError(saveFile);
        boost::endian::little_to_native_inplace(version);
        if (version != 0) return ReadResult::OUTDATED;

        // read the width and height
        int32_t matrixWidth, matrixHeight;
        saveFile.read(reinterpret_cast<char*>(&matrixWidth), sizeof matrixWidth);
        saveFile.read(reinterpret_cast<char*>(&matrixHeight), sizeof matrixHeight);
        if(!saveFile) return resolveLoadError(saveFile);
        boost::endian::little_to_native_inplace(matrixWidth);
        boost::endian::little_to_native_inplace(matrixHeight);
        if (matrixWidth < 0 || matrixHeight < 0 || static_cast<int64_t>(matrixWidth) * matrixHeight > std::numeric_limits<int32_t>::max()) return ReadResult::CORRUPTED;

        // create the matrix
        CanvasState::matrix_t canvasData(matrixWidth, matrixHeight);

        for (int32_t y = 0; y != canvasData.height(); ++y) {
            for (int32_t x = 0; x != canvasData.width(); ++x) {
                uint8_t elementData;
                static_assert(sizeof elementData == 1);
                if(!saveFile.read(reinterpret_cast<char*>(&elementData), 1)) return resolveLoadError(saveFile);
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

    enum class WriteResult : char {
        OK,
        IO_ERROR // file cannot be opened or written to
    };

    WriteResult writeSave(std::ostream& saveFile) const {

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
};
