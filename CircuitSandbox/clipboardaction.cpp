#include "clipboardaction.hpp"
#include "selectionaction.hpp"

ClipboardAction::ClipboardAction(MainWindow& mainWindow, SDL_Renderer* renderer, Mode mode) : StatefulAction(mainWindow), MainWindowEventHook(mainWindow, mainWindow.getRenderArea()), mode(mode), simulatorRunning(stateManager().simulator.running()), dialogTexture(nullptr)  {
    if (simulatorRunning) stateManager().stopSimulatorUnchecked();

    for (int32_t index : mainWindow.clipboard.getOrder()) {
        clipboardButtons.push_back(ClipboardButton(*this, index));
    }

    layoutComponents(renderer);
    ext::point mousePosition;
    SDL_GetMouseState(&mousePosition.x, &mousePosition.y);
    if (ext::point_in_rect(mousePosition, renderArea)) {
        mouseLocation = mousePosition;
    }
}

ClipboardAction::~ClipboardAction() {
    if (simulatorRunning) stateManager().startSimulator();
}

void ClipboardAction::render(SDL_Renderer* renderer) {
    // draw the translucent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // set the blend mode
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0x99);
    SDL_RenderFillRect(renderer, &renderArea);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); // unset the blend mode

    // draw the dialog box
    SDL_RenderCopy(renderer, dialogTexture.get(), nullptr, &dialogArea);

    // draw the buttons
    int32_t buttons = std::min(CLIPBOARDS_PER_PAGE, static_cast<int32_t>(NUM_CLIPBOARDS - page * CLIPBOARDS_PER_PAGE));
    for (int32_t i = 0; i < buttons; ++i) {
        renderButton(clipboardButtons[i + page * CLIPBOARDS_PER_PAGE], renderer);
    }
    renderButton(prevButton, renderer);
    renderButton(nextButton, renderer);
}

void ClipboardAction::layoutComponents(SDL_Renderer* renderer) {
    dialogTexture.reset(nullptr);
    renderArea = mainWindow.getRenderArea();

    // pseudo-constants
    int32_t HEADER_HEIGHT = mainWindow.logicalToPhysicalSize(LOGICAL_HEADER_HEIGHT);
    ext::point ELEMENT_PADDING = mainWindow.logicalToPhysicalSize(LOGICAL_ELEMENT_PADDING);
    ext::point POPUP_PADDING = mainWindow.logicalToPhysicalSize(LOGICAL_POPUP_PADDING);
    ext::point CLIPBOARD_BUTTON_SIZE = mainWindow.logicalToPhysicalSize(LOGICAL_CLIPBOARD_BUTTON_SIZE);
    ext::point NAVIGATION_BUTTON_SIZE = mainWindow.logicalToPhysicalSize(LOGICAL_NAVIGATION_BUTTON_SIZE);

    ext::point dialogTextureSize = POPUP_PADDING * 2;
    dialogTextureSize.y += (ELEMENT_PADDING.y + CLIPBOARD_BUTTON_SIZE.y) * CLIPBOARD_ROWS + HEADER_HEIGHT;
    dialogTextureSize.x += (ELEMENT_PADDING.x + CLIPBOARD_BUTTON_SIZE.x) * CLIPBOARD_COLS - ELEMENT_PADDING.x;
    dialogArea = { renderArea.x + renderArea.w / 2 - dialogTextureSize.x / 2, renderArea.y + renderArea.h / 2 - dialogTextureSize.y / 2, dialogTextureSize.x, dialogTextureSize.y };
    auto texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_TARGET, dialogTextureSize.x, dialogTextureSize.y);
    SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, foregroundColor.r, foregroundColor.g, foregroundColor.b, foregroundColor.a);
    { // outer box
        const SDL_Rect target{ 0, 0, dialogTextureSize.x, dialogTextureSize.y };
        SDL_RenderDrawRect(renderer, &target);
    }

    // draw the header text
    const char* text;
    switch (mode) {
        case Mode::COPY:
            text = "Select a clipboard to copy to";
            break;
        case Mode::PASTE:
            text = "Select a clipboard to paste from";
            break;
        default:
            text = ""; // shouldn't happen
            break;
    }
    SDL_Surface* surface1 = TTF_RenderText_Shaded(mainWindow.interfaceFont, text, foregroundColor, backgroundColor);
    SDL_Texture* texture1 = SDL_CreateTextureFromSurface(renderer, surface1);
    SDL_Rect target{
        POPUP_PADDING.x,
        POPUP_PADDING.y + HEADER_HEIGHT / 2 - surface1->h / 2,
        surface1->w,
        surface1->h
    };
    SDL_RenderCopy(renderer, texture1, nullptr, &target);

    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(texture1);
    SDL_FreeSurface(surface1);

    // set clipboard button renderareas
    for (int32_t i = 0; i < NUM_CLIPBOARDS; ++i) {
        int32_t row = i % CLIPBOARDS_PER_PAGE / CLIPBOARD_COLS;
        int32_t col = i % CLIPBOARDS_PER_PAGE % CLIPBOARD_COLS;
        SDL_Rect target{
            (CLIPBOARD_BUTTON_SIZE.x + ELEMENT_PADDING.x) * col + dialogArea.x + POPUP_PADDING.x,
            (CLIPBOARD_BUTTON_SIZE.y + ELEMENT_PADDING.y) * row + dialogArea.y + POPUP_PADDING.y + HEADER_HEIGHT + ELEMENT_PADDING.y,
            CLIPBOARD_BUTTON_SIZE.x,
            CLIPBOARD_BUTTON_SIZE.y
        };
        clipboardButtons[i].layoutComponents(renderer, target);
    }

    // set navigation button renderareas
    {
        SDL_Rect target{
            dialogArea.x + dialogArea.w - POPUP_PADDING.x - NAVIGATION_BUTTON_SIZE.x,
            dialogArea.y + POPUP_PADDING.y,
            NAVIGATION_BUTTON_SIZE.x,
            NAVIGATION_BUTTON_SIZE.y
        };
        nextButton.layoutComponents(renderer, target);
        target.x -= NAVIGATION_BUTTON_SIZE.x;
        prevButton.layoutComponents(renderer, target);
    }

    dialogTexture.reset(texture);
}

