#pragma once


#include <memory>
#include <string>
#include <wsd/ClientSession.hpp>




// Forward declaration of ClientSession.

// Interface for remote command objects.
class IRemoteCommand{
public:
    // Execute the command using the provided ClientSession.
    virtual void execute(std::shared_ptr<ClientSession> session) = 0;
};

// Concrete remote command that sends a text-based command.
class RemoteCommand : public IRemoteCommand{
public:
    explicit RemoteCommand(const std::string &text)
        : commandText(text) {}
    virtual ~RemoteCommand() {}

    void execute(std::shared_ptr<ClientSession> session) {
        if (session) {

            session->_handleInput(commandText.c_str(),commandText.length());
        }
    }

private:
    std::string commandText;
};