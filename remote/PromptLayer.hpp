#pragma once

#include <string>
#include <memory>
#include <functional>

// Forward declarations
class ClientSession;
class LLMProviderManager;

/**
 * PromptLayer - Handles prompt preparation
 *
 * This class is responsible for preparing prompts before they are sent
 * to the LLM. It can add context, format the prompt, and apply any
 * necessary transformations.
 */
class PromptLayer {
public:
    // Constructor
    PromptLayer(std::shared_ptr<ClientSession> session,
                std::shared_ptr<LLMProviderManager> _providerManager);

    // Destructor
    virtual ~PromptLayer();

    // Process a prompt and pass it to the next layer
    bool processPrompt(
        const std::string& rawPrompt,
        const std::string& systemPrompt,
        std::function<void(const std::string&, const std::string&, const std::string&)> callback
    );

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

    // Format the prompt
    std::string formatPrompt(const std::string& rawPrompt);

    // Add context to the prompt if needed
    std::string addContextToPrompt(const std::string& formattedPrompt);
};