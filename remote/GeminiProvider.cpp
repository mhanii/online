#include <config.h>

#include "GeminiProvider.hpp"
#include "ClientSession.hpp"
#include "Log.hpp"
#include "Protocol.hpp"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/SecureStreamSocket.h>
#include <Poco/Net/Context.h>
#include <Poco/URI.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>
#include <Poco/Path.h>
#include <Poco/File.h>
#include <Poco/Util/PropertyFileConfiguration.h>

#include <iostream>
#include <sstream>
#include <fstream>

// Config file path for API key
const std::string API_CONFIG_PATH = "/etc/OfficeAI/gemini_config.properties";
// Default API endpoint
const std::string DEFAULT_API_ENDPOINT = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent";

GeminiProvider::GeminiProvider(std::shared_ptr<ClientSession> session)
    : LLMProvider(session)
    , apiEndpoint(DEFAULT_API_ENDPOINT)
    , apiKey("")
    , isRequestPending(false)
{
    LOG_INF("GeminiProvider created");
    loadApiKey();
}

GeminiProvider::~GeminiProvider()
{
    LOG_INF("GeminiProvider destroyed");
    cancelPendingRequests();
}

void GeminiProvider::loadApiKey()
{
    std::cout << "GeminiProvider: Loading API key..." << std::endl;

    // First try to load from config file
    try {
        Poco::File configFile(API_CONFIG_PATH);
        if (configFile.exists()) {
            std::cout << "GeminiProvider: Found configuration file at " << API_CONFIG_PATH << std::endl;
            // Load configuration from property file
            Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> config =
                new Poco::Util::PropertyFileConfiguration(API_CONFIG_PATH);

            // Get API key from config
            if (config->hasProperty("gemini.api_key")) {
                apiKey = config->getString("gemini.api_key");
                std::cout << "GeminiProvider: Successfully loaded API key from configuration file" << std::endl;
                LOG_INF("Gemini API key loaded from configuration file");
                return;
            } else {
                std::cout << "GeminiProvider: WARNING - No gemini.api_key property found in configuration file" << std::endl;
                LOG_WRN("No Gemini API key found in configuration file");
            }
        } else {
            std::cout << "GeminiProvider: Configuration file not found at: " << API_CONFIG_PATH << std::endl;
            LOG_WRN("Gemini configuration file not found at: " << API_CONFIG_PATH);
        }
    } catch (const std::exception& e) {
        std::cout << "GeminiProvider: ERROR loading API key from file: " << e.what() << std::endl;
        LOG_ERR("Error loading Gemini API key from file: " << e.what());
    }

    // If we couldn't load from file, try environment variable
    std::cout << "GeminiProvider: Trying to load API key from environment variable GEMINI_API_KEY" << std::endl;
    try {
        const char* envApiKey = std::getenv("GEMINI_API_KEY");
        if (envApiKey != nullptr && strlen(envApiKey) > 0) {
            apiKey = envApiKey;
            std::cout << "GeminiProvider: Successfully loaded API key from environment variable" << std::endl;
            LOG_INF("Gemini API key loaded from environment variable");
            return;
        } else {
            std::cout << "GeminiProvider: WARNING - GEMINI_API_KEY environment variable not set or empty" << std::endl;
            LOG_WRN("No Gemini API key found in environment variable GEMINI_API_KEY");
        }
    } catch (const std::exception& e) {
        std::cout << "GeminiProvider: ERROR loading API key from environment: " << e.what() << std::endl;
        LOG_ERR("Error loading Gemini API key from environment: " << e.what());
    }

    std::cout << "GeminiProvider: WARNING - No API key found. API will not be available." << std::endl;
    LOG_WRN("No Gemini API key found. API will not be available.");
}

