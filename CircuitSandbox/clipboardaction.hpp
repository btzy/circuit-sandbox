#pragma once

/**
 * Action that exposes access to the extended clipboards.
 */

#include <string>
#include <stdexcept>
#include <limits>
#include <memory>
#include <optional>
#include <SDL.h>
#include <SDL_ttf.h>
#include "point.hpp"
#include "statefulaction.hpp"
#include "eventhook.hpp"
#include "simulator.hpp"
#include "sdl_automatic.hpp"
#include "renderable.hpp"
#include "declarations.hpp"

class ClipboardAction final : public StatefulAction, public MainWindowEventHook {
private:
    constexpr static int32_t LOGICAL_HEADER_HEIGHT = 16;
    constexpr static int32_t CLIPBOARD_ROWS = 2;
    constexpr static int32_t CLIPBOARD_COLS = 2;
    constexpr static int32_t CLIPBOARDS_PER_PAGE = CLIPBOARD_ROWS * CLIPBOARD_COLS;
    constexpr static int32_t TOTAL_PAGES = (NUM_CLIPBOARDS + CLIPBOARDS_PER_PAGE - 1) / CLIPBOARDS_PER_PAGE;
    constexpr static ext::point LOGICAL_ELEMENT_PADDING{ 8, 8 };
    constexpr static ext::point LOGICAL_POPUP_PADDING{ 8, 8 };
    constexpr static ext::point LOGICAL_CLIPBOARD_BUTTON_SIZE{ 200, 200 };
    constexpr static ext::point LOGICAL_NAVIGATION_BUTTON_SIZE{ 32, LOGICAL_HEADER_HEIGHT };

    constexpr static SDL_Color backgroundColor{ 0, 0, 0, 0xFF };
    constexpr static SDL_Color foregroundColor{ 0xFF, 0xFF, 0xFF, 0xFF };

    enum class Mode {
        COPY,
        PASTE
    };
    Mode const mode;

    class ClipboardButton final : public Renderable {
    private:
        ClipboardAction& owner;
    public:
        int32_t index;

        ClipboardButton(ClipboardAction& owner, int32_t index) :owner(owner), index(index) {}

        inline void prepareTexture(SDL_Renderer* renderer, UniqueTexture& textureStore, const SDL_Color& textColor, const SDL_Color& backColor) {
            textureStore.reset(nullptr);
            SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_TARGET, renderArea.w, renderArea.h);
            SDL_Surface* surface = TTF_RenderText_Blended(owner.mainWindow.interfaceFont, std::to_string(index).c_str(), textColor);
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Texture* thumbnailTexture = owner.mainWindow.clipboard.getThumbnail(index);
            SDL_SetRenderTarget(renderer, texture);

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
            SDL_RenderClear(renderer);
            // thumbnail
            if (thumbnailTexture) {
                int32_t xPadding = 0;
                int32_t yPadding = 0;
                int32_t width;
                int32_t height;
                SDL_QueryTexture(thumbnailTexture, nullptr, nullptr, &width, &height);
                if (width * renderArea.h > height * renderArea.w) {
                    yPadding = renderArea.h - renderArea.w * height / width;
                } else {
                    xPadding = renderArea.w - renderArea.h * width / height;
                }
                const SDL_Rect targetRect{ xPadding/2, yPadding/2, renderArea.w - xPadding, renderArea.h - yPadding };
                SDL_RenderCopy(renderer, thumbnailTexture, nullptr, &targetRect);
            }
            // overlay
            {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
                SDL_SetRenderDrawColor(renderer, backColor.r, backColor.g, backColor.b, 125);
                const SDL_Rect targetRect{ 0, 0, renderArea.w, renderArea.h };
                SDL_RenderFillRect(renderer, &targetRect);
            }
            // text
            {
                const SDL_Rect targetRect{ renderArea.w / 2 - surface->w / 2, renderArea.h / 2 - surface->h / 2, surface->w, surface->h };
                SDL_RenderCopy(renderer, textTexture, nullptr, &targetRect);
            }
            // border
            {
                SDL_SetRenderDrawColor(renderer, foregroundColor.r, foregroundColor.g, foregroundColor.b, foregroundColor.a);
                const SDL_Rect targetRect{ 0, 0, renderArea.w, renderArea.h };
                SDL_RenderDrawRect(renderer, &targetRect);
            }
            SDL_SetRenderTarget(renderer, nullptr);
            SDL_FreeSurface(surface);
            SDL_DestroyTexture(textTexture);
            textureStore.reset(texture);
        }

