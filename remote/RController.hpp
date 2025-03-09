#pragma once


#include <memory>

// Forward declaration: ClientSession is part of your core.
class ClientSession;

class RController {
public:
    explicit RController(std::shared_ptr<ClientSession> session);
    virtual ~RController();

    // Implement sending a simple text command.
    // bool sendCommand(const std::string &command);

    // Implement sending a command object.
    bool executeCommand(const char* buffer, int length);

private:
    std::shared_ptr<ClientSession> clientSession;
};
