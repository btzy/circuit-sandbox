#include <stdexcept>
#include <cstring>
#include <numeric>
#include <algorithm>

#include <SDL.h>
#include <SDL_ttf.h>

#include <iostream>

#include "reverse_adaptor.hpp"
#include "mainwindow.hpp"
#include "fileutils.hpp"
#include "selectionaction.hpp"
#include "filenewaction.hpp"
#include "fileopenaction.hpp"
#include "filesaveaction.hpp"
#include "historyaction.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif


using namespace std::literals::string_literals; // gives the 's' suffix for strings

#ifdef _WIN32
// hack for issue with window resizing on Windows not giving live events
int resizeEventForwarder(void* main_window_void_ptr, SDL_Event* event) {
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_RESIZED) {
        SDL_Window* event_window = SDL_GetWindowFromID(event->window.windowID);
        MainWindow* main_window = static_cast<MainWindow*>(main_window_void_ptr);
        if (event_window == main_window->window) {
            main_window->layoutComponents();
            main_window->render();
        }
    }
    return 0;
}
#endif // _WIN32


MainWindow::MainWindow(const char* const processName) : closing(false), toolbox(*this), playArea(*this), buttonBar(*this, playArea), currentEventTarget(nullptr), currentLocationTarget(nullptr), currentAction(*this), interfaceFont("OpenSans-Bold.ttf", 12), processName(processName) {

    // unset all the input handle selection state
    std::fill_n(selectedToolIndices, NUM_INPUT_HANDLES, EMPTY_INDEX);

    // update dpi once first, so we can use it to create the properly sized window
    updateDpiFields(false);

    window = SDL_CreateWindow("Circuit Playground", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, logicalToPhysicalSize(640), logicalToPhysicalSize(480), SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == nullptr) {
        throw std::runtime_error("SDL_CreateWindow() failed:  "s + SDL_GetError());
    }

    #ifdef _WIN32
    // On Windows, when the user is resizing the window, we don't get any events until the resize is complete.
    // This tries to fix this
    SDL_AddEventWatch(resizeEventForwarder, static_cast<void*>(this));
    #endif // _WIN32

    // Create the renderer.  SDL_RENDERER_PRESENTVSYNC turns on the monitor refresh rate synchronization.
    // For some reason my SDL2 doesn't have any software renderer, so I can't do "SDL_RENDERER_SOFTWARE | SDL_RENDERER_ACCELERATED"
    // So we try creating hardware renderer, and if it doesn't work then we try creating software renderer.
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
    }
    if (renderer == nullptr) {
        throw std::runtime_error("SDL_CreateRenderer() failed:  "s + SDL_GetError());
    }

    // if not already the default, set the blend mode to none
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // update dpi again (in case the window was opened on something other than the default monitor)
    if (updateDpiFields()) {
        // resize the window, if the dpi changed
        SDL_SetWindowSize(window, logicalToPhysicalSize(640), logicalToPhysicalSize(480));
    }

    // do the layout
    layoutComponents(true);
}


MainWindow::~MainWindow() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}


void MainWindow::updateFonts() {
    interfaceFont.updateDPI(*this);
}


