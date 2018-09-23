/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

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
