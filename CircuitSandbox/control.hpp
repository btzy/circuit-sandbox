#pragma once

/**
 * Base class for controls.
 * Controls are objects that are both drawable and receive events.
 */

#include <chrono>

#include <SDL.h>

#include "eventreceiver.hpp"
#include "drawable.hpp"


class Control : public EventReceiver, public Drawable {
};

