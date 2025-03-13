#pragma once

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <mutex>

// Forward declarations
class ClientSession;
class PromptLayer;
class CompletionLayer;
class ActionLayer;
class LLMProviderManager;
class RController;

/**
 * AIModelOrchestrator - Coordinates the AI model layers
 *
 * This class is responsible for coordinating the different layers of the AI model:
 * - PromptLayer: Processes the user's prompt and determines what to do with it
 * - CompletionLayer: Generates text based on the prompt
 * - ActionLayer: Extracts commands from the generated text
 *
 * It also manages the LLMProviderManager, which handles communication with LLM providers.
 */
class AIModelOrchestrator {
public:
    // Constructor
    AIModelOrchestrator(std::shared_ptr<ClientSession> session);

    // Destructor
    virtual ~AIModelOrchestrator();

    // Process a model request
    bool processModelRequest(
        const std::string& prompt,
        std::function<void(const std::string&, const std::string&, const std::string&)> responseCallback
    );

    // Cancel any pending requests
    void cancelPendingRequests();

    // Set the endpoint for a provider
    void setProviderEndpoint(const std::string& providerName, const std::string& endpoint);

    // Get the endpoint for a provider
    std::string getProviderEndpoint(const std::string& providerName) const;

    // Set the system prompt for a layer
    void setSystemPrompt(const std::string& layerName, const std::string& systemPrompt);

private:
    // Client session reference
    std::shared_ptr<ClientSession> clientSession;

    // Provider manager
    std::shared_ptr<LLMProviderManager> providerManager;

    // Layers
    std::shared_ptr<PromptLayer> promptLayer;
    std::shared_ptr<CompletionLayer> completionLayer;
    std::shared_ptr<ActionLayer> actionLayer;

    // RController for executing commands
    std::shared_ptr<RController> rController;

    // Flag to track if a request is pending
    bool isRequestPending;

    // System prompts for each layer
    std::map<std::string, std::string> systemPrompts;
};