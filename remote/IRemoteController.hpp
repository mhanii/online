#pragma once

#include <memory>
#include <string>

// Forward declaration
class ClientSession;

/**
 * IRemoteController - Interface for remote controllers
 *
 * This interface defines the methods that all remote controllers must implement.
 * It provides a common way to interact with different types of remote controllers.
 */
class IRemoteController
{
public:
    virtual ~IRemoteController() = default;

    /**
     * Execute a command received from the client
     *
     * @param buffer The command buffer
     * @param length The length of the buffer
     * @return true if the command was executed successfully
     */
    virtual bool executeCommand(const char* buffer, int length) = 0;

    /**
     * Get the session ID associated with this controller
     *
     * @return The session ID
     */
    virtual std::string getSessionId() const = 0;

    /**
     * Get the client session associated with this controller
     *
     * @return The client session
     */
    virtual std::shared_ptr<ClientSession> getClientSession() const = 0;
};
