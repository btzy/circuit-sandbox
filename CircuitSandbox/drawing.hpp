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

#include <array>
#include <cassert>

#include <SDL.h>

namespace ext {

    // Surface functions

    /**
     * Draw a border on a surface.
     * The specified rectangle includes the border thickness.
     */
    inline void drawBorder(SDL_Surface* surface, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t thickness, SDL_Color color) {
        assert(surface);
        assert(x1 + thickness <= x2);
        assert(y1 + thickness <= y2);
        std::array<SDL_Rect, 4> rects{ {
            { x1, y1, x2 - x1, thickness },
            { x1, y1, thickness, y2 - y1 },
            { x1, y2 - thickness, x2 - x1, thickness },
            { x2 - thickness, y1, thickness, y2 - y1 }
            } };
        SDL_FillRects(surface, rects.data(), 4, SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a));
    }

    /**
    * Draw a border on a surface.
    * The specified rectangle includes the border thickness.
    */
    inline void drawBorder(SDL_Surface* surface, const SDL_Rect& rect, int32_t thickness, SDL_Color color) {
        drawBorder(surface, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, thickness, color);
    }

    // Render functions

    inline void renderDrawDashedHorizontalLine(SDL_Renderer* renderer, int32_t x1, int32_t x2, int32_t y) {
        const int32_t segmentLength = 4;
        int32_t width = x2 - x1 + 1;
        int32_t segments = width / segmentLength;
        int32_t excess = width % segmentLength;
        if (segments > 0 && segments%2 == 0) {
            excess += segmentLength;
            segments--;
        }
        int32_t headLength = excess/2;
        int32_t tailLength = excess - headLength;

        if (headLength > 0) SDL_RenderDrawLine(renderer, x1, y, x1 + headLength - 1, y);
        if (tailLength > 0) SDL_RenderDrawLine(renderer, x2 - tailLength + 1, y, x2, y);

        int32_t x = x1 + headLength;
        for (int i=0; i < segments; i+=2) {
            SDL_RenderDrawLine(renderer, x, y, x + segmentLength - 1, y);
            x += segmentLength*2;
        }
    }

    inline void renderDrawDashedVerticalLine(SDL_Renderer* renderer, int32_t x, int32_t y1, int32_t y2) {
        const int32_t segmentLength = 4;
        int32_t height = y2 - y1 + 1;
        int32_t segments = height / segmentLength;
        int32_t excess = height % segmentLength;
        if (segments > 0 && segments%2 == 0) {
            excess += segmentLength;
            segments--;
        }
        int32_t headLength = excess/2;
        int32_t tailLength = excess - headLength;

        if (headLength > 0) SDL_RenderDrawLine(renderer, x, y1, x, y1 + headLength - 1);
        if (tailLength > 0) SDL_RenderDrawLine(renderer, x, y2 - tailLength + 1, x, y2);

        int32_t y = y1 + headLength;
        for (int i=0; i < segments; i+=2) {
            SDL_RenderDrawLine(renderer, x, y, x, y + segmentLength - 1);
            y += segmentLength*2;
        }
    }

    inline void renderDrawDashedRect(SDL_Renderer* renderer, const SDL_Rect* rect) {
        int32_t x1 = rect->x;
        int32_t y1 = rect->y;
        int32_t x2 = x1 + rect->w - 1;
        int32_t y2 = y1 + rect->h - 1;

        renderDrawDashedHorizontalLine(renderer, x1, x2, y1);
        renderDrawDashedHorizontalLine(renderer, x1, x2, y2);
        renderDrawDashedVerticalLine(renderer, x1, y1, y2);
        renderDrawDashedVerticalLine(renderer, x2, y1, y2);
    }
}
