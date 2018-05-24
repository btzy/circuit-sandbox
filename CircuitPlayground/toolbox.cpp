#include <stdexcept>
#include <type_traits>
#include <string>
#include <cstring>
#include <algorithm>

#include <SDL.h>
#include <SDL_ttf.h>

#include "toolbox.hpp"
#include "mainwindow.hpp"
#include "tag_tuple.hpp"


using namespace std::literals::string_literals; // gives the 's' suffix for strings


Toolbox::Toolbox(MainWindow& main_window) : mainWindow(main_window) {};


void Toolbox::updateDpi() {
    BUTTON_HEIGHT = mainWindow.logicalToPhysicalSize(LOGICAL_BUTTON_HEIGHT);
    PADDING_HORIZONTAL = mainWindow.logicalToPhysicalSize(LOGICAL_PADDING_HORIZONTAL);
    PADDING_VERTICAL = mainWindow.logicalToPhysicalSize(LOGICAL_PADDING_VERTICAL);
}


void Toolbox::render(SDL_Renderer* renderer) const {
    // draw a grey border around the toolbox
    SDL_SetRenderDrawColor(renderer, 0x66, 0x66, 0x66, 0xFF);
    SDL_RenderDrawRect(renderer, &renderArea);

    // load the font (TODO: this should be made so that we don't load the font at every render call)
    TTF_Font* button_font;
    {
        char* cwd = SDL_GetBasePath();
        if (cwd == nullptr) {
            throw std::runtime_error("SDL_GetBasePath() failed:  "s + SDL_GetError());
        }
        const char* font_name = "OpenSans-Bold.ttf";
        char* font_path = new char[std::strlen(cwd) + std::strlen(font_name) + 1];
        std::strcpy(font_path, cwd);
        std::strcat(font_path, font_name);
        SDL_free(cwd);
        button_font = TTF_OpenFont(font_path, mainWindow.logicalToPhysicalSize(12));
        delete[] font_path;
    }
    if (button_font == nullptr) {
        throw std::runtime_error("TTF_OpenFont() failed:  "s + TTF_GetError());
    }



    // draw the selection rectangles
    for (size_t i = 0; i != NUM_INPUT_HANDLES; ++i) {
        if (mainWindow.selectedToolIndices[i] != MainWindow::EMPTY_INDEX) {
            SDL_SetRenderDrawColor(renderer, selectorHandleColors[i].r, selectorHandleColors[i].g, selectorHandleColors[i].b, selectorHandleColors[i].a);
            const SDL_Rect destRect{ renderArea.x + PADDING_HORIZONTAL, renderArea.y + PADDING_VERTICAL + (BUTTON_HEIGHT + BUTTON_SPACING) * static_cast<int>(mainWindow.selectedToolIndices[i]), renderArea.w - 2 * PADDING_HORIZONTAL, BUTTON_HEIGHT };
            SDL_RenderFillRect(renderer, &destRect);
        }
    }


    // draw the buttons to the screen one-by-one
    MainWindow::tool_tags::for_each([this, renderer, button_font](const auto tool_tag, const auto index_tag) {
        // 'Tool' is the type of tool (e.g. ConductiveWire)
        using Tool = typename decltype(tool_tag)::type;
        // 'index' is the index of this element inside the tool_tags
        constexpr size_t index = decltype(index_tag)::value;

        SDL_Color backgroundColorForText = MainWindow::backgroundColor;

        // Make a grey rectangle if the element is being moused over, otherwise make a black rectangle

        {
            if (mouseoverToolIndex == index) { // <-- test that current index is the index being mouseovered
                backgroundColorForText = SDL_Color{ 0x44, 0x44, 0x44, 0xFF };
            }
            SDL_SetRenderDrawColor(renderer, backgroundColorForText.r, backgroundColorForText.g, backgroundColorForText.b, backgroundColorForText.a);
            const SDL_Rect destRect{ renderArea.x + PADDING_HORIZONTAL + BUTTON_PADDING, renderArea.y + PADDING_VERTICAL + (BUTTON_HEIGHT + BUTTON_SPACING) * static_cast<int>(index) + BUTTON_PADDING, renderArea.w - 2 * (PADDING_HORIZONTAL + BUTTON_PADDING), BUTTON_HEIGHT - 2 * BUTTON_PADDING };
            SDL_RenderFillRect(renderer, &destRect);
        }

        // Render the text
        {
            SDL_Surface* surface = TTF_RenderText_Shaded(button_font, Tool::displayName, Tool::displayColor, backgroundColorForText);
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
            int textureWidth, textureHeight;
            SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);
            // the complicated calculation here makes the text vertically-centered and horizontally displaced by 2 logical pixels
            const SDL_Rect destRect{renderArea.x + PADDING_HORIZONTAL + BUTTON_PADDING + mainWindow.logicalToPhysicalSize(4), renderArea.y + PADDING_VERTICAL + (BUTTON_HEIGHT + BUTTON_SPACING) * static_cast<int>(index) + (BUTTON_HEIGHT - textureHeight) / 2, textureWidth, textureHeight};
            SDL_RenderCopy(renderer, texture, nullptr, &destRect);
            SDL_DestroyTexture(texture);
        }
    });


    // free the font so we don't leak memory
    TTF_CloseFont(button_font);
}