bool MainWindow::updateDpiFields(bool useWindow) {

    int display_index = 0;

    if (useWindow) {
        display_index = SDL_GetWindowDisplayIndex(window);
        if (display_index < 0) { // means that SDL_GetWindowDisplayIndex doesn't work, then we just use the default monitor
            display_index = 0;
        }
    }


    float dpi_float;
    SDL_GetDisplayDPI(display_index, nullptr, &dpi_float, nullptr); // well we expect horizontal and vertical dpis to be the same
    int dpi = static_cast<int>(dpi_float + 0.5f); // round to nearest int

    int default_dpi;
#if defined(__APPLE__)
    default_dpi = 72;
#elif defined(__linux__)
    default_dpi = 144;
#else
    default_dpi = 96; // Windows default is 96; I think the Linux default is also 96.
#endif

    // use gcd, so the multipliers don't become too big
    int gcd = std::gcd(dpi, default_dpi);

    int tmp_physicalMultiplier = physicalMultiplier;
    int tmp_logicalMultiplier = logicalMultiplier;
    // update the fields
    physicalMultiplier = dpi / gcd;
    logicalMultiplier = default_dpi / gcd;

    // check if the fields changed
    bool fields_changed = tmp_physicalMultiplier != physicalMultiplier || tmp_logicalMultiplier != logicalMultiplier;

    if (fields_changed) {

        // remember to update my own pseudo-constants
        TOOLBOX_WIDTH = logicalToPhysicalSize(LOGICAL_TOOLBOX_WIDTH);
        BUTTONBAR_HEIGHT = logicalToPhysicalSize(LOGICAL_BUTTONBAR_HEIGHT);

        // tell the components to update their cached sizes
        playArea.updateDpiFields();
        toolbox.updateDpiFields();
        buttonBar.updateDpiFields();
    }

    // return true if the fields changed
    return fields_changed;
}



void MainWindow::updateTitleBar() {
    std::string title = ((unsaved) ? "* " : "") + (filePath.empty() ? "" : getFileName(filePath.c_str()) + " "s);
    if (title.empty()) title = "Circuit Playground";
    else {
        title += u8"\u2013 Circuit Playground";
    }
    SDL_SetWindowTitle(window, title.c_str());
}


void MainWindow::layoutComponents(bool forceLayout) {

    // update the two dpi member fields
    bool dpiChanged = updateDpiFields();

    // get the size of the render target (this is a physical size)
    int pixelWidth, pixelHeight;
    SDL_GetRendererOutputSize(renderer, &pixelWidth, &pixelHeight);

    // position all the components:
    playArea.renderArea = SDL_Rect{0, 0, pixelWidth - TOOLBOX_WIDTH - HAIRLINE_WIDTH, pixelHeight - BUTTONBAR_HEIGHT - HAIRLINE_WIDTH};
    toolbox.renderArea = SDL_Rect{pixelWidth - TOOLBOX_WIDTH, 0, TOOLBOX_WIDTH, pixelHeight - BUTTONBAR_HEIGHT - HAIRLINE_WIDTH};
    buttonBar.renderArea = SDL_Rect{0, pixelHeight - BUTTONBAR_HEIGHT - HAIRLINE_WIDTH, pixelWidth, BUTTONBAR_HEIGHT};

    if (dpiChanged || forceLayout) {
        // set min window size
        SDL_SetWindowMinimumSize(window, toolbox.renderArea.w + logicalToPhysicalSize(100), buttonBar.renderArea.h + logicalToPhysicalSize(100));

        // update font sizes
        updateFonts();

        // tell children about the updated dpi
        buttonBar.updateDpi(renderer);
    }
}


void MainWindow::start() {

    // Clear the window with a black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    // Show the window to the user
    SDL_ShowWindow(window);

    // event/drawing loop:
    while (true) {
        
        // get the next event to process, if any
        while (true) {
            
#if defined(_WIN32)
            MSG msg;
            if (_suppressMouseUntilNextDown && PeekMessage(&msg, nullptr, WM_LBUTTONDOWN, WM_LBUTTONDOWN, PM_NOREMOVE)) {
                _suppressMouseUntilNextDown = false;
            }
#endif
            SDL_Event event;
            if (SDL_PollEvent(&event)) {
                processEvent(event);
                if (closing) return;
            }
            else break;
        }

        // draw everything onto the screen
        render();
    }
}


void MainWindow::processEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_WINDOWEVENT:
        processWindowEvent(event.window);
        break;
    case SDL_MOUSEMOTION:
        processMouseMotionEvent(event.motion);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        processMouseButtonEvent(event.button);
        break;
    case SDL_MOUSEWHEEL:
        processMouseWheelEvent(event.wheel);
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        processKeyboardEvent(event.key);
        break;
    }
}


