#include <cassert>
#include "gamestate.hpp"


void GameState::selectRect(SDL_Rect selectionRect) {
    // make a copy of dataMatrix
    base = dataMatrix;

    extensions::point translation{ selectionRect.x, selectionRect.y };
    // restrict selectionRect to the area within base
    int32_t sy_min = std::max(selectionRect.y, 0);
    int32_t sx_min = std::max(selectionRect.x, 0);
    int32_t sy_max = std::min(selectionRect.y + selectionRect.h, base.height());
    int32_t sx_max = std::min(selectionRect.x + selectionRect.w, base.width());

    int32_t x_min = std::numeric_limits<int32_t>::max();
    int32_t x_max = std::numeric_limits<int32_t>::min();
    int32_t y_min = std::numeric_limits<int32_t>::max();
    int32_t y_max = std::numeric_limits<int32_t>::min();

    // determine bounds of selection
    for (int32_t y = sy_min; y < sy_max; ++y) {
        for (int32_t x = sx_min; x < sx_max; ++x) {
            if (!std::holds_alternative<std::monostate>(base[{x, y}])) {
                y_min = std::min(y, y_min);
                x_min = std::min(x, x_min);
                y_max = std::max(y, y_max);
                x_max = std::max(x, x_max);
            }
        }
    }

    // no elements in the selection rect
    if (x_min > x_max) return;
    hasSelection = true;

    int32_t new_width = x_max - x_min + 1;
    int32_t new_height = y_max - y_min + 1;
    selection = matrix_t(new_width, new_height);
    selectionX = x_min;
    selectionY = y_min;

    // move elements from base to selection
    for (int32_t y = y_min; y <= y_max; ++y) {
        for (int32_t x = x_min; x <= x_max; ++x) {
            if (!std::holds_alternative<std::monostate>(base[{x, y}])) {
                selection[{x - selectionX, y - selectionY}] = base[{x, y}];
                base[{x, y}] = std::monostate{};
            }
        }
    }

    // shrink base after removing all selected elements
    translation = shrinkMatrix(base);
    baseX = -translation.x;
    baseY = -translation.y;

    mergeSelection();
}

bool GameState::pointInSelection(int32_t x, int32_t y) {
    x -= selectionX;
    y -= selectionY;
    if (x >= 0 && x < selection.width() && y >= 0 && y < selection.height()) {
        return !std::holds_alternative<std::monostate>(selection[{x, y}]);
    }
    return false;
}

void GameState::clearSelection() {
    mergeSelection();
    selection = matrix_t();
    selectionX = 0;
    selectionY = 0;
    baseX = 0;
    baseY = 0;
    hasSelection = false;
}

void GameState::mergeSelection() {
    if (!hasSelection) return;
    assert(selectionX == 0 || baseX == 0);
    assert(selectionY == 0 || baseY == 0);

    int32_t new_width = std::max(selectionX + selection.width(), baseX + base.width());
    int32_t new_height = std::max(selectionY + selection.height(), baseY + base.height());

    dataMatrix = matrix_t(new_width, new_height);

    // copy base to dataMatrix
    extensions::copy_range(base, dataMatrix, 0, 0, baseX, baseY, base.width(), base.height());

    // copy non-monostate elements from selection to dataMatrix
    for (int32_t y = 0; y < selection.height(); ++y) {
        for (int32_t x = 0; x < selection.width(); ++x) {
            if (!std::holds_alternative<std::monostate>(selection[{x, y}])) {
                dataMatrix[{x + selectionX, y + selectionY}] = selection[{x, y}];
            }
        }
    }
}

extensions::point GameState::moveSelection(int32_t dx, int32_t dy) {
    extensions::point translation;

    // if base is empty, we only need to move the viewport
    if (base.empty()) {
        // this part is optional
        // in any case, the viewport will 'jump' since this gamestate will not be saved (since nothing really changed)
        deltaTrans.x -= dx;
        deltaTrans.y -= dy;

        return { -dx, -dy };
    }

    selectionX += dx;
    selectionY += dy;

    // update selectionX/Y and baseX/Y such that selection and base are flushed towards the top left corner of datamatrix
    translation.x = -std::min(selectionX, baseX);
    translation.y = -std::min(selectionY, baseY);
    selectionX += translation.x;
    selectionY += translation.y;
    baseX += translation.x;
    baseY += translation.y;

    mergeSelection();

    deltaTrans.x += translation.x;
    deltaTrans.y += translation.y;
    return translation;
}

extensions::point GameState::deleteSelection() {
    // if selection is empty, there is nothing to delete
    if (selection.empty()) {
        return { 0, 0 };
    }

    // delete the selection
    selection = matrix_t();
    selectionX = 0;
    selectionY = 0;

    // merge base back to gamestate
    extensions::point translation{ -baseX, -baseY };
    baseX = 0;
    baseY = 0;
    mergeSelection();

    deltaTrans.x += translation.x;
    deltaTrans.y += translation.y;
    return translation;
}

void GameState::copySelectionToClipboard() {
    clipboard = selection;
}

extensions::point GameState::pasteSelection(int32_t x, int32_t y) {
    selection = clipboard;
    if (selection.empty()) return { 0, 0 };
    hasSelection = true;
    selectionX = x;
    selectionY = y;

    extensions::point translation;
    translation.x = -std::min(selectionX, baseX);
    translation.y = -std::min(selectionY, baseY);
    selectionX += translation.x;
    selectionY += translation.y;
    baseX += translation.x;
    baseY += translation.y;

    base = dataMatrix;
    mergeSelection();

    deltaTrans.x += translation.x;
    deltaTrans.y += translation.y;
    return translation;
}
