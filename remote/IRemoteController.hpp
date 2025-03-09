#pragma once


#include <string>
#include <memory>
#include "RemoteCommand.hpp"

class IRemoteController {
public:
    virtual ~IRemoteController() {}
    // Send a plain text command.
    virtual bool sendCommand(const std::string &command) = 0;

    // Send a command object.
    virtual bool sendCommandObject(std::unique_ptr<IRemoteCommand> command) = 0;
};

