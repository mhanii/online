#pragma once

#include <string>
#include <memory>
#include <map>
#include <functional>
#include <vector>

// Forward declarations
class LLMProvider;
class ClientSession;

/**
 * LLMProviderManager - Manages different LLM providers
 *
 * This class is responsible for creating, configuring, and managing
 * different LLM providers (Gemini, Mistral, etc.). It provides a unified
 * interface for the layers to interact with different LLM APIs.
 */
class LLMProviderManager {
public:
    // Constructor
    explicit LLMProviderManager(std::shared_ptr<ClientSession> session);

    // Destructor
    virtual ~LLMProviderManager();

    // Send a request to the appropriate LLM provider
    bool sendRequest(
        const std::string& providerName,
        const std::string& prompt,
        const std::string& systemPrompt,
        std::function<void(const std::string&)> responseCallback
    );

    // Cancel any pending requests
    void cancelPendingRequests();

    // Set API endpoint for a specific provider
    void setProviderEndpoint(const std::string& providerName, const std::string& endpoint);

    // Get the current API endpoint for a provider
    std::string getProviderEndpoint(const std::string& providerName) const;

    // Check if a provider is available
    bool isProviderAvailable(const std::string& providerName) const;

    // Get the default provider name based on configuration
    std::string getDefaultProviderName() const;

private:
    // Client session reference
    std::shared_ptr<ClientSession> clientSession;

    // Map of provider names to provider instances
    std::map<std::string, std::shared_ptr<LLMProvider>> providers;

    // Map of provider names to endpoints
    std::map<std::string, std::string> providerEndpoints;

    // Initialize providers
    void initializeProviders();

    // Get or create a provider instance
    std::shared_ptr<LLMProvider> getOrCreateProvider(const std::string& providerName);
};