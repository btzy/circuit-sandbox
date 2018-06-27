#include <cstdint>
#include <utility>

#include <SDL.h>

#include "buttonbar.hpp"
#include "mainwindow.hpp"
#include "playarea.hpp"
#include "point.hpp"
#include "filenewaction.hpp"
#include "fileopenaction.hpp"
#include "filesaveaction.hpp"
#include "changesimulationspeedaction.hpp"

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
        item->render(renderer, *this, { x, renderArea.y }, clickedItem == item.get() ? RenderStyle::CLICK : hoveredItem == item.get() ? RenderStyle::HOVER : RenderStyle::DEFAULT);
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

bool ButtonBar::processMouseButtonDown(const SDL_MouseButtonEvent& event) {
    auto x = renderArea.x;
    for (const auto& item : items) {
        x += item->width();
        if (x > event.x) { // found the correct button
            clickedItem = item.get();
            return true;
        }
    }
    return true;
}

void ButtonBar::processMouseButtonUp() {
    clickedItem->click(*this);
    clickedItem = nullptr;
}




template <uint16_t CodePoint>
void IconButton<CodePoint>::render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const {
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
        FileNewAction::start(buttonBar.mainWindow, buttonBar.mainWindow.currentAction.getStarter());
    }
    else if constexpr (CodePoint == IconCodePoints::OPEN) {
        FileOpenAction::start(buttonBar.mainWindow, buttonBar.playArea, buttonBar.mainWindow.currentAction.getStarter());
    }
    else if constexpr (CodePoint == IconCodePoints::SAVE) {
        FileSaveAction::start(buttonBar.mainWindow, SDL_GetModState(), buttonBar.mainWindow.currentAction.getStarter());
    }
    else if constexpr (CodePoint == IconCodePoints::STEP) {
        buttonBar.mainWindow.stateManager.stepSimulator();
    }
    else if constexpr (CodePoint == IconCodePoints::SPEED) {
        ChangeSimulationSpeedAction::start(buttonBar.mainWindow, buttonBar.mainWindow.renderer, buttonBar.mainWindow.currentAction.getStarter());
    }
}



void PlayPauseButton::render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const {
    const SDL_Rect destRect{ offset.x, offset.y, _length, _length };
    switch (style) {
    case RenderStyle::DEFAULT:
        SDL_RenderCopy(renderer, textureDefault[buttonBar.mainWindow.stateManager.simulatorRunning()].get(), nullptr, &destRect);
        break;
    case RenderStyle::HOVER:
        SDL_RenderCopy(renderer, textureHover[buttonBar.mainWindow.stateManager.simulatorRunning()].get(), nullptr, &destRect);
        break;
    case RenderStyle::CLICK:
        SDL_RenderCopy(renderer, textureClick[buttonBar.mainWindow.stateManager.simulatorRunning()].get(), nullptr, &destRect);
        break;
    }
}

void PlayPauseButton::setHeight(SDL_Renderer* renderer, const ButtonBar& buttonBar, int32_t height) {
    constexpr uint16_t codepoints[2] = { IconCodePoints::PLAY, IconCodePoints::PAUSE };
    _length = height;
    for (int i = 0; i < 2; ++i) {
        {
            textureDefault[i].reset(nullptr);
            SDL_Surface* surface = TTF_RenderGlyph_Shaded(buttonBar.iconFont, codepoints[i], ButtonBar::foregroundColor, ButtonBar::backgroundColor);
            textureDefault[i].reset(SDL_CreateTextureFromSurface(renderer, surface));
            SDL_FreeSurface(surface);
        }
        {
            textureHover[i].reset(nullptr);
            SDL_Surface* surface = TTF_RenderGlyph_Shaded(buttonBar.iconFont, codepoints[i], ButtonBar::foregroundColor, ButtonBar::hoverColor);
            textureHover[i].reset(SDL_CreateTextureFromSurface(renderer, surface));
            SDL_FreeSurface(surface);
        }
        {
            textureClick[i].reset(nullptr);
            SDL_Surface* surface = TTF_RenderGlyph_Shaded(buttonBar.iconFont, codepoints[i], ButtonBar::backgroundColor, ButtonBar::foregroundColor);
            textureClick[i].reset(SDL_CreateTextureFromSurface(renderer, surface));
            SDL_FreeSurface(surface);
        }
    }
}

void PlayPauseButton::click(ButtonBar& buttonBar) {
    buttonBar.mainWindow.stateManager.startOrStopSimulator();
}
