/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

/**
 * The main window.
 */

#include <variant>
#include <optional>
#include <vector>
#include <tuple>
#if defined(__APPLE__)
#include <mutex>
#include <atomic>
#endif

#include <SDL.h>
#include <SDL_ttf.h>

#include "declarations.hpp"
#include "toolbox.hpp"
#include "playarea.hpp"
#include "buttonbar.hpp"
#include "notificationdisplay.hpp"
#include "control.hpp"
#include "font.hpp"
#include "statemanager.hpp"
#include "clipboardmanager.hpp"
#include "tag_tuple.hpp"


class MainWindow {
public:

    // logical units
    constexpr static int LOGICAL_TOOLBOX_WIDTH = 128;
    constexpr static int LOGICAL_BUTTONBAR_HEIGHT = 24;

    // physical units
    int TOOLBOX_WIDTH = LOGICAL_TOOLBOX_WIDTH;
    int BUTTONBAR_HEIGHT = LOGICAL_BUTTONBAR_HEIGHT;

    // hairline width (for drawing separating lines)
    constexpr static int HAIRLINE_WIDTH = 1;

    std::string displayedSimulationFPS = "5"; // the FPS as displayed in the dialog box (has to be defined before stateManager)

    // throws std::logic_error or its derived classes
    static Simulator::period_t getSimulatorPeriodFromFPS(long double fps);

    // game state
    StateManager stateManager;

private:
    bool closing; // whether the user has pressed the close button

    SDL_Rect renderArea;

    // Render-able components in the window:
    Toolbox toolbox;
    PlayArea playArea;
    ButtonBar buttonBar;
    NotificationDisplay notificationDisplay;

    // Note: buttonBar has to come *after* playArea for the element mouseover description to appear in the correct frame.
    std::vector<Drawable*> drawables{ &toolbox, &playArea, &buttonBar, &notificationDisplay };
    std::vector<Control*> controls{ &toolbox, &playArea, &buttonBar };
    std::vector<KeyboardEventReceiver*> keyboardEventReceivers{ &toolbox, &playArea, &buttonBar };
    Control* currentEventTarget; // the Control that the mouse was pressed down from
    Control* currentLocationTarget; // the Control that the mouse is currently inside
    size_t activeInputHandleIndex; // the active input handle index associated with currentEventTarget; only valid when currentEventTarget != nullptr

    // High DPI stuff:
    int physicalMultiplier = 1; // physical size = size in real monitor pixels
    int logicalMultiplier = 1; // logical size = size in device-independent virtual pixels
    // (physical size) = (logical size) * physicalMultiplier / logicalMultiplier

    std::string filePath; // empty string for untitled
    bool unsaved = false; // whether there are unsaved changes

    bool visible = true; // whether the window is visible (used to avoid rendering if window is not visible)

    // RAII object so we can remove the old notification immediately if the user toggles multiples times in succession
    NotificationDisplay::UniqueNotification toggleBeginnerModeNotification;
    NotificationDisplay::UniqueNotification noUndoNotification;
    NotificationDisplay::UniqueNotification noRedoNotification;
    NotificationDisplay::UniqueNotification changeSpeedNotification;

#if defined(_WIN32)
    unsigned long mainThreadId;
#endif
    
#if defined(_WIN32)
    bool _suppressMouseUntilNextDown = false; // Windows hack to prevent SDL from simulating mousedown event after the file dialog closes
    bool _sizeMoveTimerRunning = false;
#elif defined(__APPLE__)
    std::atomic<bool> _mainQueueDispatchRunning = false;
#endif

public:
    class Registrar {
    private:
        using types = ext::tag_tuple<Drawable, Control, KeyboardEventReceiver>;
        template <typename T>
        struct item {
            // items to add
            typename std::vector<T*> add;
            // number of items to remove (remove *before* adding!)
            typename std::vector<T*>::size_type remove = 0;
        };
        typename types::transform<item>::instantiate<std::tuple> data;

        friend class MainWindow;

        template <typename T>
        auto& get_real_list(MainWindow& mainWindow) {
            if constexpr(std::is_same_v<T, Drawable>) {
                return mainWindow.drawables;
            }
            else if constexpr(std::is_same_v<T, Control>) {
                return mainWindow.controls;
            }
            else if constexpr(std::is_same_v<T, KeyboardEventReceiver>) {
                return mainWindow.keyboardEventReceivers;
            }
            else {
                static_assert(std::is_same_v<T, T>, "Unknown list type.");
            }
        }

        // do necessary cleanup before removal
        template <typename T, typename Container>
        void pre_remove(MainWindow& mainWindow, Container& container) {
            if constexpr(std::is_same_v<T, Control>) {
                auto removed_pointer = container.back();
                if (mainWindow.currentLocationTarget == removed_pointer) {
                    mainWindow.currentLocationTarget = nullptr;
                }
                if (mainWindow.currentEventTarget == removed_pointer) {
                    mainWindow.currentEventTarget = nullptr;
                }
            }
            else {}
        }

        // called by main window at a safe time to update its lists
        void update(MainWindow& mainWindow) {
            types::for_each([&](auto type_tag, auto) {
                using type = typename decltype(type_tag)::type;
                // get mainWindow's real list
                auto& real_list = get_real_list<type>(mainWindow);

                auto& curr_item = std::get<item<type>>(data);
                
                // update mainWindow's real list
                for (; curr_item.remove > 0; --curr_item.remove) {
                    assert(!real_list.empty());
                    pre_remove<type>(mainWindow, real_list);
                    real_list.pop_back();
                }

                for (type* obj : curr_item.add) {
                    real_list.push_back(obj);
                }
                curr_item.add.clear();
            });
        }

