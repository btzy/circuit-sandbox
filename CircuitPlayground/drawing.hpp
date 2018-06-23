#pragma once

#include <SDL.h>

namespace extensions {

    void renderDrawDashedHorizontalLine(SDL_Renderer* renderer, int32_t x1, int32_t x2, int32_t y) {
        const int32_t segmentLength = 4;
        int32_t width = x2 - x1 + 1;
        int32_t segments = width / segmentLength;
        int32_t excess = width % segmentLength;
        if (segments%2 == 0) {
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

    void renderDrawDashedVerticalLine(SDL_Renderer* renderer, int32_t x, int32_t y1, int32_t y2) {
        const int32_t segmentLength = 4;
        int32_t height = y2 - y1 + 1;
        int32_t segments = height / segmentLength;
        int32_t excess = height % segmentLength;
        if (segments%2 == 0) {
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

    void renderDrawDashedRect(SDL_Renderer* renderer, const SDL_Rect* rect) {
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