ActionEventResult ClipboardAction::processWindowMouseButtonDown(const SDL_MouseButtonEvent& event) {
    if (!ext::point_in_rect(event, dialogArea)) {
        // select the default clipboard if the user clicked outside
        return selectClipboard();
    }

    int32_t buttons = std::min(CLIPBOARDS_PER_PAGE, static_cast<int32_t>(NUM_CLIPBOARDS - page * CLIPBOARDS_PER_PAGE));
    for (int32_t i = 0; i < buttons; ++i) {
        auto& button = clipboardButtons[i + page * CLIPBOARDS_PER_PAGE];
        if (ext::point_in_rect(event, button.renderArea)) {
            activeButton = &button;
            return ActionEventResult::PROCESSED;
        }
    }

    if (ext::point_in_rect(*mouseLocation, nextButton.renderArea)) {
        activeButton = &nextButton;
        return ActionEventResult::PROCESSED;
    }
    if (ext::point_in_rect(*mouseLocation, prevButton.renderArea)) {
        activeButton = &prevButton;
        return ActionEventResult::PROCESSED;
    }

    return ActionEventResult::PROCESSED;
}

ActionEventResult ClipboardAction::processWindowMouseButtonUp() {
    if (mouseLocation) {
        int32_t buttons = std::min(CLIPBOARDS_PER_PAGE, static_cast<int32_t>(NUM_CLIPBOARDS - page * CLIPBOARDS_PER_PAGE));
        for (int32_t i = 0; i < buttons; ++i) {
            auto& button = clipboardButtons[i + page * CLIPBOARDS_PER_PAGE];
            if (activeButton == &button && ext::point_in_rect(*mouseLocation, button.renderArea)) {
                activeButton = nullptr;
                return selectClipboard(button.index);
            }
        }

        if (activeButton == &nextButton && ext::point_in_rect(*mouseLocation, nextButton.renderArea)) {
            nextPage();
            activeButton = nullptr;
            return ActionEventResult::PROCESSED;
        }
        if (activeButton == &prevButton && ext::point_in_rect(*mouseLocation, prevButton.renderArea)) {
            prevPage();
            activeButton = nullptr;
            return ActionEventResult::PROCESSED;
        }
    }
    activeButton = nullptr;
    return ActionEventResult::PROCESSED;
}

ActionEventResult ClipboardAction::processWindowMouseHover(const SDL_MouseMotionEvent& event) {
    mouseLocation = event;
    return ActionEventResult::PROCESSED;
}

ActionEventResult ClipboardAction::processWindowMouseLeave() {
    mouseLocation = std::nullopt;
    activeButton = nullptr; // so that we won't trigger the button if the mouse is released outside
    return ActionEventResult::PROCESSED;
}

ActionEventResult ClipboardAction::processWindowKeyboard(const SDL_KeyboardEvent& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.keysym.sym) {
            case SDLK_ESCAPE:
                return selectClipboard();
            case SDLK_LEFT:
                prevPage();
                return ActionEventResult::PROCESSED;
            case SDLK_RIGHT:
                nextPage();
                return ActionEventResult::PROCESSED;
            case SDLK_0:
                return selectClipboard(0);
            case SDLK_1:
                return selectClipboard(1);
            case SDLK_2:
                return selectClipboard(2);
            case SDLK_3:
                return selectClipboard(3);
            case SDLK_4:
                return selectClipboard(4);
            case SDLK_5:
                return selectClipboard(5);
            case SDLK_6:
                return selectClipboard(6);
            case SDLK_7:
                return selectClipboard(7);
            case SDLK_8:
                return selectClipboard(8);
            case SDLK_9:
                return selectClipboard(9);
            default:
                break;
        }
    }
    return ActionEventResult::PROCESSED;
}

ActionEventResult ClipboardAction::selectClipboard() {
    switch (mode) {
    case Mode::COPY:
        mainWindow.clipboard.write(selection);
        return ActionEventResult::COMPLETED;
    case Mode::PASTE:
        SelectionAction::startByPasting(mainWindow, mainWindow.playArea, mainWindow.currentAction.getStarter());
        return ActionEventResult::PROCESSED;
    }
    return ActionEventResult::PROCESSED;
}

ActionEventResult ClipboardAction::selectClipboard(int32_t clipboardIndex) {
    switch (mode) {
    case Mode::COPY:
        mainWindow.clipboard.write(selection, clipboardIndex);
        return ActionEventResult::COMPLETED;
    case Mode::PASTE:
        SelectionAction::startByPasting(mainWindow, mainWindow.playArea, mainWindow.currentAction.getStarter(), clipboardIndex);
        return ActionEventResult::PROCESSED;
    }
    return ActionEventResult::PROCESSED;
}

void ClipboardAction::prevPage() {
    if (page == 0) return;
    page--;
}

void ClipboardAction::nextPage() {
    if (page == TOTAL_PAGES - 1) return;
    page++;
}
