#pragma once

#include <array>
#include <memory>

#include <SDL.h>
#include "sdl_automatic.hpp"

#include "declarations.hpp"
#include "control.hpp"
#include "font.hpp"
#include "buttonbaritems.hpp"
#include "iconcodepoints.hpp"

/**
 * Represents the the strip of buttons at the bottom of the window.
 * Some of the buttons will start playarea's actions.
 */


class ButtonBar final : public Control {
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

    constexpr static SDL_Color backgroundColor = DARK_GREY;
    constexpr static SDL_Color foregroundColor = WHITE;
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

    template <size_t Index, typename Tuple>
    inline void _setDescriptionSize(ext::point& size, Tuple& arr) {
        if (std::get<Index>(arr) != nullptr) {
            size.x += std::get<Index>(arr)->w;
            size.y = std::max(size.y, std::get<Index>(arr)->h);
        }
        if constexpr (Index + 1 < std::tuple_size_v<Tuple>) {
            _setDescriptionSize<Index + 1>(size, arr);
        }
    }
    template <size_t Index, typename Tuple, typename... Args>
    inline void _setDescriptionSurface(Tuple& arr, const char* description, SDL_Color color = ButtonBar::foregroundColor, Args&&... args) {
        std::get<Index>(arr) = TTF_RenderText_Shaded(getInterfaceFont(), description, color, ButtonBar::backgroundColor);
        if constexpr(sizeof...(Args) > 0) {
            _setDescriptionSurface<Index + 1>(arr, std::forward<Args>(args)...);
        }
    }
    template <size_t Index, typename Tuple>
    inline void _setDescriptionCopyAndFree(SDL_Surface* surface, Tuple& arr, int32_t offsetWidth) {
        if (std::get<Index>(arr) != nullptr) {
            SDL_Rect target{ offsetWidth, 0, std::get<Index>(arr)->w, std::get<Index>(arr)->h };
            SDL_BlitSurface(std::get<Index>(arr), nullptr, surface, &target);
            offsetWidth += std::get<Index>(arr)->w;
            SDL_FreeSurface(std::get<Index>(arr));
        }
        if constexpr (Index + 1 < std::tuple_size_v<Tuple>) {
            _setDescriptionCopyAndFree<Index + 1>(surface, arr, offsetWidth);
        }
    }

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
     * Set or clear description text.
     * setDescription() has the ability to set text in multiple colours.
     */
    template <typename... Args>
    void setDescription(const char* description, SDL_Color color = ButtonBar::foregroundColor, Args&&... args) {
        assert(description);
        if constexpr (sizeof...(args) > 0) {
            std::array<SDL_Surface*, (sizeof...(args) + 3) / 2> surfaces;
            _setDescriptionSurface<0>(surfaces, description, color, std::forward<Args>(args)...);
            descriptionSize = ext::point::zero();
            _setDescriptionSize<0>(descriptionSize, surfaces);
            if (descriptionSize.x != 0) {
                SDL_Surface* surface = SDL_CreateRGBSurface(0, descriptionSize.x, descriptionSize.y, 32, 0, 0, 0, 0);
                _setDescriptionCopyAndFree<0>(surface, surfaces, 0);
                descriptionTexture.reset(nullptr);
                descriptionTexture.reset(SDL_CreateTextureFromSurface(getRenderer(), surface));
                SDL_FreeSurface(surface);
            }
        }
        else {
            SDL_Surface* surface = TTF_RenderText_Shaded(getInterfaceFont(), description, color, ButtonBar::backgroundColor);
            if (surface) { // if there is text to show
                descriptionTexture.reset(nullptr);
                descriptionTexture.reset(SDL_CreateTextureFromSurface(getRenderer(), surface));
                descriptionSize = { surface->w, surface->h };
                SDL_FreeSurface(surface);
            }
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
