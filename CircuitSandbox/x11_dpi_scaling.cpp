#include <cstdlib>
#include <SDL.h>
#include <SDL_syswm.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>

namespace DpiScaling {
    
    /**
     * Get DPI scaling.  Returns true if query was successful.
     * See https://stackoverflow.com/questions/49071914/how-to-get-system-scale-factor-in-x11
     */
    bool getDpi(SDL_Window* window, int display_index, float& out) {
        SDL_SysWMinfo info;
        if (window) {
            SDL_VERSION(&info.version);
            SDL_GetWindowWMInfo(window, &info);
        }
        if (!window || info.subsystem != SDL_SYSWM_X11) {
            return SDL_GetDisplayDPI(display_index, nullptr, &out, nullptr) == 0;
        }
        
        char *resourceString = XResourceManagerString(info.info.x11.display);

        XrmInitialize(); /* Need to initialize the DB before calling Xrm* functions */

        XrmDatabase db = XrmGetStringDatabase(resourceString);
        
        bool good = false;

        if (db) {
            XrmValue value;
            char *type = nullptr;
            if (XrmGetResource(db, "Xft.dpi", "String", &type, &value) == True) {
                if (value.addr) {
                    char* str_end;
                    out = strtof(value.addr, &str_end);
                    if(str_end != value.addr) good = true;
                }
            }
            
            XrmDestroyDatabase(db);
        }
        
        return good;
    }
}
