#include <config.h>

#include "PromptLayer.hpp"
#include "LLMProviderManager.hpp"
#include "ClientSession.hpp"
#include "Log.hpp"

#include <iostream>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <regex>

PromptLayer::PromptLayer(std::shared_ptr<ClientSession> session,
                       std::shared_ptr<LLMProviderManager> _providerManager)
    : clientSession(session)
    , providerManager(_providerManager)
    , defaultSystemPrompt("You are an AI assistant helping with document editing. Format your response clearly.")
{
    LOG_INF("PromptLayer created");
}

PromptLayer::~PromptLayer()
{
    LOG_INF("PromptLayer destroyed");
}

bool PromptLayer::processPrompt(
    const std::string& rawPrompt,
    const std::string& systemPrompt,
    std::function<void(const std::string&, const std::string&, const std::string&)> callback)
{
    LOG_INF("Processing prompt");

    // Format the prompt
    std::string formattedPrompt = formatPrompt(rawPrompt);

    // Add context to the prompt if needed
    std::string contextualPrompt = addContextToPrompt(formattedPrompt);

    // Use the provided system prompt or the default one
    std::string actualSystemPrompt = systemPrompt.empty() ? defaultSystemPrompt : systemPrompt;

    LOG_INF("Sending prompt to LLM: " << contextualPrompt);

    // Get the default provider from the manager
    std::string defaultProvider = providerManager->getDefaultProviderName();

    // Send the prompt to the LLM for processing
    return providerManager->sendRequest(
        defaultProvider,
        contextualPrompt,
        actualSystemPrompt,
        [callback](const std::string& response) {
            LOG_INF("Received response from LLM");

            // Parse the response to extract text_gen_prompt, commands_gen_prompt, and feedback
            std::string text_gen_prompt;
            std::string commands_gen_prompt;
            std::string feedback;


            // Extract JSON from markdown code blocks if present
            std::string jsonContent = response;

            // Check if the response is wrapped in markdown code blocks
            std::regex codeBlockRegex("```json\\s*\\n(\\{[\\s\\S]*?\\})\\s*\\n```");
            std::smatch matches;

            if (std::regex_search(response, matches, codeBlockRegex) && matches.size() > 1) {
                // Found JSON inside markdown code block
                jsonContent = matches[1];
            }

            try {
                // Try to parse the response as JSON
                Poco::JSON::Parser parser;
                Poco::Dynamic::Var result = parser.parse(jsonContent);
                Poco::JSON::Object::Ptr responseObj = result.extract<Poco::JSON::Object::Ptr>();

                // Extract the components
                if (responseObj->has("text_gen_prompt")) {
                    text_gen_prompt = responseObj->getValue<std::string>("text_gen_prompt");
                }

                if (responseObj->has("commands_gen_prompt")) {
                    commands_gen_prompt = responseObj->getValue<std::string>("commands_gen_prompt");
                }

                if (responseObj->has("feedback")) {
                    feedback = responseObj->getValue<std::string>("feedback");
                }


            } catch (const std::exception& e) {
                LOG_ERR("Failed to parse LLM response as JSON: " << e.what());
                LOG_ERR("Raw response: " << response);
                LOG_ERR("Extracted JSON content: " << jsonContent);

                // If parsing fails, use the raw response as feedback
                feedback = "Error processing your request. Please try again.";
            }

            // Call the callback with the processed components
            if (callback) {
                callback(text_gen_prompt, commands_gen_prompt, feedback);
            }
        });
}

void PromptLayer::setSystemPrompt(const std::string& systemPrompt)
{
    defaultSystemPrompt = systemPrompt;
    LOG_INF("System prompt set: " << systemPrompt);
}

std::string PromptLayer::getSystemPrompt() const
{
    return defaultSystemPrompt;
}

std::string PromptLayer::formatPrompt(const std::string& rawPrompt)
{
    (void)rawPrompt;

    // For now, just return the raw prompt
    // In the future, this could be expanded to format the prompt differently

    return rawPrompt;
}

std::string PromptLayer::addContextToPrompt(const std::string& formattedPrompt)
{
    (void)formattedPrompt;
    // For now, just return the formatted prompt
    // In the future, this could be expanded to add context to the prompt
    // such as document information, user preferences, etc.

    return formattedPrompt;
}