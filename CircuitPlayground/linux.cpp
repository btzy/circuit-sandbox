#ifdef __linux__

#include <cstdlib>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>

double linux_getSystemDpi() {
    char *resourceString = XResourceManagerString(_glfw.x11.display);
    XrmDatabase db;
    XrmValue value;
    char *type = NULL;
    double dpi = 0.0;

    XrmInitialize(); /* Need to initialize the DB before calling Xrm* functions */

    db = XrmGetStringDatabase(resourceString);

    if (resourceString) {
        if (XrmGetResource(db, "Xft.dpi", "String", &type, &value) == True) {
            if (value.addr) {
                dpi = std::atof(value.addr);
            }
        }
    }

    return dpi;
}

#endif // __linux__
