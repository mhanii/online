#include <config.h>

#include "LLMProviderManager.hpp"
#include "LLMProvider.hpp"
#include "GeminiProvider.hpp"
#include "ClientSession.hpp"
#include "Log.hpp"

#include <iostream>
#include <Poco/Environment.h>

LLMProviderManager::LLMProviderManager(std::shared_ptr<ClientSession> session)
    : clientSession(session)
{
    LOG_INF("LLMProviderManager created");
    initializeProviders();
}

LLMProviderManager::~LLMProviderManager()
{
    LOG_INF("LLMProviderManager destroyed");
    cancelPendingRequests();
}

void LLMProviderManager::initializeProviders()
{
    LOG_INF("Initializing LLM providers");

    // Initialize Gemini provider
    try {
        auto geminiProvider = std::make_shared<GeminiProvider>(clientSession);
        providers["gemini"] = geminiProvider;
        providers["gemini-flash"] = geminiProvider;
        providers["gemini-1.5-flash"] = geminiProvider;
        providers["gemini-2.0-flash"] = geminiProvider;

        // Set default endpoint
        providerEndpoints["gemini"] = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent";

        LOG_INF("Gemini provider initialized");
    } catch (const std::exception& e) {
        LOG_ERR("Failed to initialize Gemini provider: " << e.what());
    }

    // Initialize other providers as needed
    // ...
}

bool LLMProviderManager::sendRequest(
    const std::string& providerName,
    const std::string& prompt,
    const std::string& systemPrompt,
    std::function<void(const std::string&)> responseCallback)
{
    LOG_INF("Sending request to provider: " << providerName);

    // Get the provider
    std::shared_ptr<LLMProvider> provider = getOrCreateProvider(providerName);
    if (!provider) {
        LOG_ERR("Provider not found: " << providerName);
        if (responseCallback) {
            responseCallback("error: Provider not found: " + providerName);
        }
        return false;
    }

    // Send the request
    return provider->sendRequest(prompt, systemPrompt, responseCallback);
}

void LLMProviderManager::cancelPendingRequests()
{
    LOG_INF("Canceling all pending requests");

    // Cancel requests for all providers
    for (auto& pair : providers) {
        if (pair.second) {
            pair.second->cancelPendingRequests();
        }
    }
}

void LLMProviderManager::setProviderEndpoint(const std::string& providerName, const std::string& endpoint)
{
    LOG_INF("Setting endpoint for provider " << providerName << ": " << endpoint);

    // Store the endpoint
    providerEndpoints[providerName] = endpoint;

    // Update the provider if it exists
    auto it = providers.find(providerName);
    if (it != providers.end() && it->second) {
        it->second->setEndpoint(endpoint);
    }
}

std::string LLMProviderManager::getProviderEndpoint(const std::string& providerName) const
{
    // Get the endpoint from the map
    auto it = providerEndpoints.find(providerName);
    if (it != providerEndpoints.end()) {
        return it->second;
    }

    return "";
}

bool LLMProviderManager::isProviderAvailable(const std::string& providerName) const
{
    // Check if the provider exists and is available
    auto it = providers.find(providerName);
    if (it != providers.end() && it->second) {
        return it->second->isAvailable();
    }

    return false;
}

std::string LLMProviderManager::getDefaultProviderName() const
{
    // Check if Gemini is available
    if (isProviderAvailable("gemini")) {
        return "gemini";
    }

    // Check for other providers
    // ...

    // Return the first available provider
    for (const auto& pair : providers) {
        if (pair.second && pair.second->isAvailable()) {
            return pair.first;
        }
    }

    // No provider available
    return "";
}

std::shared_ptr<LLMProvider> LLMProviderManager::getOrCreateProvider(const std::string& providerName)
{
    // Check if the provider already exists
    auto it = providers.find(providerName);
    if (it != providers.end()) {
        return it->second;
    }

    // If not, check if it's an alias for an existing provider
    if (providerName.find("gemini") != std::string::npos) {
        it = providers.find("gemini");
        if (it != providers.end()) {
            return it->second;
        }
    }

    // If still not found, create a new provider based on the name
    if (providerName.find("gemini") != std::string::npos) {
        try {
            auto provider = std::make_shared<GeminiProvider>(clientSession);
            providers[providerName] = provider;
            return provider;
        } catch (const std::exception& e) {
            LOG_ERR("Failed to create Gemini provider: " << e.what());
        }
    }

    // Add more provider types as needed
    // ...

    // Provider not found or couldn't be created
    return nullptr;
}