bool GeminiProvider::sendRequest(
    const std::string& prompt,
    const std::string& systemPrompt,
    std::function<void(const std::string&)> responseCallback)
{
    if (apiKey.empty()) {
        std::cout << "GeminiProvider: ERROR - API key not set. Please configure it in " << API_CONFIG_PATH << std::endl;
        LOG_ERR("Gemini API key not set. Please configure it in " << API_CONFIG_PATH);
        if (responseCallback) {
            responseCallback("error: Gemini API key not configured. Please contact your administrator.");
        }
        return false;
    }

    if (isRequestPending) {
        LOG_WRN("Request already pending, canceling previous request");
        cancelPendingRequests();
    }

    isRequestPending = true;

    // Debug the exact prompt we're receiving
    std::cout << "GeminiProvider: Received prompt (length: " << prompt.length() << "): \"" << prompt << "\"" << std::endl;

    // If prompt is empty or too short, use a default prompt
    std::string actualPrompt = prompt;
    if (actualPrompt.empty() || actualPrompt.length() < 2) {
        std::cout << "GeminiProvider: WARNING - Prompt is empty or too short, using default prompt" << std::endl;
        actualPrompt = "Please help me create a document with some formatted text.";
    }

    std::cout << "GeminiProvider: Preparing request for prompt: \"" << actualPrompt << "\"" << std::endl;

    try {
        // Create the request payload
        Poco::JSON::Object requestObj;
        Poco::JSON::Array contentsArray;

        // Add system instruction in the correct format for v1beta API
        if (!systemPrompt.empty()) {
            Poco::JSON::Object systemInstructionObj;
            Poco::JSON::Array systemParts;
            Poco::JSON::Object systemPart;
            systemPart.set("text", systemPrompt);
            systemParts.add(systemPart);
            systemInstructionObj.set("parts", systemParts);
            requestObj.set("systemInstruction", systemInstructionObj);

            std::cout << "GeminiProvider: Using system prompt: " << systemPrompt << std::endl;
        }

        // Add user message with prompt
        Poco::JSON::Object userContent;
        Poco::JSON::Array userParts;
        Poco::JSON::Object userPart;
        userPart.set("text", actualPrompt);
        userParts.add(userPart);
        userContent.set("role", "user");
        userContent.set("parts", userParts);
        contentsArray.add(userContent);

        requestObj.set("contents", contentsArray);

        // Add generation config
        Poco::JSON::Object genConfig;
        genConfig.set("temperature", 1); // Slightly higher temperature for more creative responses
        genConfig.set("maxOutputTokens", 2048);
        requestObj.set("generationConfig", genConfig);

        // Add safety settings to allow more flexibility
        Poco::JSON::Array safetySettings;

        // Define safety categories and set them to allow more content
        const std::vector<std::string> categories = {
            "HARM_CATEGORY_HARASSMENT",
            "HARM_CATEGORY_HATE_SPEECH",
            "HARM_CATEGORY_SEXUALLY_EXPLICIT",
            "HARM_CATEGORY_DANGEROUS_CONTENT"
        };

        for (const auto& category : categories) {
            Poco::JSON::Object safetySetting;
            safetySetting.set("category", category);
            safetySetting.set("threshold", "BLOCK_NONE");
            safetySettings.add(safetySetting);
        }

        requestObj.set("safetySettings", safetySettings);

        // Convert to string
        std::ostringstream oss;
        requestObj.stringify(oss);
        std::string payload = oss.str();

        std::cout << "GeminiProvider: Request payload created, length: " << payload.length() << " bytes" << std::endl;
        LOG_INF("Sending Gemini request: " << COOLProtocol::getAbbreviatedMessage(payload));

        // Set up HTTPS connection
        std::string fullUrl = apiEndpoint + "?key=" + apiKey;
        std::cout << "GeminiProvider: Connecting to API at: " << apiEndpoint << std::endl;

        // Perform the HTTP request
        return performHttpRequest(fullUrl, payload, responseCallback);
    }
    catch (const std::exception& e) {
        std::cout << "GeminiProvider: ERROR preparing request: " << e.what() << std::endl;
        LOG_ERR("Error preparing Gemini request: " << e.what());
        isRequestPending = false;
        if (responseCallback) {
            responseCallback("error: Error preparing Gemini request: " + std::string(e.what()));
        }
        return false;
    }
}

void GeminiProvider::cancelPendingRequests()
{
    if (isRequestPending) {
        LOG_INF("Canceling pending requests");
        isRequestPending = false;
    }
}

void GeminiProvider::setEndpoint(const std::string& endpoint)
{
    apiEndpoint = endpoint;
    LOG_INF("API endpoint set to: " << endpoint);
}

std::string GeminiProvider::getEndpoint() const
{
    return apiEndpoint;
}

bool GeminiProvider::isAvailable() const
{
    return !apiKey.empty();
}

