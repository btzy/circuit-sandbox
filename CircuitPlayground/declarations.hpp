#pragma once

class MainWindow;
class Toolbox;
class PlayArea;

/**
 * The number of distinct "input handles": 5 (SDL2 supports this many mouse buttons) + 1 (for touch input)
 */
constexpr static size_t NUM_INPUT_HANDLES = 6;
