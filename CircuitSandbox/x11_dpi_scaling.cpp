/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

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
