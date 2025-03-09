
#include <config.h>

#include "RController.hpp"
#include "ClientSession.hpp"

RController::RController(std::shared_ptr<ClientSession> session)
    : clientSession(session)
{
}

RController::~RController() {
    // Clean-up, if needed.
}

// bool RController::sendCommand(const std::string &command) {
//     if (clientSession) {
//         // Directly forward the command via the ClientSession interface.
//         return clientSession->forwardToClient(command);
//     }
//     return false;
// }

bool RController::executeCommand(const char* buffer, int length) {
    if (length && clientSession) {
        clientSession->_handleInput(buffer,length);
        return true;
    }
    return false;
}
