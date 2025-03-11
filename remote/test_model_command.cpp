#include <config.h>

#include "ModelApiClient.hpp"
#include "Log.hpp"
#include "Protocol.hpp"

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <sstream>

// Simple test class that mimics RController functionality
class TestController
{
public:
    TestController() { std::cout << "TestController created" << std::endl; }

    ~TestController() { std::cout << "TestController destroyed" << std::endl; }

    bool executeCommand(const char* buffer, int length)
    {
        if (!buffer || length <= 0)
        {
            return false;
        }

        // Extract the first line to get the command
        std::string message(buffer, length);
        std::string firstLine;
        size_t newlinePos = message.find('\n');
        if (newlinePos != std::string::npos)
        {
            firstLine = message.substr(0, newlinePos);
        }
        else
        {
            firstLine = message;
        }

        // Tokenize the command
        std::vector<std::string> tokens;
        std::istringstream iss(firstLine);
        std::string token;
        while (iss >> token)
        {
            tokens.push_back(token);
        }

        // Check if this is a model command
        if (!tokens.empty() && tokens[0] == "model")
        {
            // Handle model command
            return processModelCommand(message, tokens);
        }

        std::cout << "Not a model command: " << firstLine << std::endl;
        return false;
    }

    bool processModelCommand(const std::string& command, const std::vector<std::string>& tokens)
    {
        // Mark parameter as used to avoid compiler warning
        (void)command;

        if (tokens.size() < 2)
        {
            // Need at least "model" and a model name
            std::cout << "Error: Invalid syntax for model command" << std::endl;
            return false;
        }

        const std::string& modelName = tokens[1];
        std::vector<std::string> params(tokens.begin() + 2, tokens.end());

        std::cout << "Processing model command for model: " << modelName << std::endl;

        // In a real implementation, this would call the API client
        std::cout << "Would send request to model API with parameters:" << std::endl;
        for (const auto& param : params)
        {
            std::cout << "  - " << param << std::endl;
        }

        return true;
    }
};

// This is a simple test program to demonstrate the model command processing
int main(int argc, char** argv)
{
    // Mark parameters as used to avoid compiler warnings
    (void)argc;
    (void)argv;

    // Initialize logging
    Log::initialize("test_model_command", "trace", true, false, {}, false, {});

    try
    {
        // Create a test controller
        TestController controller;

        // Example model command
        const char* modelCommand = "model gpt4 generate \"Write a short poem about coding\"";

        // Execute the command
        bool result = controller.executeCommand(modelCommand, strlen(modelCommand));

        std::cout << "Command execution result: " << (result ? "success" : "failure") << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}