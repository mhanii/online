#include <config.h>

#include "RController.hpp"
#include "ModelApiClient.hpp"
#include "ClientSession.hpp"
#include "Log.hpp"
#include <vector>
#include <sstream>

// Initialize static members
std::map<std::string, std::shared_ptr<RController>> RController::controllers;
std::mutex RController::controllersMutex;

RController::RController(std::shared_ptr<ClientSession> session)
    : clientSession(session)
    , apiClient(std::make_shared<ModelApiClient>(session))
    , sessionId(session->getId())
{
    LOG_INF("RController created for session " << sessionId);
}

RController::~RController() {
    LOG_INF("RController destroyed for session " << sessionId);
    // Cancel any pending API requests
    if (apiClient) {
        apiClient->cancelPendingRequests();
    }
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
    std::cout << "Adding callback to DocumentBroker: "  << commandData << std::endl;
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
    std::string firstLine;
    size_t newlinePos = message.find('\n');
    if (newlinePos != std::string::npos) {
        firstLine = message.substr(0, newlinePos);
    } else {
        firstLine = message;
    }

    // Tokenize the command
    std::vector<std::string> tokens;
    std::istringstream iss(firstLine);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    std::cout << "Command: " << message << std::endl;
    // Check if this is a model command
    if (!tokens.empty() && tokens[0] == "model") {
        // Handle model command
        return processModelCommand(message, tokens);
    }

    // Not a model command, forward to the regular handler
    std::shared_ptr<DocumentBroker> docBroker = clientSession->getDocumentBroker();
    if (docBroker) {
        return clientSession->forwardToChild(message, docBroker);
    }
    return false;
}

bool RController::processModelCommand(const std::string& command, const std::vector<std::string>& tokens) {
    // Mark parameter as used to avoid compiler warning
    (void)command;

    if (tokens.size() < 2) {
        // Need at least "model" and a model name
        sendResponse("error: cmd=model kind=invalid_syntax");
        return false;
    }
    // Rename modeName to commandType
    const std::string& commandType = tokens[1];
    std::vector<std::string> params(tokens.begin() + 2, tokens.end());

    LOG_INF("Processing model command for command: " << commandType);

    // Use the API client to send the request
    // The API client will handle thread safety for the response callback
    return apiClient->sendModelRequest(commandType, params,
        [this](const std::string& response) {
            // Process the response to extract and execute commands
            dispatchCommandsFromResponse(response);

            // Send the response back to the client
            sendResponse(response);
        });
}
// change to be a json object

void RController::dispatchCommandsFromResponse(const std::string& response) {
    LOG_INF("Dispatching commands from response");

    // Check if this is a model response
    if (response.find("modelresponse success") != 0) {
        LOG_INF("Not a model response, skipping command dispatch");
        return;
    }

    // Extract the actual response text (after "modelresponse success ")
    // response is going to be a json object, and status is going to be result.status
    Poco::JSON::Object::Ptr responseObj;
    Poco::JSON::Parser parser;
    try {
        responseObj = parser.parse(response.substr(21)).extract<Poco::JSON::Object::Ptr>();
    } catch (const Poco::Exception& e) {
        LOG_ERR("Error parsing JSON response: " << e.displayText());
    }
    std::string status = responseObj->getValue<std::string>("status");
    if (status != "success") {
        LOG_ERR("Model response status is not success: " << status);
        return;
    }

    size_t snippetStart = response.find("[snippet:");
    if (snippetStart == std::string::npos) {
        // No snippets found, just return
        LOG_INF("No command snippets found in response");
        return;
    }

    // Process each snippet in the response
    size_t currentPos = 0;
    std::vector<std::string> executedCommands;

    while ((snippetStart = response.find("[snippet:", currentPos)) != std::string::npos) {
        // Find the end of the snippet
        size_t snippetEnd = response.find("[/snippet]", snippetStart);
        if (snippetEnd == std::string::npos) {
            LOG_WRN("Malformed snippet: missing end tag");
            break;
        }

        // Extract the snippet title and content
        size_t titleEnd = response.find("]", snippetStart);
        std::string snippetTitle = response.substr(snippetStart + 9, titleEnd - (snippetStart + 9));

        // Extract the commands (content between the title end and the end tag)
        std::string snippetContent = response.substr(titleEnd + 1,
                                                        snippetEnd - (titleEnd + 1));

        LOG_INF("Found snippet: " << snippetTitle);

        // Split the content into individual commands
        std::vector<std::string> commands;
        std::istringstream iss(snippetContent);
        std::string line;

        while (std::getline(iss, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
            line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);

            if (!line.empty()) {
                commands.push_back(line);
            }
        }

        // Execute each command
        for (const auto& cmd : commands) {
            // Check if it's a UNO command
            if (cmd.find(".uno:") != std::string::npos) {
                LOG_INF("Executing UNO command: " << cmd);

                if (clientSession) {
                    // Execute the command directly
                    executeCommand(cmd.c_str(), cmd.length());

                    // Extract just the command name for logging
                    size_t cmdNameEnd = cmd.find(" ", 0);
                    std::string cmdName = (cmdNameEnd != std::string::npos) ?
                                         cmd.substr(0, cmdNameEnd) : cmd;
                    executedCommands.push_back(cmdName);
                }
            } else {
                LOG_INF("Non-UNO command or text: " << cmd);
                // Handle other types of commands or text if needed
            }
        }

        // Move past this snippet for the next iteration
        currentPos = snippetEnd + 10; // Length of "[/snippet]"
    }

    // Log a summary of executed commands
    if (!executedCommands.empty()) {
        std::string summary = "Executed " + std::to_string(executedCommands.size()) + " commands: ";
        for (size_t i = 0; i < executedCommands.size(); ++i) {
            if (i > 0) summary += ", ";
            summary += executedCommands[i];
        }

        LOG_INF(summary);
    }
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
