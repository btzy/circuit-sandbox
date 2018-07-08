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
#include "historyaction.hpp"
#include "changesimulationspeedaction.hpp"

ButtonBar::ButtonBar(MainWindow& mainWindow, PlayArea& playArea) : mainWindow(mainWindow), playArea(playArea), iconFont("ButtonIcons.ttf", MainWindow::LOGICAL_BUTTONBAR_HEIGHT) {}

void ButtonBar::layoutComponents(SDL_Renderer* renderer) {
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
    if (hoveredItem) {
        const char* description = hoveredItem->description(*this);
        if (description) {
            SDL_Surface* surface = TTF_RenderText_Shaded(mainWindow.interfaceFont, description, ButtonBar::foregroundColor, ButtonBar::backgroundColor);
            if (surface != nullptr) { // if there is text to show
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_Rect target{ x + mainWindow.logicalToPhysicalSize(20),renderArea.y + (mainWindow.BUTTONBAR_HEIGHT - surface->h) / 2, surface->w, surface->h };
                SDL_RenderCopy(renderer, texture, nullptr, &target);
                SDL_DestroyTexture(texture);
                SDL_FreeSurface(surface);
            }
        }
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
    if (!clickedItem) return;
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
        buttonBar.mainWindow.currentAction.reset();
        buttonBar.mainWindow.stateManager.stepSimulator();
    }
    else if constexpr (CodePoint == IconCodePoints::RESET) {
        buttonBar.mainWindow.currentAction.reset();
        buttonBar.mainWindow.stateManager.resetSimulator();
    }
    else if constexpr (CodePoint == IconCodePoints::SPEED) {
        ChangeSimulationSpeedAction::start(buttonBar.mainWindow, buttonBar.mainWindow.renderer, buttonBar.mainWindow.currentAction.getStarter());
    }
    else if constexpr (CodePoint == IconCodePoints::UNDO) {
        HistoryAction::startByUndoing(buttonBar.mainWindow, buttonBar.mainWindow.currentAction.getStarter());
    }
    else if constexpr (CodePoint == IconCodePoints::REDO) {
        HistoryAction::startByRedoing(buttonBar.mainWindow, buttonBar.mainWindow.currentAction.getStarter());
    }
}


template <uint16_t CodePoint>
const char* IconButton<CodePoint>::description(const ButtonBar& buttonBar) const {
    if constexpr (CodePoint == IconCodePoints::NEW) {
        return "Start a new instance (Ctrl-N)";
    }
    else if constexpr (CodePoint == IconCodePoints::OPEN) {
        return "Open an existing file (Ctrl-O)";
    }
    else if constexpr (CodePoint == IconCodePoints::SAVE) {
        return "Save to file (Ctrl-S); hold Shift key for \"Save As\" dialog";
    }
    else if constexpr (CodePoint == IconCodePoints::STEP) {
        return "Process single step of simulation (Right arrow)";
    }
    else if constexpr (CodePoint == IconCodePoints::RESET) {
        return "Reset on/off state of circuit (R)";
    }
    else if constexpr (CodePoint == IconCodePoints::SPEED) {
        return "Set simulation speed (Ctrl-Space)";
    }
    else if constexpr (CodePoint == IconCodePoints::UNDO) {
        return "Undo the last action (Ctrl-Z)";
    }
    else if constexpr (CodePoint == IconCodePoints::REDO) {
        return "Redo the last action (Ctrl-Y)";
    }
}


void StepButton::render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const {
    if (!buttonBar.mainWindow.stateManager.simulatorRunning()) {
        IconButton::render(renderer, buttonBar, offset, style);
    }
}

void StepButton::click(ButtonBar& buttonBar) {
    if (!buttonBar.mainWindow.stateManager.simulatorRunning()) {
        IconButton::click(buttonBar);
    }
}

const char* StepButton::description(const ButtonBar& buttonBar) const {
    if (!buttonBar.mainWindow.stateManager.simulatorRunning()) {
        return IconButton::description(buttonBar);
    }
    else {
        return nullptr;
    }
}


void UndoButton::render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const {
    if (buttonBar.mainWindow.stateManager.historyManager.canUndo()) {
        IconButton::render(renderer, buttonBar, offset, style);
    }
}

void UndoButton::click(ButtonBar& buttonBar) {
    if (buttonBar.mainWindow.stateManager.historyManager.canUndo()) {
        IconButton::click(buttonBar);
    }
}

const char* UndoButton::description(const ButtonBar& buttonBar) const {
    if (buttonBar.mainWindow.stateManager.historyManager.canUndo()) {
        return IconButton::description(buttonBar);
    }
    else {
        return nullptr;
    }
}


void RedoButton::render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const {
    if (buttonBar.mainWindow.stateManager.historyManager.canRedo()) {
        IconButton::render(renderer, buttonBar, offset, style);
    }
}

void RedoButton::click(ButtonBar& buttonBar) {
    if (buttonBar.mainWindow.stateManager.historyManager.canRedo()) {
        IconButton::click(buttonBar);
    }
}

const char* RedoButton::description(const ButtonBar& buttonBar) const {
    if (buttonBar.mainWindow.stateManager.historyManager.canRedo()) {
        return IconButton::description(buttonBar);
    }
    else {
        return nullptr;
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
    buttonBar.mainWindow.currentAction.reset();
    buttonBar.mainWindow.stateManager.startOrStopSimulator();
}

const char* PlayPauseButton::description(const ButtonBar& buttonBar) const {
    if (buttonBar.mainWindow.stateManager.simulatorRunning()) {
        return "Pause simulation (Space)";
    }
    else {
        return "Run simulation (Space)";
    }
}
