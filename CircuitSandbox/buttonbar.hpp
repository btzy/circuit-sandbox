#pragma once

#include <array>
#include <vector>
#include <memory>

#include <SDL.h>
#include "sdl_automatic.hpp"

#include "declarations.hpp"
#include "drawable.hpp"
#include "font.hpp"
#include "buttonbaritems.hpp"
#include "iconcodepoints.hpp"

/**
 * Represents the the strip of buttons at the bottom of the window.
 * Some of the buttons will start playarea's actions.
 */


class ButtonBar final : public Drawable {
private:
    // owner window
    MainWindow& mainWindow;

    // play area
    PlayArea& playArea;

    // icon font
    Font iconFont;

    // description texture
    UniqueTexture descriptionTexture;
    ext::point descriptionSize;
    int32_t descriptionOffset; // in physical pixels, from left of screen

    constexpr static SDL_Color backgroundColor{ 0, 0, 0, 0xFF };
    constexpr static SDL_Color foregroundColor{ 0xFF, 0xFF, 0xFF, 0xFF };
    constexpr static SDL_Color hoverColor{ 0x44, 0x44, 0x44, 0xFF };

    const std::array<std::unique_ptr<ButtonBarItem>, 12> items{
        std::make_unique<IconButton<IconCodePoints::NEW>>(),
        std::make_unique<IconButton<IconCodePoints::OPEN>>(),
        std::make_unique<IconButton<IconCodePoints::SAVE>>(),
        std::make_unique<ButtonBarSpace>(),
        std::make_unique<PlayPauseButton>(),
        std::make_unique<IconButton<IconCodePoints::RESET>>(),
        std::make_unique<StepButton>(),
        std::make_unique<ButtonBarSpace>(),
        std::make_unique<IconButton<IconCodePoints::SPEED>>(),
        std::make_unique<ButtonBarSpace>(),
        std::make_unique<UndoButton>(),
        std::make_unique<RedoButton>()
    };

    ButtonBarItem* hoveredItem = nullptr;
    ButtonBarItem* clickedItem = nullptr;

    template <uint16_t>
    friend class IconButton;
    friend class StepButton;
    friend class UndoButton;
    friend class RedoButton;
    friend class PlayPauseButton;

    SDL_Renderer* getRenderer();
    Font& getInterfaceFont();

public:
    ButtonBar(MainWindow&, PlayArea&);

    /**
     * Informs the toolbox size or dpi has changed, so the component should recalculate its contents
     */
    void layoutComponents(SDL_Renderer*) override;

    /**
     * Renders this button bar on the given area of the renderer.
     * This method is called by MainWindow
     * @pre renderer must not be null.
     */
    void render(SDL_Renderer* renderer) override {
        std::as_const(*this).render(renderer);
    }
    void render(SDL_Renderer* renderer) const;

    /**
     * Description text.
     */
    void setDescription(const char* description) {
        assert(description);
        SDL_Surface* surface = TTF_RenderText_Shaded(getInterfaceFont(), description, ButtonBar::foregroundColor, ButtonBar::backgroundColor);
        if (surface) { // if there is text to show
            descriptionTexture.reset(nullptr);
            descriptionTexture.reset(SDL_CreateTextureFromSurface(getRenderer(), surface));
            descriptionSize = { surface->w, surface->h };
            SDL_FreeSurface(surface);
        }
    }
    void clearDescription() {
        descriptionTexture = nullptr;
    }

    /**
     * Processing of events.
     */
    void processMouseHover(const SDL_MouseMotionEvent&) override;
    void processMouseLeave() override;

    bool processMouseButtonDown(const SDL_MouseButtonEvent&) override;
    void processMouseButtonUp() override;
};
