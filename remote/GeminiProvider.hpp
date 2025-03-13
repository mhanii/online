#pragma once

#include "LLMProvider.hpp"
#include <string>
#include <functional>
#include <memory>

/**
 * GeminiProvider - Implementation of LLMProvider for Google's Gemini API
 *
 * This class handles communication with the Gemini API, including
 * authentication, request formatting, and response parsing.
 */
class GeminiProvider : public LLMProvider {
public:
    // Constructor
    explicit GeminiProvider(std::shared_ptr<ClientSession> session);

    // Destructor
    virtual ~GeminiProvider();

    // Send a request to the Gemini API
    bool sendRequest(
        const std::string& prompt,
        const std::string& systemPrompt,
        std::function<void(const std::string&)> responseCallback
    ) override;

    // Cancel any pending requests
    void cancelPendingRequests() override;

    // Set the API endpoint
    void setEndpoint(const std::string& endpoint) override;

    // Get the current API endpoint
    std::string getEndpoint() const override;

    // Check if the provider is available
    bool isAvailable() const override;

    // Get the provider name
    std::string getName() const override { return "gemini"; }

private:
    // API endpoint
    std::string apiEndpoint;

    // API key
    std::string apiKey;

    // Flag to track if a request is pending
    bool isRequestPending;

    // Load the API key from configuration
    void loadApiKey();

    // Process the response from the API
    void processResponse(
        const std::string& response,
        std::function<void(const std::string&)> callback
    );

    // Perform the actual HTTP request
    bool performHttpRequest(
        const std::string& url,
        const std::string& payload,
        std::function<void(const std::string&)> callback
    );
};