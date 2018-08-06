#pragma once

/**
 * Action that allows the user to change the simulation speed.
 */

#include <string>
#include <stdexcept>
#include <limits>
#include <memory>
#include <optional>
#include <SDL.h>
#include <SDL_ttf.h>
#include "unicode.hpp"
#include "point.hpp"
#include "statefulaction.hpp"
#include "eventhook.hpp"
#include "simulator.hpp"
#include "font.hpp"
#include "sdl_automatic.hpp"
#include "sdl_surface_create.hpp"
#include "renderable.hpp"
#include "drawing.hpp"

class ChangeSimulationSpeedAction final : public StatefulAction, public MainWindowEventHook {
private:
    constexpr static ext::point LOGICAL_TEXTBOX_SIZE{ 200, 32 };
    constexpr static ext::point LOGICAL_PADDING{ 8, 8 };
    constexpr static ext::point LOGICAL_TEXT_PADDING{ 4, 4 };
    constexpr static int32_t LOGICAL_BUTTON_HEIGHT = 24;
    constexpr static SDL_Color backgroundColor = BLACK;
    constexpr static SDL_Color foregroundColor = WHITE;
    constexpr static SDL_Color detailColor{ 0x99, 0x99, 0x99, 0xFF };
    constexpr static SDL_Color inputColor = YELLOW;
    constexpr static SDL_Color okayColor = GREEN;
    constexpr static SDL_Color cancelColor = RED;

    template <bool Okay>
    class DialogButton final : public Renderable {
    private:
        ChangeSimulationSpeedAction& owner;
    public:
        DialogButton(ChangeSimulationSpeedAction& owner) :owner(owner) {}
        inline void drawButton(SDL_Renderer* renderer, UniqueTexture& textureStore, const SDL_Color& textColor, const SDL_Color& backColor) {
            textureStore.reset(nullptr);

            SDL_Surface* surface = create_surface(renderArea.w, renderArea.h);

            // clear surface with background color
            SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, backColor.r, backColor.g, backColor.b, backColor.a));

            // draw text
            {
                SDL_Surface* textSurface = TTF_RenderText_Shaded(owner.mainWindow.interfaceFont, Okay ? "OK" : "Cancel", textColor, backColor);
                SDL_Rect targetRect{ renderArea.w / 2 - textSurface->w / 2, renderArea.h / 2 - textSurface->h / 2, textSurface->w, textSurface->h };
                SDL_BlitSurface(textSurface, nullptr, surface, &targetRect);
                SDL_FreeSurface(textSurface);
            }

            // draw rectangle border
            ext::drawBorder(surface, 0, 0, renderArea.w, renderArea.h, owner.mainWindow.logicalToPhysicalSize(1), foregroundColor);

            // create and save the texture
            textureStore.reset(SDL_CreateTextureFromSurface(renderer, surface));
            SDL_FreeSurface(surface);
        }
        void layoutComponents(SDL_Renderer* renderer, const SDL_Rect& render_area) {
            renderArea = render_area;
            const SDL_Color& color = Okay ? okayColor : cancelColor;
            const SDL_Color hoverColor{ color.r / 5, color.g / 5, color.b / 5, color.a };
            drawButton(renderer, textureDefault, color, backgroundColor);
            drawButton(renderer, textureHover, color, hoverColor);
            drawButton(renderer, textureClick, hoverColor, color);
        }
    };

    std::string text;
    bool const simulatorRunning;
    Font inputFont;
    UniqueTexture dialogTexture;
    DialogButton<true> okayButton; // the 'OK' dialog button
    DialogButton<false> cancelButton; // the 'Cancel' dialog button
    ext::point topLeftOffset, textSize; // the top-left and bottom-right offsets for rendering the text, set by layoutComponents()
    SDL_Rect dialogArea; // the area of the dialog proper (without the translucent background), set by layoutComponents()
    Renderable* activeButton = nullptr; // dialog button that is currently being pressed
    std::optional<ext::point> mouseLocation; // the location of the mouse on the renderArea
