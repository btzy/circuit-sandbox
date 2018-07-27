#pragma once

/**
 * The main window.
 */

#include <variant>
#include <optional>
#include <vector>

#include <SDL.h>
#include <SDL_ttf.h>

#include "declarations.hpp"
#include "toolbox.hpp"
#include "playarea.hpp"
#include "buttonbar.hpp"
#include "drawable.hpp"
#include "font.hpp"
#include "statemanager.hpp"
#include "clipboardmanager.hpp"


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
    static Simulator::period_t geSimulatorPeriodFromFPS(long double fps);

    // game state
    StateManager stateManager;

private:
    bool closing; // whether the user has pressed the close button

    SDL_Rect renderArea;

    // Render-able components in the window:
    Toolbox toolbox;
    PlayArea playArea;
    ButtonBar buttonBar;

    // Note: buttonBar has to come *after* playArea for the element mouseover description to appear in the correct frame.
    std::vector<Drawable*> drawables{ &toolbox, &playArea, &buttonBar };
    std::vector<KeyboardEventReceiver*> keyboardEventReceivers{ &toolbox, &playArea, &buttonBar };
    Drawable* currentEventTarget; // the Drawable that the mouse was pressed down from
    Drawable* currentLocationTarget; // the Drawable that the mouse is currently inside
    size_t activeInputHandleIndex; // the active input handle index associated with currentEventTarget; only valid when currentEventTarget != nullptr

    // High DPI stuff:
    int physicalMultiplier = 1; // physical size = size in real monitor pixels
    int logicalMultiplier = 1; // logical size = size in device-independent virtual pixels
    // (physical size) = (logical size) * physicalMultiplier / logicalMultiplier

    std::string filePath; // empty string for untitled
    bool unsaved = false; // whether there are unsaved changes

    bool visible = true; // whether the window is visible (used to avoid rendering if window is not visible)

#if defined(_WIN32)
    bool _suppressMouseUntilNextDown = false; // Windows hack to prevent SDL from simulating mousedown event after the file dialog closes
#endif

public:
    // action state
    ActionManager currentAction;
    ClipboardManager clipboard;

    friend class PlayAreaAction;
    friend class KeyboardEventHook;
    friend class MainWindowEventHook;
    friend void PlayArea::changeMouseoverElement(const CanvasState::element_variant_t&);
    friend class ClipboardAction; // TODO: proper encapsulation

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
    #ifdef _WIN32
    // hack for issue with window resizing on Windows not giving live events
    friend int resizeEventForwarder(void* main_window_void_ptr, SDL_Event* event);
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
     * Gets the render area of this window
     */
    const SDL_Rect& getRenderArea() const {
        return renderArea;
    }

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
