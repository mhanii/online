#pragma once

#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <vector>

#include "IRemoteController.hpp"

// Forward declaration: ClientSession is part of your core.
class ClientSession;

// Forward declaration for the API client
class ModelApiClient;

class RController : public IRemoteController
{
public:
    explicit RController(std::shared_ptr<ClientSession> session);
    virtual ~RController();

    // Execute a command received from the client (from IRemoteController)
    bool executeCommand(const char* buffer, int length) override;

    // Internal method to execute commands on the correct thread
    bool executeCommandInternal(const char* buffer, int length);

    // Get the session ID (from IRemoteController)
    std::string getSessionId() const override { return sessionId; }

    // Get the client session (from IRemoteController)
    std::shared_ptr<ClientSession> getClientSession() const override { return clientSession; }

    // Process model-related commands
    bool processModelCommand(const std::string& command, const std::vector<std::string>& tokens);

    // Dispatch commands from a model response
    void dispatchCommandsFromResponse(const std::string& response);

    // Get the RController instance for a specific session
    static std::shared_ptr<RController> getForSession(const std::string& sessionId);

    // Create a new RController for a session
    static std::shared_ptr<RController> createForSession(std::shared_ptr<ClientSession> session);

    // Remove a controller when a session is closed
    static void removeForSession(const std::string& sessionId);

    // Get the API client
    std::shared_ptr<ModelApiClient> getApiClient() const { return apiClient; }

private:
    std::shared_ptr<ClientSession> clientSession;
    std::shared_ptr<ModelApiClient> apiClient;
    std::string sessionId;

    // Send a response back to the client
    bool sendResponse(const std::string& response);

    // Static map to store RController instances by session ID
    static std::map<std::string, std::shared_ptr<RController>> controllers;
    static std::mutex controllersMutex;
};
