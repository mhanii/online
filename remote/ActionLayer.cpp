#include <config.h>

#include "ActionLayer.hpp"
#include "LLMProviderManager.hpp"
#include "ClientSession.hpp"
#include "Log.hpp"

#include <iostream>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <sstream>
#include <regex>

ActionLayer::ActionLayer(std::shared_ptr<ClientSession> session,
                       std::shared_ptr<LLMProviderManager> _providerManager)
    : clientSession(session)
    , providerManager(_providerManager)
    , defaultSystemPrompt("Generate UNO commands that can be executed in LibreOffice.")
{
    LOG_INF("ActionLayer created");
}

ActionLayer::~ActionLayer()
{
    LOG_INF("ActionLayer destroyed");
}

bool ActionLayer::processGeneratedText(
    const std::string& commandsPrompt,
    const std::string& text,
    const std::string& feedback,
    std::function<void(const std::string&, const std::string&, const std::string&)> callback)
{
    LOG_INF("Processing command generation prompt: " << commandsPrompt);

    // If the commands prompt is empty, just return the existing text and feedback
    if (commandsPrompt.empty()) {
        if (callback) {
            callback("[]", text, feedback);
        }
        return true;
    }

    // Get the default provider from the manager
    std::string defaultProvider = providerManager->getDefaultProviderName();

    // Send the command prompt to the LLM to generate actual commands
    return providerManager->sendRequest(
        defaultProvider,
        commandsPrompt,
        defaultSystemPrompt,
        [this, text, feedback, callback](const std::string& rawResponse) {
            LOG_INF("Received command generation response: " << rawResponse);

            // Extract commands from the response
            std::string commandsJson = "[]";
            try {
                // Check if the response is wrapped in markdown code blocks
                std::regex codeBlockRegex("```json\\s*\\n(\\{[\\s\\S]*?\\})\\s*\\n```");
                std::smatch matches;
                std::string jsonContent = rawResponse;

                if (std::regex_search(rawResponse, matches, codeBlockRegex) && matches.size() > 1) {
                    // Found JSON inside markdown code block
                    jsonContent = matches[1];
                    LOG_INF("Extracted JSON from markdown: " << jsonContent);
                }

                LOG_INF("Attempting to parse JSON content: " << jsonContent);


                // Try to parse the response as JSON
                Poco::JSON::Parser parser;
                Poco::Dynamic::Var result = parser.parse(jsonContent);
                Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();
                // If it's a JSON object with a "commands" field, extract it

                if (object->has("commands")) {
                    Poco::JSON::Array::Ptr commandsArray = object->getArray("commands");
                    std::ostringstream oss;
                    commandsArray->stringify(oss);
                    commandsJson = oss.str();
                    LOG_INF("Extracted commands array from JSON object: " << commandsJson);
                }

            } catch (const std::exception& e) {
                LOG_WRN("Failed to parse response as JSON: " << e.what());

            }

            LOG_INF("Final extracted commands JSON: " << commandsJson);

            // Format the response for the client
            std::string formattedResponse = formatResponse(commandsJson, text, feedback);

            // Call the callback with the processed response
            if (callback) {
                callback(commandsJson, text, feedback);
            }
        });
}

void ActionLayer::setSystemPrompt(const std::string& systemPrompt)
{
    defaultSystemPrompt = systemPrompt;
    LOG_INF("System prompt set: " << systemPrompt);
}

std::string ActionLayer::getSystemPrompt() const
{
    return defaultSystemPrompt;
}

std::vector<std::string> ActionLayer::extractCommands(const std::string& commandsJson)
{
    std::vector<std::string> commandList;

    if (commandsJson.empty() || commandsJson == "[]")
    {
        return commandList;
    }

    try
    {
        // Parse the commands JSON array
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(commandsJson);
        Poco::JSON::Array::Ptr commandsArray = result.extract<Poco::JSON::Array::Ptr>();

        // Process each command in the array
        for (size_t i = 0; i < commandsArray->size(); i++)
        {
            std::string cmd = commandsArray->getElement<std::string>(i);

            // Strip backslashes from the command
            std::string cleanCmd;
            cleanCmd.reserve(cmd.length());
            for (size_t j = 0; j < cmd.length(); j++)
            {
                // Skip backslashes
                if (cmd[j] != '\\')
                {
                    cleanCmd += cmd[j];
                }
            }

            // Remove quotes if they exist
            if (!cleanCmd.empty() && cleanCmd.front() == '"')
            {
                cleanCmd.erase(0, 1);
            }
            if (!cleanCmd.empty() && cleanCmd.back() == '"')
            {
                cleanCmd.pop_back();
            }

            // Add the cleaned command to the list
            if (!cleanCmd.empty())
            {
                commandList.push_back(cleanCmd);
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERR("Error parsing commands JSON: " << e.what());
    }

    return commandList;
}

bool ActionLayer::executeCommand(const std::string& command)
{
    if (command.empty())
    {
        return false;
    }

    LOG_INF("Executing command: " << command);

    // Check if it's a UNO command
    if (command.find(".uno:") != std::string::npos)
    {
        if (clientSession)
        {
            // Get the DocumentBroker from the ClientSession
            std::shared_ptr<DocumentBroker> docBroker = clientSession->getDocumentBroker();
            if (docBroker)
            {
                // Execute the command on the correct thread
                docBroker->addCallback([this, command]() {
                    clientSession->sendTextFrame(command.c_str(), command.length());
                });

                return true;
            }
            else
            {
                LOG_ERR("No DocumentBroker found for session");
                return false;
            }
        }
    }

    return false;
}

std::string ActionLayer::formatResponse(
    const std::string& commands,
    const std::string& text,
    const std::string& feedback)
{
    (void)commands;
    (void)text;
    (void)feedback;

    return "";
}