void MainWindow::processWindowEvent(const SDL_WindowEvent& event) {
    switch (event.event) {
    case SDL_WINDOWEVENT_CLOSE: // close button was pressed (or some other close command like Alt-F4)
        closing = true;
        break;
    case SDL_WINDOWEVENT_RESIZED: // window got resized by window manager or by user (will NOT be triggered by programmatic resize, e.g. SDL_SetWindowSize)
        layoutComponents();
        break;
    case SDL_WINDOWEVENT_LEAVE:
        if (currentLocationTarget != nullptr) {
            currentLocationTarget->processMouseLeave();
            currentLocationTarget = nullptr;
        }
        break;
    }
}


void MainWindow::processMouseMotionEvent(const SDL_MouseMotionEvent& event) {

    // for processMouseDrag()
    if (currentEventTarget != nullptr) {
        currentEventTarget->processMouseDrag(event);
    }

    // check if the cursor left currentLocationTarget
    SDL_Point position{ event.x, event.y };
    if (currentLocationTarget != nullptr && !SDL_PointInRect(&position, &currentLocationTarget->renderArea)) {
        currentLocationTarget->processMouseLeave();
        currentLocationTarget = nullptr;
    }

    // determine new currentLocationTarget
    for (Drawable* drawable : ext::reverse(drawables)) {
        if (drawable == currentLocationTarget) {
            break;
        }
        if (SDL_PointInRect(&position, &drawable->renderArea)) {
            // found a new Drawable that isn't currently being hovered
            if (currentLocationTarget != nullptr) {
                currentLocationTarget->processMouseLeave();
                currentLocationTarget = nullptr;
            }
            currentLocationTarget = drawable;
            break;
        }
    }

    // for processMouseHover()
    if (currentEventTarget == nullptr) {
        if (currentLocationTarget != nullptr) {
            currentLocationTarget->processMouseHover(event);
        }
    }
    else {
        if (SDL_PointInRect(&position, &currentEventTarget->renderArea)) {
            currentEventTarget->processMouseHover(event);
        }
    }

}


void MainWindow::processMouseButtonEvent(const SDL_MouseButtonEvent& event) {
#if defined(_WIN32)
    if (_suppressMouseUntilNextDown) return;
#endif

    SDL_Point position{event.x, event.y};
    size_t inputHandleIndex = resolveInputHandleIndex(event);

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        // ensure only one input handle can be down at any moment
        stopMouseDrag();
        activeInputHandleIndex = inputHandleIndex;
        for (Drawable* drawable : ext::reverse(drawables)) {
            if (SDL_PointInRect(&position, &drawable->renderArea) && drawable->processMouseButtonDown(event)) {
                currentEventTarget = drawable;
                SDL_CaptureMouse(SDL_TRUE);
                break;
            }
        }
    }
    else {
        if (activeInputHandleIndex == inputHandleIndex) {
            stopMouseDrag();
        }
    }
}

void MainWindow::stopMouseDrag() {
    if (currentEventTarget != nullptr) {
        SDL_CaptureMouse(SDL_FALSE);
        auto drawable = currentEventTarget;
        currentEventTarget = nullptr;
        drawable->processMouseButtonUp();
    }
}

void MainWindow::processMouseWheelEvent(const SDL_MouseWheelEvent& event) {
    SDL_Point position;
    // poll the mouse position since it's not reflected in the event
    SDL_GetMouseState(&position.x, &position.y);
    for (Drawable* drawable : ext::reverse(drawables)) {
        if (SDL_PointInRect(&position, &drawable->renderArea) && drawable->processMouseWheel(event)) {
            break;
        }
    }
}


