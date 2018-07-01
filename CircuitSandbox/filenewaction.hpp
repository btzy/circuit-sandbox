#pragma once

#include <boost/process/spawn.hpp>
#include <SDL.h>

#include "statefulaction.hpp"
#include "mainwindow.hpp"

class FileNewAction final : public Action {
public:
    FileNewAction(MainWindow& mainWindow) {
        boost::process::spawn(mainWindow.processName);
    };

    static inline void start(MainWindow& mainWindow, const ActionStarter& starter) {
        starter.start<FileNewAction>(mainWindow);
        starter.reset();
    }
};