bool GeminiProvider::performHttpRequest(
    const std::string& url,
    const std::string& payload,
    std::function<void(const std::string&)> callback)
{
    try {
        Poco::URI uri(url);
        Poco::Net::Context::Ptr context = new Poco::Net::Context(
            Poco::Net::Context::CLIENT_USE, "", "", "",
            Poco::Net::Context::VERIFY_RELAXED, 9, false,
            "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");

        Poco::Net::HTTPSClientSession session(uri.getHost(), uri.getPort(), context);

        // Set timeout to 30 seconds
        session.setTimeout(Poco::Timespan(30, 0));

        // Create request
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST,
                                      uri.getPathAndQuery(),
                                      Poco::Net::HTTPMessage::HTTP_1_1);
        request.setContentType("application/json");
        request.setContentLength(payload.length());

        std::cout << "GeminiProvider: Sending request..." << std::endl;

        // Send request
        std::ostream& os = session.sendRequest(request);
        os << payload;

        std::cout << "GeminiProvider: Request sent, waiting for response..." << std::endl;

        // Get response
        Poco::Net::HTTPResponse response;
        std::istream& rs = session.receiveResponse(response);

        // Read response
        std::string responseStr;
        Poco::StreamCopier::copyToString(rs, responseStr);

        std::cout << "GeminiProvider: Received response, status: " << response.getStatus() << " " << response.getReason() << std::endl;
        std::cout << "GeminiProvider: Response size: " << responseStr.length() << " bytes" << std::endl;
        LOG_INF("Received Gemini response: " << COOLProtocol::getAbbreviatedMessage(responseStr));

        // Process the response
        processResponse(responseStr, callback);

        return true;
    }
    catch (const std::exception& e) {
        std::cout << "GeminiProvider: ERROR sending request: " << e.what() << std::endl;
        LOG_ERR("Error sending Gemini request: " << e.what());
        isRequestPending = false;
        if (callback) {
            callback("error: Error sending Gemini request: " + std::string(e.what()));
        }
        return false;
    }
}

void GeminiProvider::processResponse(
    const std::string& response,
    std::function<void(const std::string&)> callback)
{
    isRequestPending = false;
    std::cout << "GeminiProvider: Processing response: " << response << std::endl;
    try {
        std::cout << "GeminiProvider: Parsing response JSON..." << std::endl;

        // Parse JSON response
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(response);
        Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();

        // Check for errors
        if (object->has("error")) {
            Poco::JSON::Object::Ptr errorObj = object->getObject("error");
            std::string errorMessage = errorObj->getValue<std::string>("message");
            int errorCode = 0;
            if (errorObj->has("code")) {
                errorCode = errorObj->getValue<int>("code");
            }
            std::cout << "GeminiProvider: ERROR from API - Code " << errorCode << ": " << errorMessage << std::endl;
            LOG_ERR("Gemini API error " << errorCode << ": " << errorMessage);

            if (callback) {
                callback("error: " + errorMessage);
            }
            return;
        }

        // Extract the response text
        std::cout << "GeminiProvider: Extracting text from response..." << std::endl;
        std::string responseText = "";
        if (object->has("candidates")) {
            Poco::JSON::Array::Ptr candidates = object->getArray("candidates");
            if (candidates->size() > 0) {
                Poco::JSON::Object::Ptr candidate = candidates->getObject(0);

                // Check for finish reason
                if (candidate->has("finishReason") &&
                    candidate->getValue<std::string>("finishReason") != "STOP") {
                    std::string finishReason = candidate->getValue<std::string>("finishReason");
                    std::cout << "GeminiProvider: WARNING - Response was truncated. Finish reason: " << finishReason << std::endl;
                    LOG_WRN("Gemini response was truncated. Finish reason: " << finishReason);
                }

                if (candidate->has("content")) {
                    Poco::JSON::Object::Ptr content = candidate->getObject("content");
                    if (content->has("parts")) {
                        Poco::JSON::Array::Ptr parts = content->getArray("parts");
                        if (parts->size() > 0) {
                            Poco::JSON::Object::Ptr part = parts->getObject(0);
                            if (part->has("text")) {
                                responseText = part->getValue<std::string>("text");
                                std::cout << "GeminiProvider: Successfully extracted text, length: " << responseText.length() << " bytes" << std::endl;
                            }
                        }
                    }
                }
            }
        }

        if (responseText.empty()) {
            std::cout << "GeminiProvider: ERROR - No text found in response" << std::endl;
            LOG_ERR("No text found in Gemini response");
            if (callback) {
                callback("error: No text found in Gemini response");
            }
            return;
        }

        LOG_INF("Extracted text from Gemini response: " << COOLProtocol::getAbbreviatedMessage(responseText));

        // Call the callback with the extracted text
        if (callback) {
            callback(responseText);
        }
    }
    catch (const std::exception& e) {
        std::cout << "GeminiProvider: ERROR processing response: " << e.what() << std::endl;
        LOG_ERR("Error processing Gemini response: " << e.what());
        if (callback) {
            callback("error: Error processing Gemini response: " + std::string(e.what()));
        }
    }
}
