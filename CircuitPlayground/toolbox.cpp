#include <stdexcept>
#include <type_traits>
#include <string>
#include <cstring>

#include <SDL.h>
#include <SDL_ttf.h>

#include "toolbox.hpp"
#include "mainwindow.hpp"


using namespace std::literals::string_literals; // gives the 's' suffix for strings

void Toolbox::render(SDL_Renderer* renderer, const SDL_Rect& render_area) {
    // draw a grey border around the toolbox
    SDL_SetRenderDrawColor(renderer, 0x66, 0x66, 0x66, 0xFF);
    SDL_RenderDrawRect(renderer, &render_area);

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
        button_font = TTF_OpenFont(font_path, 12);
        delete[] font_path;
    }
    if (button_font == nullptr) {
        throw std::runtime_error("TTF_OpenFont() failed:  "s + TTF_GetError());
    }


    // draw the buttons to the screen one-by-one
    element_tags::for_each([renderer, button_font, &render_area](const auto element_tag, const auto index_tag) {
        // 'Element' is the type of element (e.g. ConductiveWire)
        using Element = typename decltype(element_tag)::type;
        // 'index' is the index of this element inside the element_tags
        constexpr size_t index = decltype(index_tag)::value;

        // Render the text
        SDL_Surface* surface = TTF_RenderText_Shaded(button_font, Element::displayName, Element::displayColor, MainWindow::backgroundColor);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        int textureWidth, textureHeight;
        SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);
        const SDL_Rect destRect{render_area.x + 8, render_area.y + 8 + 20 * static_cast<int>(index), textureWidth, textureHeight};
        SDL_RenderCopy(renderer, texture, nullptr, &destRect);
        SDL_DestroyTexture(texture);
    });

    // free the font so we don't leak memory
    TTF_CloseFont(button_font);
}
