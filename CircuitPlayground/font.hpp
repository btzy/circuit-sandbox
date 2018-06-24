#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <cstring>
#include <SDL.h>
#include <SDL_ttf.h>

struct FontDeleter {
    void operator()(TTF_Font* ptr) const noexcept {
        TTF_CloseFont(ptr);
    }
};

class Font {
private:
    char* fontPath;
    const int logicalSize;
    std::unique_ptr<TTF_Font, FontDeleter> font;
public:
    Font(const char* fontName, int logicalSize) :logicalSize(logicalSize) {
        char* cwd = SDL_GetBasePath();
        if (cwd == nullptr) {
            using namespace std::literals::string_literals;
            throw std::runtime_error("SDL_GetBasePath() failed:  "s + SDL_GetError());
        }
        fontPath = new char[std::strlen(cwd) + std::strlen(fontName) + 1];
        std::strcpy(fontPath, cwd);
        std::strcat(fontPath, fontName);
        SDL_free(cwd);
    }

    ~Font() {
        delete[] fontPath;
    }

    template <typename MainWindow>
    void updateDPI(const MainWindow& mainWindow) {
        font.reset(nullptr); // delete the old font first
        font.reset(TTF_OpenFont(fontPath, mainWindow.logicalToPhysicalSize(logicalSize))); // make the new font
    }

    operator TTF_Font*() const {
        return font.get();
    }
};
