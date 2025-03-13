#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

// Forward declarations
class ClientSession;
class LLMProviderManager;

/**
 * ActionLayer - Handles command generation and execution
 *
 * This class is responsible for processing the generated text from the
 * CompletionLayer and extracting commands that can be executed. It also
 * handles the execution of these commands.
 */
class ActionLayer {
public:
    // Constructor
    ActionLayer(std::shared_ptr<ClientSession> session,
               std::shared_ptr<LLMProviderManager> _providerManager);

    // Destructor
    virtual ~ActionLayer();

    // Process the generated text and extract commands
    bool processGeneratedText(
        const std::string& commandsPrompt,
        const std::string& text,
        const std::string& feedback,
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

    // Extract commands from a JSON string
    std::vector<std::string> extractCommands(const std::string& commandsJson);

    // Execute a command
    bool executeCommand(const std::string& command);

    // Format the response for the client
    std::string formatResponse(
        const std::string& commands,
        const std::string& text,
        const std::string& feedback);
};