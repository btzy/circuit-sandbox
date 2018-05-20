#include <SDL.h>
#include <variant>

#include "gamestate.hpp"

// this lets us combine lambdas into visitors (not sure if this is the right file to put it)
template<class... Ts> struct visitor : Ts... { using Ts::operator()...; };
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;

void GameState::fillSurface(SDL_Surface* surface) const {
    // TODO: pass a rect containing the visible region and skip elements that lie outside
    for (int x=0; x < dataMatrix.width(); x++) {
        for (int y=0; y < dataMatrix.height(); y++) {
            std::visit(visitor {
                [](std::monostate) {},
                [this, surface, x, y](auto element) {
                    Uint32 color = SDL_MapRGBA(surface->format, element.displayColor.r, element.displayColor.g, element.displayColor.b, element.displayColor.a);
                    // draw a single pixel on the surface (TODO: find a better way to do this)
                    const SDL_Rect destRect{x, y, 1, 1};
                    SDL_FillRect(surface, &destRect, color);
                },
            }, dataMatrix[{x,y}]);
        }
    }
}