    public:
        template <typename... T, typename Obj>
        void push(Obj* obj) {
            ext::tag_tuple<T...>::for_each([&](auto type_tag, auto) {
                // store this object in the add list
                std::get<item<typename decltype(type_tag)::type>>(data).add.push_back(obj);
            });
        }
        template <typename... T, typename Obj>
        void pop(Obj* obj) {
            ext::tag_tuple<T...>::for_each([&](auto type_tag, auto) {
                auto& curr_item = std::get<item<typename decltype(type_tag)::type>>(data);
                if (!curr_item.add.empty()) {
                    // if the add list is not empty, we should remove from it first

                    curr_item.add.pop_back();
                }
                else {
                    // if theres nothing to remove, then we add to the remove count
                    curr_item.remove++;
                }
            });
        }
    };

    friend class Registrar;

    // action state
    ActionManager currentAction;
    ClipboardManager clipboard;
    Registrar registrar;

    /**
     * Gets the render area of this window
     */
    const SDL_Rect& getRenderArea() const {
        return renderArea;
    }

private:
    friend class PlayAreaAction;
    friend class KeyboardEventHook;
    friend class MainWindowEventHook;
    friend void PlayArea::changeMouseoverElement(const CanvasState::element_variant_t&);
    friend class ClipboardAction;
    friend class HistoryAction;
    friend class ChangeSimulationSpeedAction;

    /**
     * Process the event that has occurred (called by start())
     */
    void processEvent(const SDL_Event&);
    void processWindowEvent(const SDL_WindowEvent&);
    void processMouseMotionEvent(const SDL_MouseMotionEvent&);
    void processMouseButtonEvent(const SDL_MouseButtonEvent&);
    void processMouseWheelEvent(const SDL_MouseWheelEvent&);
    void processKeyboardEvent(const SDL_KeyboardEvent&);
    void processTextInputEvent(const SDL_TextInputEvent&);

    /**
     * Renders everything to the screen
     */
    void render();
#if defined(_WIN32)
    // hack for issue with window resizing on Windows and Mac not giving live events
    friend int resizeEventForwarder(void* main_window_void_ptr, SDL_Event* event);
#endif // _WIN32
#if defined(__APPLE__)
    // hack for issue with window resizing on Windows and Mac not giving live events
    friend int resizeEventForwarder(void* main_window_void_ptr, SDL_Event* event);
    friend void dispatchLayoutHandler(void* main_window_void_ptr);
    friend void dispatchRenderHandler(void* main_window_void_ptr);
#endif // _WIN32

    /**
     * Starts the event loop (blocks until window is closed).
     */
    void startEventLoop();

    /**
     * Recalculate all the 'renderArea' for all the Drawables (call after resizing)
     * Also does DPI recalculation (SDL2 will send a resize event when the dpi changes)
     */
    void layoutComponents(bool forceLayout = false);

    /**
     * Update the dpi fields.  Returns true if the dpi got changed
     */
    bool updateDpiFields(bool useWindow = true);

    /**
     * Update fonts
     */
    void updateFonts();

    /**
     * Update the title bar (uses the unsaved flag and the file name)
     */
    void updateTitleBar();

    /**
     * Toggle beginner mode notifications
     */
    void toggleBeginnerMode();

public:

    // SDL and window stuff:
    SDL_Window* window;
    SDL_Renderer* renderer;

    // fonts (loaded in constructor and closed in destructor)
    Font interfaceFont;


    /**
     * Stores the element that is selected by the toolbox.
     */
    size_t selectedToolIndices[NUM_INPUT_HANDLES];

    constexpr static size_t EMPTY_INDEX = static_cast<size_t>(-1);

    constexpr static SDL_Color backgroundColor{0, 0, 0, 0xFF}; // black background

    const char* const processName; // the name used to start this process (used for spawning duplicate processes)

    /**
     * Initializes SDL and the window (it will be hidden)
     */
    MainWindow(const char* const processName);

    /**
     * Destroys the window and quits SDL
     */
    ~MainWindow();

    /**
     * Shows the window to the user, and runs the event loop.
     * This function will block until the window is closed.
     * 'filePath' overload will open the given file.
     */
    void start();
    void start(const char* filePath);

    /**
     * Overwrite the current canvas state with the given file.
     * This will reset the history system.
     */
    void loadFile(const char* filePath);

    /**
     * Set the asterisk in the title bar
     */
    void setUnsaved(bool unsaved);

    /**
     * Set the file path
     */
    void setFilePath(const char* filePath);

    /**
     * Get the file path
     */
    const char* getFilePath() const;

    /**
     * Check whether file path has been set
     */
    bool hasFilePath() const;

    /**
     * Bind a tool to an input handle. If there is already a handle bound to the tool, swap the handles.
     */
    void bindTool(size_t inputHandleIndex, size_t tool_index);

    /**
     * Lifts up the current mouse button, if any.
     * processMouseButtonUp() will be called immediately if the mouse button is currently down
     * This is a no-op if currentEventTarget == nullptr
     */
    void stopMouseDrag();

    /**
     * Prevent mousedown event from entering the event system until the next real Windows WM_LBUTTONDOWN is received.
     */
    void suppressMouseUntilNextDown() {
#if defined(_WIN32)
        _suppressMouseUntilNextDown = true;
#endif
    }

    NotificationDisplay& getNotificationDisplay() {
        return notificationDisplay;
    }

    /**
     * DPI conversion functions
     */
    template <typename T>
    inline T logicalToPhysicalSize(T logicalSize) const {
        return logicalSize * physicalMultiplier / logicalMultiplier;
    }
    template <typename T>
    inline T physicalToLogicalSize(T physicalSize) const {
        return physicalSize * logicalMultiplier / physicalMultiplier;
    }
};
