#pragma once

#include <memory>
#include <cstdint>
#include <SDL.h>
#include "point.hpp"
#include "sdl_deleters.hpp"
#include "renderable.hpp"

class ButtonBar;

class ButtonBarItem {
public:
    virtual void render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const {};
    virtual int32_t width() const = 0;
    virtual void setHeight(SDL_Renderer* renderer, const ButtonBar& buttonBar, int32_t height) = 0;
    virtual void click(ButtonBar& buttonBar) {}
};

template <uint16_t CodePoint>
class IconButton final : public ButtonBarItem {
private:
    std::unique_ptr<SDL_Texture, TextureDeleter> textureDefault;
    std::unique_ptr<SDL_Texture, TextureDeleter> textureHover;
    std::unique_ptr<SDL_Texture, TextureDeleter> textureClick;
    int32_t _length;
public:
    void render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const override;
    int32_t width() const override {
        return _length;
    }
    void setHeight(SDL_Renderer* renderer, const ButtonBar& buttonBar, int32_t height) override;
    void click(ButtonBar& buttonBar) override;
};

class PlayPauseButton final : public ButtonBarItem {
private:
    std::unique_ptr<SDL_Texture, TextureDeleter> textureDefault[2];
    std::unique_ptr<SDL_Texture, TextureDeleter> textureHover[2];
    std::unique_ptr<SDL_Texture, TextureDeleter> textureClick[2];
    int32_t _length;
public:
    void render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const override;
    int32_t width() const override {
        return _length;
    }
    void setHeight(SDL_Renderer* renderer, const ButtonBar& buttonBar, int32_t height) override;
    void click(ButtonBar& buttonBar) override;
};

class ButtonBarSpace final : public ButtonBarItem {
private:
    int32_t _length;
public:
    int32_t width() const override {
        return _length;
    }
    void setHeight(SDL_Renderer*, const ButtonBar&, int32_t height) override {
        _length = height / 2;
    }
};
