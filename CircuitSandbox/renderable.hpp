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

#include <memory>
#include <SDL.h>
#include "sdl_automatic.hpp"
#include "point.hpp"

/**
 * A general-purpose class for standard renderable objects.
 *
 * Owners are expected to set up the three textures and the renderArea before they do any rendering.
 * Those textures may be changed any time.
 * When setting the textures and renderArea, owners must ensure that the textures have the same size as the renderArea.
 *
 * When owners want to render, they should call render(SDL_Renderer*, RenderStyle) const, or render<same value of type RenderStyle>(SDL_Renderer*) const.
 */

enum class RenderStyle : unsigned char {
    DEFAULT,
    HOVER,
    CLICK
};

class Renderable {
protected:
    UniqueTexture textureDefault;
    UniqueTexture textureHover;
    UniqueTexture textureClick;
public:
    SDL_Rect renderArea;
    void render(SDL_Renderer* renderer, RenderStyle style) const {
        switch (style) {
        case RenderStyle::DEFAULT:
            render<RenderStyle::DEFAULT>(renderer);
            return;
        case RenderStyle::HOVER:
            render<RenderStyle::HOVER>(renderer);
            return;
        case RenderStyle::CLICK:
            render<RenderStyle::CLICK>(renderer);
            return;
        }
    }
    template <RenderStyle style>
    void render(SDL_Renderer* renderer) const {
        if constexpr(style == RenderStyle::DEFAULT) {
            SDL_RenderCopy(renderer, textureDefault.get(), nullptr, &renderArea);
        }
        else if constexpr(style == RenderStyle::HOVER) {
            SDL_RenderCopy(renderer, textureHover.get(), nullptr, &renderArea);
        }
        else if constexpr(style == RenderStyle::CLICK) {
            SDL_RenderCopy(renderer, textureClick.get(), nullptr, &renderArea);
        }
    }
};
