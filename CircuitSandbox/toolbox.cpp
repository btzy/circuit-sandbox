#include <stdexcept>
#include <type_traits>
#include <string>
#include <cstring>
#include <algorithm>
#include <tuple>

#include <SDL.h>
#include <SDL_ttf.h>

#include "toolbox.hpp"
#include "mainwindow.hpp"
#include "tag_tuple.hpp"


using namespace std::literals::string_literals; // gives the 's' suffix for strings


Toolbox::Toolbox(MainWindow& main_window) : 
    mainWindow(main_window),
    mouseoverToolIndex(MainWindow::EMPTY_INDEX),
    mouseclickToolIndex(MainWindow::EMPTY_INDEX),
    toolButtons(std::apply([&](auto... tool_tags) {
        return tool_buttons_t{ ToolRenderable<typename decltype(tool_tags)::type>(*this)... };
    }, tool_tags_t::transform<ext::tag>::instantiate<std::tuple>{})) {
};


void Toolbox::layoutComponents(SDL_Renderer* renderer) {
    BUTTON_HEIGHT = mainWindow.logicalToPhysicalSize(LOGICAL_BUTTON_HEIGHT);
    PADDING_HORIZONTAL = mainWindow.logicalToPhysicalSize(LOGICAL_PADDING_HORIZONTAL);
    PADDING_VERTICAL = mainWindow.logicalToPhysicalSize(LOGICAL_PADDING_VERTICAL);
    BUTTON_PADDING = mainWindow.logicalToPhysicalSize(LOGICAL_BUTTON_PADDING);
    BUTTON_SPACING = mainWindow.logicalToPhysicalSize(LOGICAL_BUTTON_SPACING);

    SDL_Rect buttonRect{
        renderArea.x + PADDING_HORIZONTAL + BUTTON_PADDING,
        renderArea.y + PADDING_VERTICAL + BUTTON_PADDING,
        renderArea.w - 2 * (PADDING_HORIZONTAL + BUTTON_PADDING),
        BUTTON_HEIGHT - 2 * BUTTON_PADDING
    };
    tool_tags_t::for_each([&](auto, auto int_tag) {
        std::get<decltype(int_tag)::value>(toolButtons).layoutComponents(renderer, buttonRect);
        buttonRect.y += BUTTON_HEIGHT + BUTTON_SPACING;
    });
}


void Toolbox::render(SDL_Renderer* renderer) const {
    // draw the selection rectangles
    for (size_t i = 0; i != NUM_INPUT_HANDLES; ++i) {
        if (mainWindow.selectedToolIndices[i] != MainWindow::EMPTY_INDEX) {
            SDL_SetRenderDrawColor(renderer, selectorHandleColors[i].r, selectorHandleColors[i].g, selectorHandleColors[i].b, selectorHandleColors[i].a);
            const SDL_Rect destRect{ renderArea.x + PADDING_HORIZONTAL, renderArea.y + PADDING_VERTICAL + (BUTTON_HEIGHT + BUTTON_SPACING) * static_cast<int>(mainWindow.selectedToolIndices[i]), renderArea.w - 2 * PADDING_HORIZONTAL, BUTTON_HEIGHT };
            SDL_RenderFillRect(renderer, &destRect);
        }
    }


    // draw the buttons to the screen one-by-one
    tool_tags_t::for_each([&](auto, auto index_tag) {
        // 'index' is the index of this element inside the tool_tags_t
        constexpr size_t Index = decltype(index_tag)::value;

        renderButton<Index>(renderer);
    });

    // render the status of the current action if any
    const char* status = mainWindow.currentAction.getStatus();
    if (status) {
        SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(mainWindow.interfaceFont, status, SDL_Color{0xFF, 0xFF, 0xFF, 0xFF}, renderArea.w - 2 * PADDING_HORIZONTAL - mainWindow.logicalToPhysicalSize(8));
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        int textureWidth, textureHeight;
        SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);
        const SDL_Rect destRect{renderArea.x + PADDING_HORIZONTAL + BUTTON_PADDING + mainWindow.logicalToPhysicalSize(4), renderArea.y + PADDING_VERTICAL * 3 + (BUTTON_HEIGHT + BUTTON_SPACING) * static_cast<int>(tool_tags_t::size), textureWidth, textureHeight};
        SDL_RenderCopy(renderer, texture, nullptr, &destRect);
        SDL_DestroyTexture(texture);
    }

}


