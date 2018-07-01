#pragma once

#include <array>
#include <vector>
#include <memory>

#include <SDL.h>

#include "declarations.hpp"
#include "drawable.hpp"
#include "font.hpp"
#include "buttonbaritems.hpp"

/**
 * Represents the the strip of buttons at the bottom of the window.
 * Some of the buttons will start playarea's actions.
 */



namespace IconCodePoints {
    enum : uint16_t {
        NEW = 0xE000,
        OPEN = 0xE001,
        SAVE = 0xE002,
        PLAY = 0xE010,
        PAUSE = 0xE011,
        STEP = 0xE012,
        SPEED = 0xE018
    };
}

class ButtonBar final : public Drawable {
private:
    // owner window
    MainWindow& mainWindow;

    // play area
    PlayArea& playArea;

    // icon font
    Font iconFont;

    constexpr static SDL_Color backgroundColor{ 0, 0, 0, 0xFF };
    constexpr static SDL_Color foregroundColor{ 0xFF, 0xFF, 0xFF, 0xFF };
    constexpr static SDL_Color hoverColor{ 0x44, 0x44, 0x44, 0xFF };

    const std::array<std::unique_ptr<ButtonBarItem>, 8> items{
        std::make_unique<IconButton<IconCodePoints::NEW>>(),
        std::make_unique<IconButton<IconCodePoints::OPEN>>(),
        std::make_unique<IconButton<IconCodePoints::SAVE>>(),
        std::make_unique<ButtonBarSpace>(),
        std::make_unique<PlayPauseButton>(),
        std::make_unique<IconButton<IconCodePoints::STEP>>(),
        std::make_unique<ButtonBarSpace>(),
        std::make_unique<IconButton<IconCodePoints::SPEED>>()
    };

    ButtonBarItem* hoveredItem = nullptr;
    ButtonBarItem* clickedItem = nullptr;

    template <uint16_t>
    friend class IconButton;
    friend class PlayPauseButton;

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
     * Processing of events.
     */
    void processMouseHover(const SDL_MouseMotionEvent&) override;
    void processMouseLeave() override;

    bool processMouseButtonDown(const SDL_MouseButtonEvent&) override;
    void processMouseButtonUp() override;
};
