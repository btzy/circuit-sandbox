#pragma once

#include <memory>
#include <cstdint>
#include <SDL.h>
#include "point.hpp"
#include "sdl_automatic.hpp"
#include "renderable.hpp"
#include "iconcodepoints.hpp"

class ButtonBar;

class ButtonBarItem {
public:
    virtual void render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const {};
    virtual int32_t width() const = 0;
    virtual void setHeight(SDL_Renderer* renderer, const ButtonBar& buttonBar, int32_t height) = 0;
    virtual void click(ButtonBar& buttonBar) {}
    virtual const char* description(const ButtonBar& buttonBar) const { return nullptr; };
};

template <uint16_t CodePoint>
class IconButton : public ButtonBarItem {
private:
    UniqueTexture textureDefault;
    UniqueTexture textureHover;
    UniqueTexture textureClick;
    int32_t _length;
public:
    void render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const override;
    int32_t width() const override {
        return _length;
    }
    void setHeight(SDL_Renderer* renderer, const ButtonBar& buttonBar, int32_t height) override;
    void click(ButtonBar& buttonBar) override;
    const char* description(const ButtonBar& buttonBar) const override;
};

class StepButton final : public IconButton<IconCodePoints::STEP> {
    void click(ButtonBar& buttonBar) override;
    void render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const override;
    const char* description(const ButtonBar& buttonBar) const override;
};

class UndoButton final : public IconButton<IconCodePoints::UNDO> {
    void click(ButtonBar& buttonBar) override;
    void render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const override;
    const char* description(const ButtonBar& buttonBar) const override;
};

class RedoButton final : public IconButton<IconCodePoints::REDO> {
    void click(ButtonBar& buttonBar) override;
    void render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const override;
    const char* description(const ButtonBar& buttonBar) const override;
};

class PlayPauseButton final : public ButtonBarItem {
private:
    UniqueTexture textureDefault[2];
    UniqueTexture textureHover[2];
    UniqueTexture textureClick[2];
    int32_t _length;
public:
    void render(SDL_Renderer* renderer, const ButtonBar& buttonBar, const ext::point& offset, RenderStyle style) const override;
    int32_t width() const override {
        return _length;
    }
    void setHeight(SDL_Renderer* renderer, const ButtonBar& buttonBar, int32_t height) override;
    void click(ButtonBar& buttonBar) override;
    const char* description(const ButtonBar& buttonBar) const override;
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
