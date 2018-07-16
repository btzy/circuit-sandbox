#include "elements.hpp"
#include "fileinputcommunicator.hpp"

// This file is so that we don't need to have fileinputcommunicator.hpp included in elements.hpp (which is included by fileinputcommunicator.hpp)

const std::string& FileInputCommunicatorElement::getCommunicatorFile() const noexcept {
    return communicator->getFile();
}
