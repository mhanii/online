#pragma once

#include <string>
#include <functional>
#include <memory>

// Forward declaration
class ClientSession;

/**
 * LLMProvider - Base interface for LLM providers
 *
 * This abstract class defines the interface that all LLM providers
 * must implement. It provides methods for sending requests, canceling
 * requests, and configuring the provider.
 */
class LLMProvider {
public:
    // Constructor
    explicit LLMProvider(std::shared_ptr<ClientSession> session) : clientSession(session) {}

    // Virtual destructor
    virtual ~LLMProvider() = default;

    // Send a request to the LLM
    virtual bool sendRequest(
        const std::string& prompt,
        const std::string& systemPrompt,
        std::function<void(const std::string&)> responseCallback
    ) = 0;

    // Cancel any pending requests
    virtual void cancelPendingRequests() = 0;

    // Set the API endpoint
    virtual void setEndpoint(const std::string& endpoint) = 0;

    // Get the current API endpoint
    virtual std::string getEndpoint() const = 0;

    // Check if the provider is available
    virtual bool isAvailable() const = 0;

    // Get the provider name
    virtual std::string getName() const = 0;

protected:
    // Client session reference
    std::shared_ptr<ClientSession> clientSession;
};