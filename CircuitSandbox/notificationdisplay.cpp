/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <numeric>
#include <algorithm>
#include <cmath>
#include <cassert>

#include <boost/math/constants/constants.hpp>

#include <SDL_ttf.h>

#include "cstring.hpp"
#include "interpolate.hpp"
#include "sdl_surface_create.hpp"
#include "notificationdisplay.hpp"
#include "mainwindow.hpp"

NotificationDisplay::NotificationDisplay(MainWindow& mainWindow, Flags visibleFlags) : mainWindow(mainWindow), visibleFlags(visibleFlags) {}

void NotificationDisplay::render(SDL_Renderer* renderer) {
    // from bottom-left corner
    ext::point currOffset = mainWindow.logicalToPhysicalSize(LOGICAL_OFFSET);
    for (size_t i = 0; i != notifications.size(); ++i) {
        assert(notifications[i] != nullptr);
        Notification& notification = *(notifications[i]);
        if (notification.texture) {
            const SDL_Rect destRect{
                renderArea.x + currOffset.x,
                renderArea.y + renderArea.h - currOffset.y - notification.textureSize.y,
                notification.textureSize.x,
                notification.textureSize.y
            };
            const auto& now = Drawable::renderTime;
            if (now < notification.expireTime) {
                // non-expired notification
                // TODO: entry effects
                SDL_RenderCopy(renderer, notification.texture.get(), nullptr, &destRect);
                currOffset.y += notification.textureSize.y + mainWindow.logicalToPhysicalSize(LOGICAL_SPACING);
            }
            else {
                // Expired notification that is in the process of fading out
                static constexpr Drawable::RenderClock::duration slideDownTime = 300ms;
                static constexpr Drawable::RenderClock::duration fadeOutTime = 500ms;
                assert(now >= notification.expireTime);
                if (now >= notification.expireTime + fadeOutTime) {
                    // notification is already invisible, remove from vector,
                    // and decrement the loop counter to account for erasing this element.
                    notifications.erase(notifications.begin() + i--);
                    
                }
                else {
                    // still fading
                    Uint8 alpha = static_cast<Uint8>(ext::interpolate_time(notification.expireTime, notification.expireTime + fadeOutTime, 255, 0, now));
                    assert(alpha >= 0 && alpha <= 255);
                    SDL_SetTextureAlphaMod(notification.texture.get(), alpha);
                    SDL_RenderCopy(renderer, notification.texture.get(), nullptr, &destRect);
                    const int32_t effectiveHeight = notification.textureSize.y + mainWindow.logicalToPhysicalSize(LOGICAL_SPACING);
                    if (now <= notification.expireTime + (fadeOutTime - slideDownTime)) {
                        currOffset.y += effectiveHeight;
                    }
                    else {
                        using namespace boost::math::double_constants;
                        int32_t computedHeight = static_cast<int32_t>(effectiveHeight*((1 + std::sin(ext::interpolate_time(notification.expireTime + (fadeOutTime - slideDownTime), notification.expireTime + fadeOutTime, half_pi, -half_pi, now))) / 2));
                        currOffset.y += computedHeight;
                    }
                }
            }
        }
    }
}

NotificationDisplay::NotificationHandle NotificationDisplay::add(Flags flags, Drawable::RenderClock::time_point expire, Data description) {
    if (visibleFlags & flags) {
        std::shared_ptr<Notification> notification = std::make_shared<Notification>(std::move(description), flags, expire);
        notification->layout(mainWindow.renderer, *this);
        NotificationHandle handle(notification);
        notifications.emplace_back(std::move(notification));
        return handle;
    }
    else {
        return NotificationHandle();
    }
}

void NotificationDisplay::remove(const NotificationHandle& data) noexcept {
    std::shared_ptr<Notification> notification = data.lock();
    if (notification && notification->expireTime >= Drawable::renderTime) {
        // set to expire now if it isn't already expired
        notification->expireTime = Drawable::renderTime;
    }
}

NotificationDisplay::NotificationHandle NotificationDisplay::modifyOrAdd(const NotificationHandle& data, Flags flags, Drawable::RenderClock::time_point expire, Data description) {
    std::shared_ptr<Notification> notification = data.lock();
    if (notification && notification->expireTime >= Drawable::renderTime) {
        // modify, since it is not already removed
        assert(notification->flags == flags);
        notification->data = std::move(description);
        notification->expireTime = expire;
        notification->layout(mainWindow.renderer, *this);
        return data;
    }
    else {
        return add(flags, expire, std::move(description));
    }
}