public:
    ChangeSimulationSpeedAction(MainWindow& mainWindow, SDL_Renderer* renderer) : StatefulAction(mainWindow), MainWindowEventHook(mainWindow, mainWindow.getRenderArea()), simulatorRunning(stateManager().simulator.running()), inputFont("OpenSans-Bold.ttf", 16), dialogTexture(nullptr), okayButton(*this), cancelButton(*this) {
        if (simulatorRunning) stateManager().stopSimulatorUnchecked();
        layoutComponents(renderer);
        text = mainWindow.displayedSimulationFPS;
        SDL_StartTextInput();
        ext::point mousePosition;
        SDL_GetMouseState(&mousePosition.x, &mousePosition.y);
        if (ext::point_in_rect(mousePosition, renderArea)) {
            mouseLocation = mousePosition;
        }
    }

    ~ChangeSimulationSpeedAction() override {
        SDL_StopTextInput();
        if (simulatorRunning) stateManager().startSimulator();
    }

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

    void render(SDL_Renderer* renderer, Drawable::RenderClock::time_point) override {
        // draw the translucent background
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // set the blend mode
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0x99);
        SDL_RenderFillRect(renderer, &renderArea);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); // unset the blend mode

        // draw the dialog box and buttons
        SDL_RenderCopy(renderer, dialogTexture.get(), nullptr, &dialogArea);
        renderButton(okayButton, renderer);
        renderButton(cancelButton, renderer);

        // draw the text
        {
            SDL_Surface* surface = TTF_RenderText_Shaded(inputFont, text.c_str(), inputColor, backgroundColor);
            int32_t cursorWidth = mainWindow.logicalToPhysicalSize(2);
            int32_t cursorLeft;
            if (surface == nullptr) { // happens when text has zero width
                cursorLeft = topLeftOffset.x;
            }
            else {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                if (surface->w + cursorWidth <= textSize.x) {
                    // can fit into the space available
                    const SDL_Rect target{ topLeftOffset.x, topLeftOffset.y + (textSize.y - surface->h) / 2, surface->w, surface->h };
                    SDL_RenderCopy(renderer, texture, nullptr, &target);
                    cursorLeft = topLeftOffset.x + surface->w;
                }
                else {
                    // cannot fit, so we scroll to right
                    const SDL_Rect target{ topLeftOffset.x + textSize.x - surface->w - cursorWidth, topLeftOffset.y + (textSize.y - surface->h) / 2, surface->w, surface->h };
                    const SDL_Rect clipRect{ topLeftOffset.x, topLeftOffset.y, textSize.x, textSize.y };
                    SDL_Rect oldClipRect;
                    SDL_RenderGetClipRect(renderer, &oldClipRect);
                    SDL_RenderSetClipRect(renderer, &clipRect);
                    SDL_RenderCopy(renderer, texture, nullptr, &target);
                    SDL_RenderSetClipRect(renderer, &oldClipRect);
                    cursorLeft = topLeftOffset.x + textSize.x - cursorWidth;
                }
                SDL_FreeSurface(surface);
                SDL_DestroyTexture(texture);
            }
            {
                // render the cursor (caret)
                const SDL_Rect target{ cursorLeft, topLeftOffset.y, cursorWidth, textSize.y };
                SDL_SetRenderDrawColor(renderer, inputColor.r, inputColor.g, inputColor.b, inputColor.a);
                SDL_RenderFillRect(renderer, &target);
            }
        }
    }

    void layoutComponents(SDL_Renderer* renderer) override {
        dialogTexture.reset(nullptr);
        renderArea = mainWindow.getRenderArea();
        inputFont.updateDPI(mainWindow);

        // pseudo-constants
        ext::point TEXTBOX_SIZE = mainWindow.logicalToPhysicalSize(LOGICAL_TEXTBOX_SIZE);
        ext::point PADDING = mainWindow.logicalToPhysicalSize(LOGICAL_PADDING);
        ext::point TEXT_PADDING = mainWindow.logicalToPhysicalSize(LOGICAL_TEXT_PADDING);
        int32_t BUTTON_HEIGHT = mainWindow.logicalToPhysicalSize(LOGICAL_BUTTON_HEIGHT);

        // draw all the stuff that don't change (unless the layout changes)
        SDL_Surface* surface1 = TTF_RenderText_Shaded(mainWindow.interfaceFont, "Enter simulation speed (FPS):", foregroundColor, backgroundColor);
        SDL_Surface* surface2 = TTF_RenderText_Shaded(mainWindow.interfaceFont, "(0 = as fast as possible)", detailColor, backgroundColor);
        
        ext::point textureSize = TEXTBOX_SIZE + PADDING * 2;
        textureSize.y += surface1->h + surface2->h + PADDING.y * 2 + BUTTON_HEIGHT;
        dialogArea = { renderArea.x + renderArea.w / 2 - textureSize.x / 2, renderArea.y + renderArea.h / 2 - textureSize.y / 2, textureSize.x, textureSize.y };

        SDL_Surface* surface = create_surface(textureSize.x, textureSize.y);
        
        // fill background
        SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a));
        //SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 255,255,255,255));
        
        // outer box
        ext::drawBorder(surface, 0, 0, textureSize.x, textureSize.y, mainWindow.logicalToPhysicalSize(1), foregroundColor);

        // first line of text
        {
            SDL_Rect target{ PADDING.x, PADDING.y, surface1->w, surface1->h };
            SDL_BlitSurface(surface1, nullptr, surface, &target);
        }

        // second line of text
        {
            SDL_Rect target{ PADDING.x, PADDING.y + surface1->h, surface2->w, surface2->h };
            SDL_BlitSurface(surface2, nullptr, surface, &target);
        }

        // text box outline
        {
            const SDL_Rect target{ PADDING.x, PADDING.y * 2 + surface1->h + surface2->h, TEXTBOX_SIZE.x, TEXTBOX_SIZE.y };
            ext::drawBorder(surface, target, mainWindow.logicalToPhysicalSize(1), foregroundColor);
        }

        // save the offsets so that the render() function can do its job
        ext::point textBoxTopLeftOffset = { renderArea.x + renderArea.w / 2 - textureSize.x / 2 + PADDING.x, renderArea.y + renderArea.h / 2 - textureSize.y / 2 + PADDING.y * 2 + surface1->h + surface2->h };
        topLeftOffset = textBoxTopLeftOffset + TEXT_PADDING;
        textSize = TEXTBOX_SIZE - TEXT_PADDING * 2;

        // free the surfaces here (because topLeftOffset calculation needs data from the surfaces)
        SDL_FreeSurface(surface1);
        SDL_FreeSurface(surface2);

        // set the buttons' renderareas
        int32_t buttonWidth = (TEXTBOX_SIZE.x - PADDING.x) / 2;
        int32_t buttonY = textBoxTopLeftOffset.y + TEXTBOX_SIZE.y + PADDING.y;
        okayButton.layoutComponents(renderer, SDL_Rect{ textBoxTopLeftOffset.x, buttonY, buttonWidth, BUTTON_HEIGHT });
        cancelButton.layoutComponents(renderer, SDL_Rect{ textBoxTopLeftOffset.x + TEXTBOX_SIZE.x - buttonWidth, buttonY, buttonWidth, BUTTON_HEIGHT });

        dialogTexture.reset(SDL_CreateTextureFromSurface(renderer, surface));
        SDL_FreeSurface(surface);
    }

    ActionEventResult processWindowMouseButtonDown(const SDL_MouseButtonEvent& event) override {
        if (!ext::point_in_rect(event, dialogArea)) {
            // escape the dialog if the user clicked outside
            return ActionEventResult::COMPLETED;
        }
        if (ext::point_in_rect(event, okayButton.renderArea)) {
            activeButton = &okayButton;
        }
        else if (ext::point_in_rect(event, cancelButton.renderArea)) {
            activeButton = &cancelButton;
        }
        return ActionEventResult::PROCESSED;
    }

    ActionEventResult processWindowMouseButtonUp() override {
        if (mouseLocation) {
            if (&okayButton == activeButton && ext::point_in_rect(*mouseLocation, okayButton.renderArea)) {
                // user clicked okay
                activeButton = nullptr;
                return commit();
            }
            else if (&cancelButton == activeButton && ext::point_in_rect(*mouseLocation, cancelButton.renderArea)) {
                // user clicked cancel
                activeButton = nullptr;
                return ActionEventResult::COMPLETED;
            }
        }
        activeButton = nullptr;
        return ActionEventResult::PROCESSED;
    }

    ActionEventResult processWindowMouseHover(const SDL_MouseMotionEvent& event) override {
        mouseLocation = event;
        return ActionEventResult::PROCESSED;
    }
    ActionEventResult processWindowMouseLeave() override {
        mouseLocation = std::nullopt;
        activeButton = nullptr; // so that we won't trigger the button if the mouse is released outside
        return ActionEventResult::PROCESSED;
    }

    ActionEventResult processWindowKeyboard(const SDL_KeyboardEvent& event) override {
        if (event.type == SDL_KEYDOWN) {
            switch (event.keysym.sym) {
            case SDLK_ESCAPE:
                return ActionEventResult::COMPLETED;
            case SDLK_BACKSPACE: [[fallthrough]];
            case SDLK_KP_BACKSPACE:
                if (!text.empty()) text.pop_back();
                return ActionEventResult::PROCESSED;
            case SDLK_RETURN: [[fallthrough]];
            case SDLK_RETURN2: [[fallthrough]];
            case SDLK_KP_ENTER:
                return commit();
            default:
                break;
            }
        }
        return ActionEventResult::PROCESSED;
    }

    ActionEventResult processWindowTextInput(const SDL_TextInputEvent& event) override {
        // keep only the characters (UTF-8 code points) that are 0-9 or '.' or ','
        ext::utf8_foreach(event.text, [&](const char* begin, size_t length) {
            if (length == 1) {
                if (*begin >= '0' && *begin <= '9') {
                    text += *begin;
                }
                else if (*begin == '.' || *begin == ',') {
                    text += '.';
                }
            }
        });
        return ActionEventResult::PROCESSED;
    }

    inline ActionEventResult commit() {
        // try to parse and save the fps of the simulator
        try {
            size_t len;
            long double fps = std::stold(text, &len); // note that stold throws on empty string
            if (len != text.size()) {
                throw std::logic_error("");
            }
            Simulator::period_t period = MainWindow::getSimulatorPeriodFromFPS(fps);
            stateManager().simulator.setPeriod(period);
            std::string fpsText;
            if (fps == 0.0) {
                fpsText = "max";
                mainWindow.displayedSimulationFPS = "0";
            }
            else {
                // truncate trailing zeros and decimal point
                fpsText = std::to_string(fps);
                fpsText.erase(fpsText.find_last_not_of('0') + 1, std::string::npos);
                fpsText.erase(fpsText.find_last_not_of('.') + 1, std::string::npos);
                mainWindow.displayedSimulationFPS = fpsText;
            }
            NotificationDisplay::Data notificationData{
                { "Simulation speed set to "s, NotificationDisplay::TEXT_COLOR },
                { fpsText, NotificationDisplay::TEXT_COLOR_KEY },
                { " fps", NotificationDisplay::TEXT_COLOR },
            };
            mainWindow.changeSpeedNotification = mainWindow.notificationDisplay.uniqueAdd(NotificationFlags::DEFAULT, 5s, std::move(notificationData));
            return ActionEventResult::COMPLETED;
        }
        catch (const std::logic_error&) {
            mainWindow.changeSpeedNotification = mainWindow.notificationDisplay.uniqueAdd(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "Invalid input: Entered FPS is not a number", NotificationDisplay::TEXT_COLOR_ERROR } });
            return ActionEventResult::CANCELLED;
        }
    }

    static inline void start(MainWindow& mainWindow, SDL_Renderer* renderer, const ActionStarter& starter) {
        starter.start<ChangeSimulationSpeedAction>(mainWindow, renderer);
    }
};
