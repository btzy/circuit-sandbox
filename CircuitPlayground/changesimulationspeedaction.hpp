#pragma once

/**
 * Action that allows the user to change the simulation speed.
 */

#include <string>
#include <stdexcept>
#include <limits>
#include <memory>
#include <sstream> // for to_string_with_precision
#include <iomanip> // for to_string_with_precision
#include <SDL.h>
#include "unicode.hpp"
#include "point.hpp"
#include "statefulaction.hpp"
#include "eventhook.hpp"
#include "simulator.hpp"
#include "font.hpp"
#include "sdl_deleters.hpp"

class ChangeSimulationSpeedAction final : public StatefulAction, public MainWindowEventHook {
private:
    std::string text;
    bool const simulatorRunning;
    constexpr static int LOGICAL_WIDTH = 200;
    constexpr static int LOGICAL_HEIGHT = 32;
    constexpr static int PADDING = 8;
    constexpr static SDL_Color backgroundColor{ 0, 0, 0, 0xFF };
    constexpr static SDL_Color foregroundColor{ 0xFF, 0xFF, 0xFF, 0xFF };
    constexpr static SDL_Color detailColor{ 0x99, 0x99, 0x99, 0xFF };
    constexpr static SDL_Color inputColor{ 0xFF, 0xFF, 0x66, 0xFF };
    Font inputFont;
    std::unique_ptr<SDL_Texture, TextureDeleter> dialogTexture;
    ext::point topLeftOffset, textSize; // the top-left and bottom-right offsets for rendering the text
public:
    ChangeSimulationSpeedAction(MainWindow& mainWindow, SDL_Renderer* renderer) : StatefulAction(mainWindow), MainWindowEventHook(mainWindow, mainWindow.getRenderArea()), simulatorRunning(stateManager().simulator.running()), inputFont("OpenSans-Bold.ttf", 16), dialogTexture(nullptr) {
        if (simulatorRunning) stateManager().stopSimulatorUnchecked();
        updateDpi(renderer);
        text = mainWindow.displayedSimulationFPS;
        SDL_StartTextInput();
    }

    ~ChangeSimulationSpeedAction() override {
        SDL_StopTextInput();
        if (simulatorRunning) stateManager().startSimulator();
    }

    void render(SDL_Renderer* renderer) override {
        // draw the translucent background
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // set the blend mode
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0x99);
        SDL_RenderFillRect(renderer, &renderArea);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); // unset the blend mode

        // draw the window and box
        {
            ext::point textureSize;
            SDL_QueryTexture(dialogTexture.get(), nullptr, nullptr, &textureSize.x, &textureSize.y); 
            const SDL_Rect target{ renderArea.x + renderArea.w / 2 - textureSize.x / 2, renderArea.y + renderArea.h / 2 - textureSize.y / 2, textureSize.x, textureSize.y };
            SDL_RenderCopy(renderer, dialogTexture.get(), nullptr, &target);
        }
        
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

    void updateDpi(SDL_Renderer* renderer) override {
        dialogTexture.reset(nullptr);
        renderArea = mainWindow.getRenderArea();
        inputFont.updateDPI(mainWindow);

        // draw all the stuff that don't change (unless the DPI changes)
        SDL_Surface* surface1 = TTF_RenderText_Shaded(mainWindow.interfaceFont, "Enter FPS:", foregroundColor, backgroundColor);
        SDL_Surface* surface2 = TTF_RenderText_Shaded(mainWindow.interfaceFont, "(0 = as fast as possible)", detailColor, backgroundColor);
        SDL_Texture* texture1 = SDL_CreateTextureFromSurface(renderer, surface1);
        SDL_Texture* texture2 = SDL_CreateTextureFromSurface(renderer, surface2);
        ext::point textureSize{ mainWindow.logicalToPhysicalSize(LOGICAL_WIDTH + 2 * PADDING), mainWindow.logicalToPhysicalSize(LOGICAL_HEIGHT + 2 * PADDING) + surface1->h + surface2->h };
        auto texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_TARGET, textureSize.x, textureSize.y);
        SDL_SetRenderTarget(renderer, texture);
        SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, foregroundColor.r, foregroundColor.g, foregroundColor.b, foregroundColor.a);
        auto real_padding = mainWindow.logicalToPhysicalSize(PADDING);
        { // outer box
            const SDL_Rect target{ 0, 0, textureSize.x, textureSize.y };
            SDL_RenderDrawRect(renderer, &target);
        }
        { // first line of text
            const SDL_Rect target{ real_padding, real_padding, surface1->w, surface1->h };
            SDL_RenderCopy(renderer, texture1, nullptr, &target);
        }
        { // second line of text
            const SDL_Rect target{ real_padding, real_padding + surface1->h, surface2->w, surface2->h };
            SDL_RenderCopy(renderer, texture2, nullptr, &target);
        }
        { // text box outline
            const SDL_Rect target{ real_padding, real_padding + surface1->h + surface2->h, mainWindow.logicalToPhysicalSize(LOGICAL_WIDTH), mainWindow.logicalToPhysicalSize(LOGICAL_HEIGHT) };
            SDL_RenderDrawRect(renderer, &target);
        }
        SDL_SetRenderTarget(renderer, nullptr);
        SDL_DestroyTexture(texture1);
        SDL_DestroyTexture(texture2);
        SDL_FreeSurface(surface1);
        SDL_FreeSurface(surface2);

        // save the offsets so that the render() function can do its job
        topLeftOffset = { renderArea.x + renderArea.w / 2 - mainWindow.logicalToPhysicalSize(LOGICAL_WIDTH) / 2, renderArea.y + renderArea.h / 2 - textureSize.y / 2 + textureSize.y - mainWindow.logicalToPhysicalSize(LOGICAL_HEIGHT + PADDING) };
        textSize = { mainWindow.logicalToPhysicalSize(LOGICAL_WIDTH), mainWindow.logicalToPhysicalSize(LOGICAL_HEIGHT) };
        topLeftOffset += {4, 4};
        textSize -= {8, 8};
        dialogTexture.reset(texture);
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
                // try to parse and save the fps of the simulator

                try {
                    size_t len;
                    long double fps = std::stold(text, &len);
                    if (len != text.size()) {
                        throw std::logic_error("Invalid trailing characters!");
                    }
                    Simulator::period_t period = MainWindow::geSimulatorPeriodFromFPS(fps);
                    mainWindow.displayedSimulationFPS = std::move(text);
                    stateManager().simulator.setPeriod(period);
                    return ActionEventResult::COMPLETED;
                }
                catch (const std::logic_error&) {} // from std::stold or other throw clauses
                return ActionEventResult::PROCESSED;
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

    static inline void start(MainWindow& mainWindow, SDL_Renderer* renderer, const ActionStarter& starter) {
        starter.start<ChangeSimulationSpeedAction>(mainWindow, renderer);
    }
};
