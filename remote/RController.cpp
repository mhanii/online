#include <config.h>

#include "RController.hpp"
#include "ClientSession.hpp"
#include "Log.hpp"
#include <vector>
#include <sstream>
#include <regex>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

// Initialize static members
std::map<std::string, std::shared_ptr<RController>> RController::controllers;
std::mutex RController::controllersMutex;

RController::RController(std::shared_ptr<ClientSession> session)
    : clientSession(session)
    , sessionId(session->getId())
{
    LOG_INF("RController created for session " << sessionId);
}

RController::~RController() {
    LOG_INF("RController destroyed for session " << sessionId);
}

bool RController::executeCommand(const char* buffer, int length) {
    if (!buffer || length <= 0 || !clientSession) {
        return false;
    }

    // Store the command for processing
    std::string commandData(buffer, length);

    // Get the DocumentBroker from the ClientSession
    std::shared_ptr<DocumentBroker> docBroker = clientSession->getDocumentBroker();
    if (!docBroker) {
        LOG_ERR("No DocumentBroker found for session " << sessionId);
        return false;
    }
    // Use the DocumentBroker's thread to process the command
    docBroker->addCallback([this, commandData]() {
        this->executeCommandInternal(commandData.c_str(), commandData.length());
    });

    return true;
}

bool RController::executeCommandInternal(const char* buffer, int length) {
    if (!buffer || length <= 0 || !clientSession) {
        return false;
    }

    // Extract the first line to get the command
    std::string message(buffer, length);

    // Forward to the regular handler
    std::shared_ptr<DocumentBroker> docBroker = clientSession->getDocumentBroker();
    if (docBroker) {
        return clientSession->forwardToChild(message, docBroker);
    }
    return false;
}

bool RController::executeUnoCommand(const std::string& command) {
    if (command.empty() || !clientSession) {
        return false;
    }

    LOG_INF("Executing UNO command: " << command);
    std::cout << "command: " << command << std::endl;
    // Check if it's a UNO command
    if (command.find(".uno:") != std::string::npos) {
        // Get the DocumentBroker from the ClientSession
        std::shared_ptr<DocumentBroker> docBroker = clientSession->getDocumentBroker();
        if (docBroker) {
            // Execute the command on the correct thread
            clientSession->forwardToChild(command, docBroker);

            return true;
        }
        else {
            LOG_ERR("No DocumentBroker found for session");
            return false;
        }
    }

    return false;
}

bool RController::dispatchCommands(const std::string& commandsJson) {
    LOG_INF("Dispatching commands from JSON");

    if (commandsJson.empty() || commandsJson == "[]") {
        LOG_INF("No commands to dispatch");
        return false;
    }

    try {
        // Parse the commands JSON array
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(commandsJson);
        Poco::JSON::Array::Ptr commandsArray = result.extract<Poco::JSON::Array::Ptr>();

        std::vector<std::string> executedCommands;

        // Process each command in the array
        for (size_t i = 0; i < commandsArray->size(); i++) {
            std::string cmd = commandsArray->getElement<std::string>(i);
            std::cout << "cmd: " << cmd << std::endl;
            // Strip backslashes from the command
            std::string cleanCmd;
            cleanCmd.reserve(cmd.length());
            for (size_t j = 0; j < cmd.length(); j++) {
                // Skip backslashes
                if (cmd[j] != '\\') {
                    cleanCmd += cmd[j];
                }
            }

            // Remove quotes if they exist
            if (!cleanCmd.empty() && cleanCmd.front() == '"') {
                cleanCmd.erase(0, 1);
            }
            if (!cleanCmd.empty() && cleanCmd.back() == '"') {
                cleanCmd.pop_back();
            }
            // Check if it's a UNO command
            if (!cleanCmd.empty()) {
                LOG_INF("Executing UNO command: " << cleanCmd);

                if (clientSession) {
                    // Execute the cleaned command
                    cleanCmd = "uno .uno:" + cleanCmd;
                    // std::cout << "cleanCmd: " << cleanCmd << std::endl;
                    executeUnoCommand(cleanCmd);

                    // Extract just the command name for logging
                    size_t cmdNameEnd = cleanCmd.find(" ", 0);
                    std::string cmdName = (cmdNameEnd != std::string::npos) ?
                                         cleanCmd.substr(0, cmdNameEnd) : cleanCmd;
                    executedCommands.push_back(cmdName);
                }
            } else {
                LOG_INF("Non-UNO command or text: " << cleanCmd);
                // Handle other types of commands or text if needed
            }
        }

        // Log a summary of executed commands
        if (!executedCommands.empty()) {
            std::string summary = "Executed " + std::to_string(executedCommands.size()) + " commands: ";
            for (size_t i = 0; i < executedCommands.size(); ++i) {
                if (i > 0) summary += ", ";
                summary += executedCommands[i];
            }

            LOG_INF(summary);
            return true;
        }
    }
    catch (const std::exception& e) {
        LOG_ERR("Error parsing commands JSON: " << e.what());
    }

    return false;
}

bool RController::sendResponse(const std::string& response) {
    if (!clientSession) {
        return false;
    }

    // Get the DocumentBroker from the ClientSession
    std::shared_ptr<DocumentBroker> docBroker = clientSession->getDocumentBroker();
    if (!docBroker) {
        LOG_ERR("No DocumentBroker found for session " << sessionId);
        return false;
    }

    // Send the response back to the client on the correct thread
    docBroker->addCallback([this, response]() {
        clientSession->sendTextFrame(response.c_str(), response.length());
    });

    return true;
}

std::shared_ptr<RController> RController::getForSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(controllersMutex);
    auto it = controllers.find(sessionId);
    if (it != controllers.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<RController> RController::createForSession(std::shared_ptr<ClientSession> session) {
    if (!session) {
        return nullptr;
    }

    std::string sessionId = session->getId();
    std::lock_guard<std::mutex> lock(controllersMutex);

    // Check if a controller already exists
    auto it = controllers.find(sessionId);
    if (it != controllers.end()) {
        return it->second;
    }

    // Create a new controller
    auto controller = std::make_shared<RController>(session);
    controllers[sessionId] = controller;
    return controller;
}

void RController::removeForSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(controllersMutex);
    controllers.erase(sessionId);
    LOG_INF("RController removed for session " << sessionId);
}
