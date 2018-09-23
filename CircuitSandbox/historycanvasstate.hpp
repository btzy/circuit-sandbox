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

#include <utility>
#include <memory>
#include <variant>
#include "elements.hpp"
#include "canvasstate.hpp"
#include "heap_matrix.hpp"

/**
 * Canvas state that is for storing in the history manager.
 * The communicator elements only store weak references to the shared communicators
 */

class HistoryCanvasState {
private:
    template <typename T, typename TCommunicator>
    struct CommunicatorHistoryElementBase : public SignalReceivingElementBase<T>, public LogicLevelElementBase<T> {
        CommunicatorHistoryElementBase(const T& other) noexcept : LogicLevelElementBase<T>(other.logicLevel, other.startingLogicLevel), transmitState(other.transmitState), communicator(other.communicator) {}
        bool transmitState;
        std::weak_ptr<TCommunicator> communicator;
        operator T() const noexcept {
            auto tmp = T(this->logicLevel, this->startingLogicLevel, this->transmitState);
            tmp.communicator = communicator.lock();
            return tmp;
        }
    };

    template <typename T>
    struct ElementType {
        using type = T;
    };

    template <typename T, typename TCommunicator>
    struct ElementType<CommunicatorElementBase<T, TCommunicator>> {
        using type = CommunicatorHistoryElementBase<T, TCommunicator>;
    };

    template <typename T>
    using ElementType_t = typename ElementType<T>::type;

public:
    using element_variant_t = CanvasState::element_tags_t::transform<ElementType_t>::instantiate<std::variant>;

private:
    using matrix_t = ext::heap_matrix<element_variant_t>;

    matrix_t dataMatrix;
public:
    HistoryCanvasState() = default;
    HistoryCanvasState(const CanvasState& state) {
        dataMatrix = state.dataMatrix;
    }
    HistoryCanvasState(CanvasState&& state) {
        dataMatrix = std::move(state.dataMatrix);
    }
    operator CanvasState() const & {
        CanvasState ret;
        ret.dataMatrix = dataMatrix;
        return ret;
    }
    operator CanvasState() && {
        CanvasState ret;
        ret.dataMatrix = std::move(dataMatrix);
        return ret;
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
};
