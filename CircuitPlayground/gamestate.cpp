#include "gamestate.hpp"

void GameState::selectRect(SDL_Rect selectionRect) {
    extensions::point translation{ selectionRect.x, selectionRect.y };
    int32_t y_min = std::max(selectionRect.y, 0);
    int32_t x_min = std::max(selectionRect.x, 0);
    int32_t y_max = std::min(selectionRect.y + selectionRect.h, dataMatrix.height());
    int32_t x_max = std::min(selectionRect.x + selectionRect.w, dataMatrix.width());
    for (int32_t y = y_min; y < y_max; ++y) {
        for (int32_t x = x_min; x < x_max; ++x) {
            if (!std::holds_alternative<std::monostate>(dataMatrix[{x, y}])) {
                translation = prepareMatrixForAddition(selection, x - selectionX, y - selectionY);
                selectionX -= translation.x;
                selectionY -= translation.y;
                selection[{x - selectionX, y - selectionY}] = dataMatrix[{x, y}];
            }
        }
    }

    for (int32_t y = 0; y < selection.height(); ++y) {
        for (int32_t x = 0; x < selection.width(); ++x) {
            if (!std::holds_alternative<std::monostate>(selection[{x, y}])) {
                printf("1 ");
            } else {
                printf("0 ");
            }
        }
        printf("\n");
    }
}

bool GameState::pointInSelection(int32_t x, int32_t y) {
    int32_t select_x = x - selectionX;
    int32_t select_y = y - selectionY;
    if (select_x >= 0 && select_x < selection.width() && select_y >= 0 && select_y < selection.height()) {
        return !std::holds_alternative<std::monostate>(selection[{select_x, select_y}]);
    }
    return false;
}