void Toolbox::processMouseMotionEvent(const SDL_MouseMotionEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int offsetX = event.x - renderArea.x;
    int offsetY = event.y - renderArea.y;

    // reset the mouseover context
    mouseoverToolIndex = MainWindow::EMPTY_INDEX;

    // check left/right out of bounds
    if(offsetX < PADDING_HORIZONTAL || offsetX >= renderArea.w - PADDING_HORIZONTAL) return;

    // element index
    size_t index = static_cast<size_t>((offsetY - PADDING_VERTICAL) / (BUTTON_HEIGHT + BUTTON_SPACING));
    if (index >= MainWindow::tool_tags::size) return;

    // check if the mouse is on the button spacing instead of on the actual button
    if ((offsetY - PADDING_VERTICAL) - index * (BUTTON_HEIGHT + BUTTON_SPACING) >= BUTTON_HEIGHT) return;

    // save the index since it is valid
    mouseoverToolIndex = index;
}


void Toolbox::processMouseButtonDownEvent(const SDL_MouseButtonEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int offsetX = event.x - renderArea.x;
    int offsetY = event.y - renderArea.y;

    // check left/right out of bounds
    if(offsetX < PADDING_HORIZONTAL || offsetX >= renderArea.w - PADDING_HORIZONTAL) return;

    // element index
    size_t index = static_cast<size_t>((offsetY - PADDING_VERTICAL) / (BUTTON_HEIGHT + BUTTON_SPACING));
    if (index >= MainWindow::tool_tags::size) return;

    // check if the mouse is on the button spacing instead of on the actual button
    if ((offsetY - PADDING_VERTICAL) - index * (BUTTON_HEIGHT + BUTTON_SPACING) >= BUTTON_HEIGHT) return;

    // if there is already a handle binded to this tool, then swap the handles
    auto it = std::find(mainWindow.selectedToolIndices, mainWindow.selectedToolIndices + NUM_INPUT_HANDLES, index);
    if (it == mainWindow.selectedToolIndices + NUM_INPUT_HANDLES) {
        // cannot find an existing handle
        mainWindow.selectedToolIndices[MainWindow::resolveInputHandleIndex(event)] = index;
    }
    else {
        // can find an existing handle
        using std::swap;
        swap(mainWindow.selectedToolIndices[MainWindow::resolveInputHandleIndex(event)], *it);
    }
    
}


void Toolbox::processMouseLeave() {
    mouseoverToolIndex = MainWindow::EMPTY_INDEX;
}