void MainWindow::processKeyboardEvent(const SDL_KeyboardEvent& event) {
    // invoke the handlers for all drawables and see if they want to stop event propagation
    for (KeyboardEventReceiver* receiver : ext::reverse(keyboardEventReceivers)) {
        if (receiver->processKeyboard(event)) {
            return;
        }
    }
    
    if (event.type == SDL_KEYDOWN) {
        SDL_Keymod modifiers = SDL_GetModState();
        if (modifiers & KMOD_CTRL) {
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_A:
                SelectionAction::startBySelectingAll(*this, currentAction.getStarter());
                return;
            case SDL_SCANCODE_N:
                FileNewAction::start(*this, currentAction.getStarter());
                return;
            case SDL_SCANCODE_O:
                FileOpenAction::start(*this, playArea, currentAction.getStarter());
                return;
            case SDL_SCANCODE_S:
                FileSaveAction::start(*this, modifiers, currentAction.getStarter());
                return;
            case SDL_SCANCODE_V:
                SelectionAction::startByPasting(*this, playArea, currentAction.getStarter());
                return;
            case SDL_SCANCODE_Y:
                HistoryAction::startByRedoing(*this, currentAction.getStarter());
                return;
            case SDL_SCANCODE_Z:
                HistoryAction::startByUndoing(*this, currentAction.getStarter());
                return;
            default:
                break;
            }
        }
        else {
            switch (event.keysym.scancode) { // using the scancode layout so that keys will be in the same position if the user has a non-qwerty keyboard
            case SDL_SCANCODE_R:
                currentAction.reset();
                stateManager.resetSimulator();
                return;
            case SDL_SCANCODE_SPACE:
                currentAction.reset();
                stateManager.startOrStopSimulator();
                return;
            case SDL_SCANCODE_S:
                currentAction.reset();
                stateManager.stepSimulator();
                return;
            default:
                break;
            }
        }
    }
}


// TODO: needs some way to use the old data when resizing, for consistency?
void MainWindow::render() {

    // Clear the window with a black background
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, 255);
    SDL_RenderClear(renderer);

    // draw everything to the screen - buttons, status info, play area, etc.
    for (Drawable* drawable : drawables) {
        // set clip rect to clip off parts of the surface outside renderArea
        SDL_RenderSetClipRect(renderer, &drawable->renderArea);
        // render the stuff
        drawable->render(renderer);
        // reset the clip rect
        SDL_RenderSetClipRect(renderer, nullptr);
    }

    // draw the separators
    SDL_SetRenderDrawColor(renderer, 0x66, 0x66, 0x66, 0xFF);
    SDL_RenderDrawLine(renderer, toolbox.renderArea.x - 1, 0, toolbox.renderArea.x - 1, buttonBar.renderArea.y - 2);
    SDL_RenderDrawLine(renderer, 0, buttonBar.renderArea.y - 1, buttonBar.renderArea.w - 1, buttonBar.renderArea.y - 1);

    // Then display to the user
    SDL_RenderPresent(renderer);
}


void MainWindow::loadFile(const char* filePath) {
    FileOpenAction::start(*this, playArea, currentAction.getStarter(), filePath);
}

void MainWindow::setUnsaved(bool unsaved) {
    if (this->unsaved != unsaved) {
        this->unsaved = unsaved;
        updateTitleBar();
    }
}


void MainWindow::setFilePath(const char* filePath) {
    if (this->filePath != filePath) {
        this->filePath = filePath;
        updateTitleBar();
    }
}


const char* MainWindow::getFilePath() const {
    return filePath.empty() ? nullptr : filePath.c_str();
}


bool MainWindow::hasFilePath() const {
    return !filePath.empty();
}

void MainWindow::bindTool(size_t inputHandleIndex, size_t tool_index) {
    auto it = std::find(selectedToolIndices, selectedToolIndices + NUM_INPUT_HANDLES, tool_index);
    if (it == selectedToolIndices + NUM_INPUT_HANDLES) {
        // cannot find an existing handle
        selectedToolIndices[inputHandleIndex] = tool_index;
    }
    else {
        // can find an existing handle
        using std::swap;
        swap(selectedToolIndices[inputHandleIndex], *it);
    }
}
