#ifndef MOCK_RESPONSES_HPP
#define MOCK_RESPONSES_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <Poco/Dynamic/Var.h>

/**
 * @brief Helper class for managing mock responses for testing
 */
class MockResponses
{
public:
    /**
     * @brief Get a mock response for a given model and prompt
     *
     * @param modelName The name of the model
     * @param prompt The prompt text
     * @param jsonFilePath Path to the JSON file containing mock responses
     * @return std::string The mock response as a JSON string
     */
    static std::string getMockResponse(const std::string& modelName, const std::string& prompt,
                                       const std::string& jsonFilePath)
    {
        try
        {
            // Read the mock_responses.json file
            std::ifstream file(jsonFilePath);
            if (!file.is_open())
            {
                return createErrorResponse("Failed to open mock responses file");
            }

            // Parse the JSON file
            std::string jsonContent((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
            file.close();

            Poco::JSON::Parser parser;
            Poco::Dynamic::Var result = parser.parse(jsonContent);
            Poco::JSON::Object::Ptr rootObj = result.extract<Poco::JSON::Object::Ptr>();

            // Extract the responses array
            Poco::JSON::Array::Ptr responses = rootObj->getArray("responses");

            // Find a matching response
            std::string responseStr;
            bool found = false;

            // First try to find an exact match
            for (size_t i = 0; i < responses->size(); i++)
            {
                Poco::JSON::Object::Ptr respObj = responses->getObject(i);
                if (respObj->getValue<std::string>("model") == modelName &&
                    respObj->getValue<std::string>("prompt") == prompt)
                {
                    // Found an exact match
                    Poco::JSON::Object::Ptr response = respObj->getObject("response");
                    std::ostringstream oss;
                    response->stringify(oss);
                    responseStr = oss.str();
                    found = true;
                    break;
                }
            }

            // If no exact match, try to find a match for the model with default prompt
            if (!found)
            {
                for (size_t i = 0; i < responses->size(); i++)
                {
                    Poco::JSON::Object::Ptr respObj = responses->getObject(i);
                    if (respObj->getValue<std::string>("model") == modelName &&
                        respObj->getValue<std::string>("prompt") == "*")
                    {
                        // Found a model match with default prompt
                        Poco::JSON::Object::Ptr response = respObj->getObject("response");
                        std::ostringstream oss;
                        response->stringify(oss);
                        responseStr = oss.str();
                        found = true;
                        break;
                    }
                }
            }

            // If still no match, use the default response
            if (!found)
            {
                for (size_t i = 0; i < responses->size(); i++)
                {
                    Poco::JSON::Object::Ptr respObj = responses->getObject(i);
                    if (respObj->getValue<std::string>("model") == "default" &&
                        respObj->getValue<std::string>("prompt") == "*")
                    {
                        // Found the default response
                        Poco::JSON::Object::Ptr response = respObj->getObject("response");
                        std::ostringstream oss;
                        response->stringify(oss);
                        responseStr = oss.str();
                        found = true;
                        break;
                    }
                }
            }

            if (!found)
            {
                // If we still don't have a response, create a generic one
                return createGenericResponse(modelName, prompt);
            }

            return responseStr;
        }
        catch (const std::exception& e)
        {
            return createErrorResponse(std::string("Error processing mock response: ") + e.what());
        }
    }

    /**
     * @brief Create a generic response for when no matching response is found
     *
     * @param modelName The name of the model
     * @param prompt The prompt text
     * @return std::string The generic response as a JSON string
     */
    static std::string createGenericResponse(const std::string& modelName,
                                             const std::string& prompt)
    {
        return "{\"status\":\"success\",\"model\":\"" + modelName +
               "\",\"result\":\"This is a generic mock response for model " + modelName +
               " and prompt '" + prompt + "'.\"}";
    }

    /**
     * @brief Create an error response
     *
     * @param errorMessage The error message
     * @return std::string The error response as a JSON string
     */
    static std::string createErrorResponse(const std::string& errorMessage)
    {
        return "{\"status\":\"error\",\"message\":\"" + errorMessage + "\"}";
    }
};

#endif // MOCK_RESPONSES_HPP