        void layoutComponents(SDL_Renderer* renderer, const SDL_Rect& render_area) {
            renderArea = render_area;
            const SDL_Color& color = foregroundColor;
            const SDL_Color hoverColor{ color.r / 5, color.g / 5, color.b / 5, color.a };
            prepareTexture(renderer, textureDefault, color, backgroundColor);
            prepareTexture(renderer, textureHover, color, hoverColor);
            prepareTexture(renderer, textureClick, hoverColor, color);
        }
    };

    template <bool Next>
    class NavigationButton final : public Renderable {
    private:
        ClipboardAction& owner;
    public:
        NavigationButton(ClipboardAction& owner) :owner(owner) {}

        inline void prepareTexture(SDL_Renderer* renderer, UniqueTexture& textureStore, const SDL_Color& textColor, const SDL_Color& backColor) {
            textureStore.reset(nullptr);
            SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_TARGET, renderArea.w, renderArea.h);
            SDL_Surface* surface = TTF_RenderText_Shaded(owner.mainWindow.interfaceFont, Next ? ">" : "<", textColor, backColor);
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_SetRenderTarget(renderer, texture);
            {
                SDL_SetRenderDrawColor(renderer, backColor.r, backColor.g, backColor.b, backColor.a);
                SDL_RenderClear(renderer);
            }
            {
                const SDL_Rect targetRect{ renderArea.w / 2 - surface->w / 2, renderArea.h / 2 - surface->h / 2, surface->w, surface->h };
                SDL_RenderCopy(renderer, textTexture, nullptr, &targetRect);
            }
            SDL_SetRenderTarget(renderer, nullptr);
            SDL_FreeSurface(surface);
            SDL_DestroyTexture(textTexture);
            textureStore.reset(texture);
        }

        void layoutComponents(SDL_Renderer* renderer, const SDL_Rect& render_area) {
            renderArea = render_area;
            const SDL_Color& color = foregroundColor;
            const SDL_Color hoverColor{ color.r / 5, color.g / 5, color.b / 5, color.a };
            prepareTexture(renderer, textureDefault, color, backgroundColor);
            prepareTexture(renderer, textureHover, color, hoverColor);
            prepareTexture(renderer, textureClick, hoverColor, color);
        }

        template <RenderStyle style>
        void render(SDL_Renderer* renderer) const {
            if constexpr (Next) {
                if (owner.page < TOTAL_PAGES - 1) Renderable::render<style>(renderer);
            }
            else {
                if (owner.page > 0) Renderable::render<style>(renderer);
            }
        }
    };

    bool const simulatorRunning;
    int32_t page = 0;
    CanvasState selection;
    UniqueTexture dialogTexture;
    std::array<ClipboardButton, NUM_CLIPBOARDS> clipboardButtons;
    NavigationButton<true> nextButton = NavigationButton<true>(*this);
    NavigationButton<false> prevButton = NavigationButton<false>(*this);

    SDL_Rect dialogArea; // the area of the dialog proper (without the translucent background), set by layoutComponents()
    Renderable* activeButton = nullptr; // dialog button that is currently being pressed
    std::optional<ext::point> mouseLocation; // the location of the mouse on the renderArea
public:
    ClipboardAction(MainWindow& mainWindow, SDL_Renderer* renderer, Mode mode);

    ~ClipboardAction() override;

    template <typename Button>
    inline void renderButton(const Button& button, SDL_Renderer* renderer) {
        if (&button == activeButton) {
            button.template render<RenderStyle::CLICK>(renderer);
        }
        else if (activeButton == nullptr && mouseLocation && ext::point_in_rect(*mouseLocation, button.renderArea)) {
            button.template render<RenderStyle::HOVER>(renderer);
        }
        else {
            button.template render<RenderStyle::DEFAULT>(renderer);
        }
    }

    void render(SDL_Renderer* renderer) override;

    /**
     * Initialise the parameters for render()
     * Draw all the stuff that don't change (unless the layout changes)
     */
    void layoutComponents(SDL_Renderer* renderer) override;

    ActionEventResult processWindowMouseButtonDown(const SDL_MouseButtonEvent& event) override;

    ActionEventResult processWindowMouseButtonUp() override;

    ActionEventResult processWindowMouseHover(const SDL_MouseMotionEvent& event) override;

    ActionEventResult processWindowMouseLeave() override;

    ActionEventResult processWindowKeyboard(const SDL_KeyboardEvent& event) override;

    /**
     * The user has selected a clipboard. The actual clipboard action is performed here.
     */
    ActionEventResult selectClipboard(int32_t clipboardIndex);

    void prevPage();
    void nextPage();

    // note: selection passed by value, because the caller (SelectionAction) will have been destroyed by the action starter.
    static inline void startCopyDialog(MainWindow& mainWindow, SDL_Renderer* renderer, const ActionStarter& starter, CanvasState selection) {
        auto& action = starter.start<ClipboardAction>(mainWindow, renderer, Mode::COPY);
        action.selection = std::move(selection);
    }

    static inline void startPasteDialog(MainWindow& mainWindow, SDL_Renderer* renderer, const ActionStarter& starter) {
        starter.start<ClipboardAction>(mainWindow, renderer, Mode::PASTE);
    }
};