void Toolbox::processMouseHover(const SDL_MouseMotionEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int offsetX = event.x - renderArea.x;
    int offsetY = event.y - renderArea.y;

    // reset the mouseover context
    mouseoverToolIndex = MainWindow::EMPTY_INDEX;

    // check left/right out of bounds
    if(offsetX < PADDING_HORIZONTAL || offsetX >= renderArea.w - PADDING_HORIZONTAL) return;

    // element index
    size_t index = static_cast<size_t>((offsetY - PADDING_VERTICAL) / (BUTTON_HEIGHT + BUTTON_SPACING));
    if (index >= tool_tags_t::size) return;

    // check if the mouse is on the button spacing instead of on the actual button
    if ((offsetY - PADDING_VERTICAL) - static_cast<int32_t>(index) * (BUTTON_HEIGHT + BUTTON_SPACING) >= BUTTON_HEIGHT) return;

    // save the index since it is valid
    mouseoverToolIndex = index;
}


bool Toolbox::processMouseButtonDown(const SDL_MouseButtonEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int offsetX = event.x - renderArea.x;
    int offsetY = event.y - renderArea.y;

    // check left/right out of bounds
    if(offsetX < PADDING_HORIZONTAL || offsetX >= renderArea.w - PADDING_HORIZONTAL) return true;

    // element index
    size_t index = static_cast<size_t>((offsetY - PADDING_VERTICAL) / (BUTTON_HEIGHT + BUTTON_SPACING));
    if (index >= tool_tags_t::size) return true;

    // check if the mouse is on the button spacing instead of on the actual button
    if ((offsetY - PADDING_VERTICAL) - static_cast<int32_t>(index) * (BUTTON_HEIGHT + BUTTON_SPACING) >= BUTTON_HEIGHT) return true;

    mouseclickToolIndex = index;
    mouseclickInputHandle = resolveInputHandleIndex(event);
    return true;
}


void Toolbox::processMouseButtonUp() {

    if (mouseoverToolIndex == mouseclickToolIndex) {
        mainWindow.bindTool(mouseclickInputHandle, mouseclickToolIndex);
    }

    mouseclickToolIndex = MainWindow::EMPTY_INDEX;
}

void Toolbox::processMouseLeave() {
    mouseoverToolIndex = MainWindow::EMPTY_INDEX;
}


template <size_t Index>
inline void Toolbox::renderButton(SDL_Renderer* renderer) const {
    if (mouseclickToolIndex == Index) {
        std::get<Index>(toolButtons).template render<RenderStyle::CLICK>(renderer);
    }
    else if (mouseoverToolIndex == Index) {
        std::get<Index>(toolButtons).template render<RenderStyle::HOVER>(renderer);
    }
    else {
        std::get<Index>(toolButtons).template render<RenderStyle::DEFAULT>(renderer);
    }
}


template <typename Tool>
Toolbox::ToolRenderable<Tool>::ToolRenderable(Toolbox& owner) noexcept : owner(owner) {};


template <typename Tool>
void Toolbox::ToolRenderable<Tool>::prepareTexture(SDL_Renderer* renderer, UniqueTexture& textureStore, const SDL_Color& backColor) {
    textureStore.reset(nullptr);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_TARGET, renderArea.w, renderArea.h);
    SDL_Surface* textSurface = TTF_RenderText_Shaded(owner.mainWindow.interfaceFont, Tool::displayName, Tool::displayColor, backColor);
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

    SDL_SetRenderTarget(renderer, texture);

    SDL_SetRenderDrawColor(renderer, backColor.r, backColor.g, backColor.b, backColor.a);
    SDL_RenderClear(renderer);

    // text
    {
        const SDL_Rect targetRect{ owner.mainWindow.logicalToPhysicalSize(LOGICAL_TEXT_LEFT_PADDING), (renderArea.h - textSurface->h) / 2, textSurface->w, textSurface->h };
        SDL_RenderCopy(renderer, textTexture, nullptr, &targetRect);
    }

    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);
    textureStore.reset(texture);
}
template <typename Tool>
void Toolbox::ToolRenderable<Tool>::layoutComponents(SDL_Renderer* renderer, const SDL_Rect& render_area) {
    renderArea = render_area;
    prepareTexture(renderer, textureDefault, backgroundColor);
    prepareTexture(renderer, textureHover, hoverColor);
    prepareTexture(renderer, textureClick, clickColor);
}
