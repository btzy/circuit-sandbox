//
//  launch_browser.mm
//  Circuit Sandbox
//
//  Created by Bernard Teo on 20/8/18.
//
//  For Cocoa browser launching.
//

#if defined(__APPLE__)

#import <AppKit/AppKit.h>
#include "launch_browser.hpp"

namespace WebResource {
    bool launch(const url_char_t* url) {
        @autoreleasepool {
            return ([[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]]) == YES;
        }
    }
}

#endif
