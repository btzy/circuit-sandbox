#include <variant>

#include "gamestate.hpp"

// this lets us combine lambdas into visitors (not sure if this is the right file to put it)
template<class... Ts> struct visitor : Ts... { using Ts::operator()...; };
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;

void GameState::fillSurface(uint32_t* pixelBuffer, int32_t left, int32_t top, int32_t width, int32_t height) const {
    for (int32_t y = top; y != top + height; ++y) {
        for (int32_t x = left; x != left + width; ++x) {
            uint32_t color = 0;

            // check if the requested pixel inside the buffer
            if (x >= 0 && x < dataMatrix.width() && y >= 0 && y < dataMatrix.height()) {
                std::visit(visitor{
                    [&color](std::monostate) {},
                    [&color](auto element) {
                        // 'Element' is the type of tool (e.g. ConductiveWire)
                        using Element = typename decltype(element);

                        color = Element::displayColor.r | (Element::displayColor.g << 8) | (Element::displayColor.b << 16);
                    },
                }, dataMatrix[{x, y}]);
            }
            *pixelBuffer++ = color;
        }
    }
}
