#include <config.h>

#include "CompletionLayer.hpp"
#include "LLMProviderManager.hpp"
#include "ClientSession.hpp"
#include "Log.hpp"

#include <iostream>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <regex>
#include <sstream>

CompletionLayer::CompletionLayer(std::shared_ptr<ClientSession> session,
                               std::shared_ptr<LLMProviderManager> _providerManager)
    : clientSession(session)
    , providerManager(_providerManager)
    , defaultSystemPrompt("Generate helpful, accurate, and concise responses.")
    , isRequestPending(false)
{
    LOG_INF("CompletionLayer created");
}

CompletionLayer::~CompletionLayer()
{
    LOG_INF("CompletionLayer destroyed");
    cancelPendingRequests();
}

bool CompletionLayer::generateCompletion(
    const std::string& prompt,
    const std::string& systemPrompt,
    std::function<void(const std::string&, const std::string&)> callback)
{
    if (isRequestPending)
    {
        LOG_WRN("Request already pending, canceling previous request");
        cancelPendingRequests();
    }

    isRequestPending = true;

    LOG_INF("Generating completion");

    // Use the provided system prompt or the default one
    std::string actualSystemPrompt = systemPrompt.empty() ? defaultSystemPrompt : systemPrompt;

    // Get the default provider from the manager
    std::string defaultProvider = providerManager->getDefaultProviderName();

    // Send the request to the provider manager
    return providerManager->sendRequest(
        defaultProvider,
        prompt,
        actualSystemPrompt,
        [this, callback](const std::string& rawResponse) {
            // Mark the request as no longer pending
            isRequestPending = false;

            // Process the raw response
            processRawResponse(rawResponse, callback);
        });
}

void CompletionLayer::cancelPendingRequests()
{
    if (isRequestPending)
    {
        LOG_INF("Canceling pending requests");
        isRequestPending = false;

        // Cancel requests in the provider manager
        if (providerManager)
        {
            providerManager->cancelPendingRequests();
        }
    }
}

void CompletionLayer::setSystemPrompt(const std::string& systemPrompt)
{
    defaultSystemPrompt = systemPrompt;
    LOG_INF("System prompt set: " << systemPrompt);
}

std::string CompletionLayer::getSystemPrompt() const
{
    return defaultSystemPrompt;
}

void CompletionLayer::processRawResponse(
    const std::string& rawResponse,
    std::function<void(const std::string&, const std::string&)> callback)
{
    LOG_INF("Processing raw response");

    // Check if the response is an error
    if (rawResponse.find("error:") == 0)
    {
        LOG_ERR("Error in raw response: " << rawResponse);
        if (callback)
        {
            callback("", rawResponse);
        }
        return;
    }

    // Extract the components from the response
    std::string text;
    std::string feedback;

    extractResponseComponents(rawResponse, text, feedback);

    LOG_INF("Extracted text: " << text);
    LOG_INF("Extracted feedback: " << feedback);

    // Call the callback with the extracted components
    if (callback)
    {
        callback(text, feedback);
    }
}

void CompletionLayer::extractResponseComponents(
    const std::string& rawResponse,
    std::string& text,
    std::string& feedback)
{
    // Default values
    text = rawResponse;
    feedback = "";


    // Extract JSON from markdown code blocks if present
    std::string jsonContent = rawResponse;

    // Check if the response is wrapped in markdown code blocks
    std::regex codeBlockRegex("```json\\s*\\n(\\{[\\s\\S]*?\\})\\s*\\n```");
    std::smatch matches;

    if (std::regex_search(rawResponse, matches, codeBlockRegex) && matches.size() > 1) {
        // Found JSON inside markdown code block
        jsonContent = matches[1];
    }

    try
    {
        // Try to parse the response as JSON
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(jsonContent);

        Poco::JSON::Object::Ptr responseObj = result.extract<Poco::JSON::Object::Ptr>();

        // Extract text field
        if (responseObj->has("text"))
        {
            text = responseObj->getValue<std::string>("text");
            std::cout << "Found text field: " << text << std::endl;
        }

        // Extract feedback field
        if (responseObj->has("feedback"))
        {
            feedback = responseObj->getValue<std::string>("feedback");
            std::cout << "Found feedback field: " << feedback << std::endl;
        }


        return;

    }
    catch (const std::exception& e)
    {
        LOG_WRN("Failed to parse response as JSON: " << e.what());
        LOG_WRN("Raw response: " << rawResponse);
        LOG_WRN("Extracted JSON content: " << jsonContent);
    }
}