void NotificationDisplay::Notification::layout(SDL_Renderer* renderer, NotificationDisplay& notificationDisplay) {
    texture.reset(nullptr);
    if (data.empty()) {
        return;
    }

    TTF_Font* font = notificationDisplay.mainWindow.interfaceFont;

    // proper multiline multicolor text
    int32_t lineHeight = TTF_FontHeight(font);
    const int32_t fullwidth = notificationDisplay.renderArea.w - LOGICAL_OFFSET.x * 2;
    int32_t availableWidth = fullwidth;
    // surfaces, grouped by line
    std::vector<std::vector<SDL_Surface*>> surfaces;
    surfaces.emplace_back();
    for (const ColorText& colorText : data) {
        const char* text = colorText.text.c_str();
        const char* done = text;
        while(*done) {
            if (surfaces.back().empty()) {
                // remove leading spaces if we are at the start of the line
                done = ext::next_non_space(done);
            }
            assert(*done); // ensures that the last character is a non-space, and the string isn't a space
            const char* best = done;
            while (*best) {
                const char* next = ext::next_space(ext::next_non_space(best)); // get the next space at or after the current pointer
                std::string tmp(done, next);
                int32_t textWidth;
                TTF_SizeText(font, tmp.c_str(), &textWidth, nullptr);
                if (textWidth <= availableWidth) {
                    best = next;
                }
                else {
                    // need to break the text
                    break;
                }
            }
            SDL_Surface* textSurface;
            if (best == done && surfaces.back().empty()) {
                // even a single word wouldn't fit in the line
                assert(availableWidth > 0);
                best = ext::next_space(ext::next_non_space(best));
                std::string tmp(done, best);
                SDL_Surface* tmpSurface = TTF_RenderText_Blended(font, tmp.c_str(), colorText.color);
                if (tmpSurface) {
                    textSurface = create_alpha_surface(availableWidth, tmpSurface->h);
                    assert(textSurface);
                    SDL_Rect srcRect{ 0, 0, availableWidth, tmpSurface->h };
                    SDL_Rect destRect{ 0, 0, availableWidth, tmpSurface->h };
                    SDL_BlitSurface(tmpSurface, &srcRect, textSurface, &destRect);
                    SDL_FreeSurface(tmpSurface);
                }
                else {
                    textSurface = nullptr;
                }
            }
            else if (best != done) {
                std::string tmp(done, best);
                textSurface = TTF_RenderText_Blended(font, tmp.c_str(), colorText.color);
            }
            else {
                textSurface = nullptr;
            }
            done = best;
            if (textSurface) {
                surfaces.back().emplace_back(textSurface);
                availableWidth -= textSurface->w;
                assert(availableWidth >= 0);
            }
            if (*done) {
                // if we get here, it means we were cut off due to line size restrictions,
                // so we add a new line.
                surfaces.emplace_back();
                availableWidth = fullwidth;
            }
        }
    }

    // if there is an additional new line, remove it
    if (surfaces.back().empty()) {
        surfaces.pop_back();
    }

    // if there's no lines to render, just return an empty texture
    if (surfaces.empty()) {
        return;
    }

    int32_t max_width = std::accumulate(surfaces.begin(), surfaces.end(), 0, [](int32_t best, const std::vector<SDL_Surface*>& line) {
        return std::max(best, std::accumulate(line.begin(), line.end(), 0, [](int32_t prev, const SDL_Surface* surface) {
            return prev + surface->w;
        }));
    });

    const ext::point TEXT_PADDING = notificationDisplay.mainWindow.logicalToPhysicalSize(LOGICAL_TEXT_PADDING);

    // create a new surface and paint it with the background color
    SDL_Surface* fullsurface = create_alpha_surface(max_width + TEXT_PADDING.x * 2, static_cast<int32_t>(surfaces.size()) * lineHeight + TEXT_PADDING.y * 2);
    assert(fullsurface);
    SDL_FillRect(fullsurface, nullptr, SDL_MapRGBA(fullsurface->format, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a));

    ext::point offset = TEXT_PADDING;
    for (auto& line : surfaces) {
        offset.x = TEXT_PADDING.x;
        for (SDL_Surface* surface : line) {
            assert(surface);
            assert(offset.x + surface->w <= fullsurface->w && offset.y + surface->h <= fullsurface->h);
            SDL_Rect destRect{ offset.x, offset.y, surface->w, surface->h };
            SDL_BlitSurface(surface, nullptr, fullsurface, &destRect);
            offset.x += surface->w;
            SDL_FreeSurface(surface);
        }
        offset.y += lineHeight;
    }

    texture.reset(SDL_CreateTextureFromSurface(renderer, fullsurface));
    textureSize = { fullsurface->w, fullsurface->h };
    SDL_FreeSurface(fullsurface);

}
