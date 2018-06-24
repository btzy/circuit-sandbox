#include <cstdint>
#include <utility>

#include <SDL.h>

#include "buttonbar.hpp"
#include "mainwindow.hpp"
#include "playarea.hpp"
#include "filenewaction.hpp"
#include "fileopenaction.hpp"
#include "filesaveaction.hpp"

ButtonBar::ButtonBar(MainWindow& mainWindow, PlayArea& playArea) : mainWindow(mainWindow), playArea(playArea), iconFont("ButtonIcons.ttf", MainWindow::LOGICAL_BUTTONBAR_HEIGHT) {}

void ButtonBar::updateDpi(SDL_Renderer* renderer) {
    iconFont.updateDPI(mainWindow);
    int32_t height = mainWindow.BUTTONBAR_HEIGHT;
    for (auto& item : items) {
        item->setHeight(renderer, *this, height);
    }
}

void ButtonBar::render(SDL_Renderer* renderer) const {
    auto x = renderArea.x;
    for (const auto& item : items) {
        item->render(renderer, { x, renderArea.y }, clickedItem == item.get() ? RenderStyle::CLICK : hoveredItem == item.get() ? RenderStyle::HOVER : RenderStyle::DEFAULT);
        x += item->width();
    }
}

void ButtonBar::processMouseHover(const SDL_MouseMotionEvent& event) {
    auto x = renderArea.x;
    for (const auto& item : items) {
        x += item->width();
        if (x > event.x) {
            hoveredItem = item.get();
            return;
        }
    }
    hoveredItem = nullptr;
}
void ButtonBar::processMouseLeave() {
    hoveredItem = nullptr;
}

void ButtonBar::processMouseButtonDown(const SDL_MouseButtonEvent& event) {
    auto x = renderArea.x;
    for (const auto& item : items) {
        x += item->width();
        if (x > event.x) {
            clickedItem = item.get();
            item->click(*this);
            return;
        }
    }
}

void ButtonBar::processMouseButtonUp(const SDL_MouseButtonEvent &) {
    clickedItem = nullptr;
}




template <uint16_t CodePoint>
void IconButton<CodePoint>::render(SDL_Renderer* renderer, const ext::point& offset, RenderStyle style) const {
    const SDL_Rect destRect{ offset.x, offset.y, _length, _length };
    switch (style) {
    case RenderStyle::DEFAULT:
        SDL_RenderCopy(renderer, textureDefault.get(), nullptr, &destRect);
        break;
    case RenderStyle::HOVER:
        SDL_RenderCopy(renderer, textureHover.get(), nullptr, &destRect);
        break;
    case RenderStyle::CLICK:
        SDL_RenderCopy(renderer, textureClick.get(), nullptr, &destRect);
        break;
    }
}

template <uint16_t CodePoint>
void IconButton<CodePoint>::setHeight(SDL_Renderer* renderer, const ButtonBar& buttonBar, int32_t height) {
    _length = height;
    {
        textureDefault.reset(nullptr);
        SDL_Surface* surface = TTF_RenderGlyph_Shaded(buttonBar.iconFont, CodePoint, ButtonBar::foregroundColor, ButtonBar::backgroundColor);
        textureDefault.reset(SDL_CreateTextureFromSurface(renderer, surface));
        SDL_FreeSurface(surface);
    }
    {
        textureHover.reset(nullptr);
        SDL_Surface* surface = TTF_RenderGlyph_Shaded(buttonBar.iconFont, CodePoint, ButtonBar::foregroundColor, ButtonBar::hoverColor);
        textureHover.reset(SDL_CreateTextureFromSurface(renderer, surface));
        SDL_FreeSurface(surface);
    }
    {
        textureClick.reset(nullptr);
        SDL_Surface* surface = TTF_RenderGlyph_Shaded(buttonBar.iconFont, CodePoint, ButtonBar::backgroundColor, ButtonBar::foregroundColor);
        textureClick.reset(SDL_CreateTextureFromSurface(renderer, surface));
        SDL_FreeSurface(surface);
    }
}

template <uint16_t CodePoint>
void IconButton<CodePoint>::click(ButtonBar& buttonBar) {
    if constexpr (CodePoint == IconCodePoints::NEW) {
        buttonBar.playArea.startInstantaneousAction<FileNewAction>();
    }
    else if constexpr (CodePoint == IconCodePoints::OPEN) {
        buttonBar.playArea.startInstantaneousAction<FileOpenAction>();
    }
    else if constexpr (CodePoint == IconCodePoints::SAVE) {
        SDL_Keymod modifiers = SDL_GetModState();
        buttonBar.playArea.saveFile(modifiers & KMOD_SHIFT);
    }
    else if constexpr (CodePoint == IconCodePoints::PLAY) {
        buttonBar.playArea.startSimulator();
    }
    else if constexpr (CodePoint == IconCodePoints::PAUSE) {
        buttonBar.playArea.stopSimulator();
    }
    else if constexpr (CodePoint == IconCodePoints::STEP) {
        buttonBar.playArea.stepSimulator();
    }
}