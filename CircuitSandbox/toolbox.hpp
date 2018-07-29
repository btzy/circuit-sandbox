#pragma once

/**
 * Represents the toolbox - the place where all the element buttons are located.
 */

#include <utility>
#include <tuple>

#include <SDL.h>

#include "declarations.hpp"
#include "control.hpp"
#include "renderable.hpp"


class Toolbox final : public Control {
private:
    constexpr static SDL_Color clickColor{ 0x66, 0x66, 0x66, 0xFF };
    constexpr static SDL_Color hoverColor{ 0x44, 0x44, 0x44, 0xFF };
    constexpr static SDL_Color backgroundColor{ 0, 0, 0, 0xFF };

    // logical units
    constexpr static int LOGICAL_BUTTON_HEIGHT = 24;
    constexpr static int LOGICAL_PADDING_HORIZONTAL = 8;
    constexpr static int LOGICAL_PADDING_VERTICAL = 8;
    constexpr static int LOGICAL_BUTTON_PADDING = 2;
    constexpr static int LOGICAL_BUTTON_SPACING = 2;

    // physical units
    int BUTTON_HEIGHT = LOGICAL_BUTTON_HEIGHT;
    int PADDING_HORIZONTAL = LOGICAL_PADDING_HORIZONTAL;
    int PADDING_VERTICAL = LOGICAL_PADDING_VERTICAL;
    int BUTTON_PADDING = LOGICAL_BUTTON_PADDING;
    int BUTTON_SPACING = LOGICAL_BUTTON_SPACING;

    template <typename Tool>
    class ToolRenderable : public Renderable {
    private:
        Toolbox& owner;
        constexpr static int LOGICAL_TEXT_LEFT_PADDING = 4;
    public:
        ToolRenderable(Toolbox& owner) noexcept;
        void prepareTexture(SDL_Renderer* renderer, UniqueTexture& textureStore, const SDL_Color& backColor);
        void layoutComponents(SDL_Renderer* renderer, const SDL_Rect& render_area);
    };

    // owner window
    MainWindow& mainWindow;

    // the index of the element being mouseovered
    size_t mouseoverToolIndex;

    // the index of the element being clicked
    size_t mouseclickToolIndex;
    // the index of the input handle used to click (not used if mouseclickToolIndex == MainWindow::EMPTY_INDEX
    size_t mouseclickInputHandle;

    // cached buttons (renderables)
    using tool_buttons_t = tool_tags_t::transform<ToolRenderable>::instantiate<std::tuple>;
    tool_buttons_t toolButtons;

    /**
     * The colour of the each handle
     */
    constexpr static SDL_Color selectorHandleColors[NUM_INPUT_HANDLES] = {
        { 0xFF, 0xFF, 0, SDL_ALPHA_OPAQUE },
        { 0xFF, 0, 0, SDL_ALPHA_OPAQUE },
        { 0, 0xFF, 0, SDL_ALPHA_OPAQUE },
        { 0x55, 0x55, 0xFF, SDL_ALPHA_OPAQUE },
        { 0, 0xCC, 0xFF, SDL_ALPHA_OPAQUE },
        { 0xCC, 0, 0xFF, SDL_ALPHA_OPAQUE }
    };

    template <size_t Index>
    inline void renderButton(SDL_Renderer* renderer) const;

public:

    Toolbox(MainWindow&);

    /**
     * Informs the toolbox that the dpi has been updated, so the toolbox should set its physical unit fields.
     */
    void layoutComponents(SDL_Renderer*) override;

    /**
     * Renders this toolbox on the given area of the renderer.
     * This method is called by MainWindow
     * @pre renderer must not be null.
     */
    void render(SDL_Renderer* renderer, Drawable::RenderClock::time_point) override {
        std::as_const(*this).render(renderer);
    }
    void render(SDL_Renderer* renderer) const;

    /**
     * Processing of events.
     */
    void processMouseHover(const SDL_MouseMotionEvent&) override;
    bool processMouseButtonDown(const SDL_MouseButtonEvent&) override;
    void processMouseButtonUp() override;

    /**
     * This is called to reset all the hovering of buttons, because mousemove events are only triggered when the mouse is still on the canvas
     */
    void processMouseLeave() override;
};

