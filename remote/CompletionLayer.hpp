#pragma once

#include <string>
#include <memory>
#include <functional>

// Forward declarations
class ClientSession;
class LLMProviderManager;

/**
 * CompletionLayer - Handles text generation
 *
 * This class is responsible for sending prompts to the LLM provider
 * and receiving the generated text. It handles the communication with
 * the LLM provider and processes the raw responses.
 */
class CompletionLayer {
public:
    // Constructor
    CompletionLayer(std::shared_ptr<ClientSession> session,
                   std::shared_ptr<LLMProviderManager> _providerManager);

    // Destructor
    virtual ~CompletionLayer();

    // Process a prompt and generate text
    bool generateCompletion(
        const std::string& prompt,
        const std::string& systemPrompt,
        std::function<void(const std::string&, const std::string&)> callback
    );

    // Cancel any pending requests
    void cancelPendingRequests();

    // Set the system prompt
    void setSystemPrompt(const std::string& systemPrompt);

    // Get the current system prompt
    std::string getSystemPrompt() const;

private:
    // Client session reference
    std::shared_ptr<ClientSession> clientSession;

    // Provider manager reference
    std::shared_ptr<LLMProviderManager> providerManager;

    // Default system prompt
    std::string defaultSystemPrompt;

    // Flag to track if a request is pending
    bool isRequestPending;

    // Process the raw response from the LLM
    void processRawResponse(
        const std::string& rawResponse,
        std::function<void(const std::string&, const std::string&)> callback
    );

    // Extract text and feedback from the response
    void extractResponseComponents(
        const std::string& rawResponse,
        std::string& text,
        std::string& feedback
    );
};