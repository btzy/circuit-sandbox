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

namespace IconCodePoints {
    enum : uint16_t {
        NEW = 0xE000,
        OPEN = 0xE001,
        SAVE = 0xE002,
        PLAY = 0xE010,
        PAUSE = 0xE011,
        STEP = 0xE012,
        RESET = 0xE013,
        SPEED = 0xE018,
        UNDO = 0xE020,
        REDO = 0xE021
    };
}
