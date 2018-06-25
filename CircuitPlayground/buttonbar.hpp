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
        STEP = 0xE012
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

    std::array<std::unique_ptr<ButtonBarItem>, 7> items{
        std::make_unique<IconButton<IconCodePoints::NEW>>(),
        std::make_unique<IconButton<IconCodePoints::OPEN>>(),
        std::make_unique<IconButton<IconCodePoints::SAVE>>(),
        std::make_unique<ButtonBarSpace>(),
        std::make_unique<IconButton<IconCodePoints::PLAY>>(),
        std::make_unique<IconButton<IconCodePoints::PAUSE>>(),
        std::make_unique<IconButton<IconCodePoints::STEP>>()
    };

    ButtonBarItem* hoveredItem;
    ButtonBarItem* clickedItem;

    template <uint16_t>
    friend class IconButton;

public:
    ButtonBar(MainWindow&, PlayArea&);

    /**
     * Update the dpi fields (currently a no-op).
     */
    void updateDpiFields() {}

    /**
     * Informs the toolbox that the dpi has been updated, so the toolbox should set its physical unit fields.
     */
    void updateDpi(SDL_Renderer*);

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

    void processMouseButtonDown(const SDL_MouseButtonEvent&) override;
    void processMouseButtonUp() override;
};
