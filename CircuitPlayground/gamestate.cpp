#include <cassert>
#include "gamestate.hpp"


void GameState::selectRect(SDL_Rect selectionRect) {
    // make a copy of dataMatrix
    base = dataMatrix;

    extensions::point translation{ selectionRect.x, selectionRect.y };
    // restrict selectionRect to the area within base
    int32_t y_min = std::max(selectionRect.y, 0);
    int32_t x_min = std::max(selectionRect.x, 0);
    int32_t y_max = std::min(selectionRect.y + selectionRect.h, base.height());
    int32_t x_max = std::min(selectionRect.x + selectionRect.w, base.width());

    // move elements from base to selection
    for (int32_t y = y_min; y < y_max; ++y) {
        for (int32_t x = x_min; x < x_max; ++x) {
            if (!std::holds_alternative<std::monostate>(base[{x, y}])) {
                translation = prepareMatrixForAddition(selection, x - selectionX, y - selectionY);
                selectionX -= translation.x;
                selectionY -= translation.y;
                selection[{x - selectionX, y - selectionY}] = base[{x, y}];
                base[{x, y}] = std::monostate{};
            }
        }
    }

    if (selection.empty()) return;
    hasSelection = true;

    // only shrink after removing all selected elements
    translation = shrinkMatrix(base);
    baseX = -translation.x;
    baseY = -translation.y;

    mergeSelection();
}

bool GameState::pointInSelection(int32_t x, int32_t y) {
    int32_t select_x = x - selectionX;
    int32_t select_y = y - selectionY;
    if (select_x >= 0 && select_x < selection.width() && select_y >= 0 && select_y < selection.height()) {
        return !std::holds_alternative<std::monostate>(selection[{select_x, select_y}]);
    }
    return false;
}

void GameState::clearSelection() {
    mergeSelection();
    selection = matrix_t();
    selectionX = 0;
    selectionY = 0;
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

    // update selectionX/Y and baseX/Y such that selection and base are flushed towards the top left corner of datamatrix
    selectionX += dx;
    translation.x = -std::min(selectionX, baseX);
    selectionX += translation.x;
    baseX += translation.x;

    selectionY += dy;
    translation.y = -std::min(selectionY, baseY);
    selectionY += translation.y;
    baseY += translation.y;

    mergeSelection();

    deltaTrans.x += translation.x;
    deltaTrans.y += translation.y;
    return translation;
}
