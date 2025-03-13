#include <config.h>

#include "ModelApiClient.hpp"
#include "ClientSession.hpp"
#include "Log.hpp"
#include "Protocol.hpp"
#include "MockResponses.hpp"
#include <COOLWSD.hpp>

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

#include <thread>
#include <sstream>
#include <fstream>
#include <iostream>

// Gemini API endpoint
const std::string GEMINI_API_ENDPOINT = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent";
// Config file path for API key
const std::string API_CONFIG_PATH = "/etc/OfficeAI/gemini_config.properties";

ModelApiClient::ModelApiClient(std::shared_ptr<ClientSession> session)
    : clientSession(session)
    , apiEndpoint("http://localhost:8080/api/models") // Default endpoint
    , isRequestPending(false)
    , geminiApiKey("")
{
    LOG_INF("ModelApiClient created");
    loadGeminiApiKey();
}

ModelApiClient::~ModelApiClient()
{
    LOG_INF("ModelApiClient destroyed");
    cancelPendingRequests();
}

bool ModelApiClient::sendModelRequest(const std::string& modelName,
                                      const std::vector<std::string>& params,
                                      std::function<void(const std::string&, const std::string&, const std::string&)> responseCallback)
{
    if (isRequestPending)
    {
        LOG_WRN("Request already pending, canceling previous request");
        cancelPendingRequests();
    }

    // Extract the prompt from params
    std::string prompt = "";
    if (!params.empty())
    {
        prompt = params[0];
    }

    // Check if we should use Gemini (either explicitly requested or as default)
    bool useGemini = (modelName == "gemini" || modelName == "gemini-flash" ||
                      modelName == "gemini-1.5-flash" || modelName == "gemini-2.0-flash" ||
                      !geminiApiKey.empty());

    if (useGemini && !geminiApiKey.empty())
    {
        LOG_INF("Using Gemini API for request");
        isRequestPending = true;
        return sendGeminiRequest(prompt, responseCallback);
    }

    // If Gemini is not available or not requested, fall back to the original implementation
    LOG_INF("Using standard model API for request");

    // Create JSON payload
    Poco::JSON::Object requestObj;
    requestObj.set("model", modelName);

    // Add parameters as an array
    Poco::JSON::Array paramsArray;
    for (const auto& param : params)
    {
        paramsArray.add(param);
    }
    requestObj.set("params", paramsArray);

    // Convert to string for the request
    std::ostringstream oss;
    requestObj.stringify(oss);
    std::string payload = oss.str();

    LOG_INF("Sending model request: " << payload);

    // Set the request as pending
    isRequestPending = true;

    // Perform the actual HTTP request
    return performHttpRequest(apiEndpoint, payload, responseCallback);
}

void ModelApiClient::cancelPendingRequests()
{
    if (isRequestPending)
    {
        LOG_INF("Canceling pending requests");
        isRequestPending = false;
        // Additional cleanup if needed
    }
}

void ModelApiClient::setApiEndpoint(const std::string& endpoint)
{
    apiEndpoint = endpoint;
    LOG_INF("API endpoint set to: " << endpoint);
}

std::string ModelApiClient::getApiEndpoint() const { return apiEndpoint; }

void ModelApiClient::handleApiResponse(const std::string& response,
                                       std::function<void(const std::string&, const std::string&, const std::string&)> callback)
{
    isRequestPending = false;

    // Process the response
    LOG_INF("Received API response: " << COOLProtocol::getAbbreviatedMessage(response));
    try
    {
        // Parse JSON response
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(response);
        Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();

        // Check if the response has a result field
        if (object->has("result"))
        {
            // Extract the result object
            Poco::JSON::Object::Ptr resultObj = object->getObject("result");

            // Extract the conversation, snippet, and commands
            std::string conversation = "";
            std::string snippet = "";
            std::string commands = "";

            if (resultObj->has("conversation")) {
                conversation = resultObj->getValue<std::string>("conversation");
            }

            if (resultObj->has("snippet")) {
                snippet = resultObj->getValue<std::string>("snippet");
            }

            if (resultObj->has("commands")) {
                // Convert commands array to JSON string
                Poco::JSON::Array::Ptr commandsArray = resultObj->getArray("commands");
                std::ostringstream commandsStream;
                commandsArray->stringify(commandsStream);
                commands = commandsStream.str();
            }

            LOG_INF("Extracted conversation: " << conversation);
            LOG_INF("Extracted snippet: " << snippet);
            LOG_INF("Extracted commands: " << commands);

            // Call the callback with the extracted data
            if (callback && clientSession)
            {
                // Get the DocumentBroker from the ClientSession
                std::shared_ptr<DocumentBroker> docBroker = clientSession->getDocumentBroker();
                if (docBroker)
                {
                    // Execute the callback on the correct thread
                    docBroker->addCallback([callback, commands, snippet, conversation]() {
                        callback(commands, snippet, conversation);
                    });
                }
                else
                {
                    LOG_ERR("No DocumentBroker found for session");
                    std::string errorMsg = "error: No DocumentBroker found";
                    std::string emptyCommands = "{}";
                    std::string emptySnippet = "";
                    callback(emptyCommands, emptySnippet, errorMsg);
                }
            }
        }
        else
        {
            LOG_ERR("No result field in response");
        if (callback && clientSession)
        {
                std::string errorMsg = "error: No result field in response";
                std::string emptyCommands = "{}";
                std::string emptySnippet = "";

            // Get the DocumentBroker from the ClientSession
            std::shared_ptr<DocumentBroker> docBroker = clientSession->getDocumentBroker();
            if (docBroker)
            {
                // Execute the callback on the correct thread
                    docBroker->addCallback([callback, emptyCommands, emptySnippet, errorMsg]() {
                        callback(emptyCommands, emptySnippet, errorMsg);
                    });
            }
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERR("Error parsing API response: " << e.what());
        if (callback && clientSession)
        {
            std::string errorMsg = "error: " + std::string(e.what());
            std::string emptyCommands = "{}";
            std::string emptySnippet = "";

            // Get the DocumentBroker from the ClientSession
            std::shared_ptr<DocumentBroker> docBroker = clientSession->getDocumentBroker();
            if (docBroker)
            {
                // Execute the callback on the correct thread
                docBroker->addCallback([callback, emptyCommands, emptySnippet, errorMsg]() {
                    callback(emptyCommands, emptySnippet, errorMsg);
                });
            }
        }
    }
}

bool ModelApiClient::performHttpRequest(const std::string& url, const std::string& payload,
                                        std::function<void(const std::string&, const std::string&, const std::string&)> callback)
{
    // REAL HTTP REQUEST MODE
    std::cout << "ModelApiClient: Sending HTTP request to: " << url << std::endl;
    LOG_INF("Sending HTTP request to: " << url);

    try {
        // Create a session
        Poco::URI uri(url);
        Poco::Net::HTTPClientSession session(uri.getHost(), uri.getPort());

        // Set timeout to 30 seconds
        session.setTimeout(Poco::Timespan(30, 0));

        // Create a request
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, uri.getPathAndQuery(), Poco::Net::HTTPMessage::HTTP_1_1);
        request.setContentType("application/json");
        request.setContentLength(payload.length());

        std::cout << "ModelApiClient: Request payload: " << payload << std::endl;

        // Send the request
        std::ostream& os = session.sendRequest(request);
        os << payload;

        // Get the response
        Poco::Net::HTTPResponse response;
        std::istream& rs = session.receiveResponse(response);

        // Read the response
        std::string responseStr;
        Poco::StreamCopier::copyToString(rs, responseStr);

        std::cout << "ModelApiClient: Received HTTP response status: " << response.getStatus() << " " << response.getReason() << std::endl;
        std::cout << "ModelApiClient: Response body (abbreviated): " << responseStr.substr(0, 200) << "..." << std::endl;

        LOG_INF("Received HTTP response: " << COOLProtocol::getAbbreviatedMessage(responseStr));

        // Process the response
        handleApiResponse(responseStr, callback);

        return true;
    }
    catch (const std::exception& e) {
        std::cout << "ModelApiClient: Error sending HTTP request: " << e.what() << std::endl;
        LOG_ERR("Error sending HTTP request: " << e.what());
        if (callback) {
            std::string errorMsg = "Error sending HTTP request: " + std::string(e.what());
            std::string emptyCommands = "{}";
            std::string emptySnippet = "";
            callback(emptyCommands, emptySnippet, errorMsg);
        }
        return false;
    }
}

bool ModelApiClient::isEndpointAvailable(const std::string& url)
{
    try
    {
        Poco::URI uri(url);
        Poco::Net::HTTPClientSession session(uri.getHost(), uri.getPort());
        session.setTimeout(Poco::Timespan(2, 0)); // 2 second timeout

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_HEAD, uri.getPathAndQuery(),
                                       Poco::Net::HTTPMessage::HTTP_1_1);

        session.sendRequest(request);
        Poco::Net::HTTPResponse response;
        session.receiveResponse(response);

        return response.getStatus() < 400; // Consider any non-error status as available
    }
    catch (const std::exception&)
    {
        return false; // Any exception means the endpoint is not available
    }
}

std::string ModelApiClient::createMockResponse(const std::string& payload)
{
    try
    {
        // Parse the payload
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(payload);
        Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();

        // Create a response object
        Poco::JSON::Object responseObj;
        responseObj.set("status", "success");

        // Get the model name
        std::string modelName = "unknown";
        if (object->has("model"))
        {
            modelName = object->getValue<std::string>("model");
        }
        responseObj.set("model", modelName);

        // Get the parameters
        Poco::JSON::Array::Ptr params;
        if (object->has("params"))
        {
            params = object->getArray("params");
        }

        // Create a result object with the new structure
        Poco::JSON::Object resultObj;

        // Add conversation field
        resultObj.set("conversation", "This is a mock conversation response for the " + modelName + " model.");

        // Add snippet field
        resultObj.set("snippet", "This is a mock snippet for the " + modelName + " model.");

        // Add commands array
        Poco::JSON::Array commandsArray;
        commandsArray.add(".uno:Bold");
        commandsArray.add(".uno:Italic");
        commandsArray.add(".uno:InsertText {\"Text\":{\"type\":\"string\",\"value\":\"This is mock text.\"}}");
        resultObj.set("commands", commandsArray);

        // Set the result in the response object
        responseObj.set("result", resultObj);

        // Convert to string
        std::ostringstream oss;
        responseObj.stringify(oss);
        return oss.str();
    }
    catch (const std::exception& e)
    {
        // If parsing fails, return a generic response
        LOG_ERR("Error parsing payload: " << e.what());

        // Create a response object with the new structure
        Poco::JSON::Object responseObj;
        responseObj.set("status", "success");
        responseObj.set("model", "test");

        // Create a result object
        Poco::JSON::Object resultObj;
        resultObj.set("conversation", "This is a test conversation response. The API endpoint is not available.");
        resultObj.set("snippet", "This is a test snippet. The API endpoint is not available.");

        // Add empty commands array
        Poco::JSON::Array commandsArray;
        resultObj.set("commands", commandsArray);

        // Set the result in the response object
        responseObj.set("result", resultObj);

        // Convert to string
        std::ostringstream oss;
        responseObj.stringify(oss);
        return oss.str();
    }
}

std::string ModelApiClient::generateModelResponse(Poco::JSON::Array::Ptr params)
{
    // Extract the prompt from params if available
    std::string prompt = "";
    if (params && params->size() > 0)
    {
        try
        {
            prompt = params->getElement<std::string>(0);
        }
        catch (const std::exception&)
        {
            // If we can't get the prompt, use a default one
            prompt = "default prompt";
        }
    }

    // Use mock responses from the JSON file
    std::string mockResponse =
        MockResponses::getMockResponse("remote/mock_responses.json");
    if (!mockResponse.empty())
    {
        return mockResponse;
    }

    // Fallback responses if no mock response is found
    Poco::JSON::Object responseObj;
    responseObj.set("status", "success");

    // Create a result object with the new structure
    Poco::JSON::Object resultObj;

    // Add commands array
    Poco::JSON::Array commandsArray;

    // Check if the prompt is "Hani" and return special UNO API commands
    if (prompt == "Hani" || prompt == "hani")
    {
        // Set conversation
        resultObj.set("conversation", "I'll add some styled text at the cursor position.");

        // Set snippet
        resultObj.set("snippet", "Hello, Hani!");

        // Add commands
        commandsArray.add(".uno:InsertText {\"Text\":{\"type\":\"string\",\"value\":\"Hello, Hani!\"}}");
        commandsArray.add(".uno:CharFontName {\"FontName\":{\"type\":\"string\",\"value\":\"Arial\"}}");
        commandsArray.add(".uno:Bold");
        commandsArray.add(".uno:FontHeight {\"FontHeight\":{\"type\":\"float\",\"value\":20}}");
        commandsArray.add(".uno:CharBackColor {\"BackColor\":{\"type\":\"long\",\"value\":16777215}}");
    }
    else
    {
        // Default response
        resultObj.set("conversation", "Here's a demonstration of text formatting in the document.");

        // Set snippet
        resultObj.set("snippet", "This text demonstrates formatting capabilities.");

        // Add default commands
        commandsArray.add(".uno:Bold");
        commandsArray.add(".uno:Italic");
        commandsArray.add(".uno:Underline");
        commandsArray.add(".uno:InsertText {\"Text\":{\"type\":\"string\",\"value\":\"This text demonstrates formatting capabilities.\"}}");
    }

    // Add the commands array to the result object
    resultObj.set("commands", commandsArray);

    // Add the result object to the response
    responseObj.set("result", resultObj);

    // Convert to string
    std::ostringstream oss;
    responseObj.stringify(oss);
    return oss.str();
}

void ModelApiClient::loadGeminiApiKey()
{
    std::cout << "ModelApiClient: Loading Gemini API key..." << std::endl;

    // First try to load from config file
    try {
        Poco::File configFile(API_CONFIG_PATH);
        if (configFile.exists()) {
            std::cout << "ModelApiClient: Found configuration file at " << API_CONFIG_PATH << std::endl;
            // Load configuration from property file
            Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> config =
                new Poco::Util::PropertyFileConfiguration(API_CONFIG_PATH);

            // Get API key from config
            if (config->hasProperty("gemini.api_key")) {
                geminiApiKey = config->getString("gemini.api_key");
                std::cout << "ModelApiClient: Successfully loaded Gemini API key from configuration file" << std::endl;
                LOG_INF("Gemini API key loaded from configuration file");
                return;
            } else {
                std::cout << "ModelApiClient: WARNING - No gemini.api_key property found in configuration file" << std::endl;
                LOG_WRN("No Gemini API key found in configuration file");
            }
        } else {
            std::cout << "ModelApiClient: Configuration file not found at: " << API_CONFIG_PATH << std::endl;
            LOG_WRN("Gemini configuration file not found at: " << API_CONFIG_PATH);
        }
    } catch (const std::exception& e) {
        std::cout << "ModelApiClient: ERROR loading API key from file: " << e.what() << std::endl;
        LOG_ERR("Error loading Gemini API key from file: " << e.what());
    }

    // If we couldn't load from file, try environment variable
    std::cout << "ModelApiClient: Trying to load API key from environment variable GEMINI_API_KEY" << std::endl;
    try {
        const char* envApiKey = std::getenv("GEMINI_API_KEY");
        if (envApiKey != nullptr && strlen(envApiKey) > 0) {
            geminiApiKey = envApiKey;
            std::cout << "ModelApiClient: Successfully loaded Gemini API key from environment variable" << std::endl;
            LOG_INF("Gemini API key loaded from environment variable");
            return;
        } else {
            std::cout << "ModelApiClient: WARNING - GEMINI_API_KEY environment variable not set or empty" << std::endl;
            LOG_WRN("No Gemini API key found in environment variable GEMINI_API_KEY");
        }
    } catch (const std::exception& e) {
        std::cout << "ModelApiClient: ERROR loading API key from environment: " << e.what() << std::endl;
        LOG_ERR("Error loading Gemini API key from environment: " << e.what());
    }

    std::cout << "ModelApiClient: WARNING - No Gemini API key found. API will not be available." << std::endl;
    LOG_WRN("No Gemini API key found. API will not be available.");
}

bool ModelApiClient::sendGeminiRequest(const std::string& prompt,
                                    std::function<void(const std::string&, const std::string&, const std::string&)> callback)
{
    if (geminiApiKey.empty()) {
        std::cout << "ModelApiClient: ERROR - Gemini API key not set. Please configure it in " << API_CONFIG_PATH << std::endl;
        LOG_ERR("Gemini API key not set. Please configure it in " << API_CONFIG_PATH);
        if (callback) {
            std::string errorMsg = "Error: Gemini API key not configured. Please contact your administrator.";
            std::string emptyCommands = "{}";
            std::string emptySnippet = "";
            callback(emptyCommands, emptySnippet, errorMsg);
        }
        return false;
    }

    // Debug the exact prompt we're receiving
    std::cout << "ModelApiClient: Received prompt (length: " << prompt.length() << "): \"" << prompt << "\"" << std::endl;

    // If prompt is empty or too short, use a default prompt
    std::string actualPrompt = prompt;
    if (actualPrompt.empty() || actualPrompt.length() < 2) {
        std::cout << "ModelApiClient: WARNING - Prompt is empty or too short, using default prompt" << std::endl;
        actualPrompt = "Please help me create a document with some formatted text.";
    }

    std::cout << "ModelApiClient: Preparing Gemini request for prompt: \"" << actualPrompt << "\"" << std::endl;

    try {
        // Create the request payload for Gemini
        Poco::JSON::Object requestObj;
        Poco::JSON::Array contentsArray;

        // Create a system prompt to instruct Gemini how to format its response
        std::string systemPrompt = "## Core Purpose\n" +
"The system is designed to analyze user inputs and separate them into content generation and document manipulation components. It produces three distinct outputs: text generation instructions, command generation instructions, and feedback when needed.\n\n" +
"## Response Structure\n" +
"- `text_gen_prompt`: Content generation instructions only (no styling information)\n" +
"- `commands_gen_prompt`: Styling, formatting, and document manipulation commands\n" +
"- `feedback`: Used primarily when clarification is needed\n\n" +
"## Processing Guidelines\n" +
"- Never include styling/formatting instructions in text_gen_prompt\n" +
"- Commands can reference specific content (e.g., \"Highlight the section about benefits\")\n" +
"- If the user provides an unclear request, both prompts should be empty and feedback should ask for clarification\n\n" +
"## Examples\n\n" +
"USER: Write me a paragraph about the health benefits of Mediterranean diet.\n" +
"ASSISTANT:\n" +
"```\n" +
"text_gen_prompt: \"Write a paragraph about the health benefits of Mediterranean diet.\"\n" +
"commands_gen_prompt: \"Format the text with appropriate paragraph spacing and use a readable font size.\"\n" +
"feedback: \"\"\n" +
"```\n\n" +
"USER: Make the first two lines bold and change the font color of the third paragraph to blue.\n" +
"ASSISTANT:\n" +
"```\n" +
"text_gen_prompt: \"\"\n" +
"commands_gen_prompt: \"Make the first two lines bold and change the font color of the third paragraph to blue.\"\n" +
"feedback: \"\"\n" +
"```\n\n" +
"USER: Create a three-part outline for a presentation on renewable energy and use bullet points for each section.\n" +
"ASSISTANT:\n" +
"```\n" +
"text_gen_prompt: \"Create a three-part outline for a presentation on renewable energy.\"\n" +
"commands_gen_prompt: \"Format the outline using bullet points for each section.\"\n" +
"feedback: \"\"\n" +
"```\n\n" +
"USER: I want you to please just do the thing now.\n" +
"ASSISTANT:\n" +
"```\n" +
"text_gen_prompt: \"\"\n" +
"commands_gen_prompt: \"\"\n" +
"feedback: \"I'm not sure what you'd like me to do. Could you please provide more specific details about what content you want to create or how you want to format something?\"\n" +
"```";


        std::cout << "ModelApiClient: Using system prompt to guide Gemini's response format" << std::endl;

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

        // Add system instruction in the correct format for v1beta API
        Poco::JSON::Object systemInstructionObj;
        Poco::JSON::Array systemParts;
        Poco::JSON::Object systemPart;
        systemPart.set("text", systemPrompt);
        systemParts.add(systemPart);
        systemInstructionObj.set("parts", systemParts);
        requestObj.set("systemInstruction", systemInstructionObj);

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
        std::string geminiPayload = oss.str();
        std::cout << "ModelApiClient: Gemini request payload: " << geminiPayload << std::endl;
        std::cout << "ModelApiClient: Gemini request payload created, length: " << geminiPayload.length() << " bytes" << std::endl;
        LOG_INF("Sending Gemini request: " << COOLProtocol::getAbbreviatedMessage(geminiPayload));

        // Set up HTTPS connection - use v1beta endpoint which supports systemInstruction
        std::string fullUrl = GEMINI_API_ENDPOINT + "?key=" + geminiApiKey;
        std::cout << "ModelApiClient: Connecting to Gemini API at: " << GEMINI_API_ENDPOINT << std::endl;

        Poco::URI uri(fullUrl);
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
        request.setContentLength(geminiPayload.length());

        std::cout << "ModelApiClient: Sending request to Gemini API..." << std::endl;

        // Send request
        std::ostream& os = session.sendRequest(request);
        os << geminiPayload;

        std::cout << "ModelApiClient: Request sent, waiting for response..." << std::endl;

        // Get response
        Poco::Net::HTTPResponse response;
        std::istream& rs = session.receiveResponse(response);

        // Read response
        std::string responseStr;
        Poco::StreamCopier::copyToString(rs, responseStr);

        std::cout << "ModelApiClient: Received Gemini response, status: " << response.getStatus() << " " << response.getReason() << std::endl;
        std::cout << "ModelApiClient: Response size: " << responseStr.length() << " bytes" << std::endl;
        LOG_INF("Received Gemini response: " << COOLProtocol::getAbbreviatedMessage(responseStr));

        // Process the Gemini response
        std::cout << "ModelApiClient: Processing Gemini response..." << std::endl;
        processGeminiResponse(responseStr, callback);

        return true;
    }
    catch (const std::exception& e) {
        std::cout << "ModelApiClient: ERROR sending Gemini request: " << e.what() << std::endl;
        LOG_ERR("Error sending Gemini request: " << e.what());
        if (callback) {
            std::string errorMsg = "Error sending Gemini request: " + std::string(e.what());
            std::string emptyCommands = "{}";
            std::string emptySnippet = "";
            callback(emptyCommands, emptySnippet, errorMsg);
        }
        return false;
    }
}

void ModelApiClient::processGeminiResponse(const std::string& response,
                                      std::function<void(const std::string&, const std::string&, const std::string&)> callback)
{
    try {
        std::cout << "ModelApiClient: Parsing Gemini response JSON..." << std::endl;

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
            std::cout << "ModelApiClient: ERROR from Gemini API - Code " << errorCode << ": " << errorMessage << std::endl;
            LOG_ERR("Gemini API error " << errorCode << ": " << errorMessage);

            if (callback) {
                std::string emptyCommands = "{}";
                std::string emptySnippet = "";
                callback(emptyCommands, emptySnippet, "Error from Gemini API: " + errorMessage);
            }
            return;
        }

        // Extract the response text
        std::cout << "ModelApiClient: Extracting text from Gemini response..." << std::endl;
        std::string responseText = "";
        if (object->has("candidates")) {
            Poco::JSON::Array::Ptr candidates = object->getArray("candidates");
            if (candidates->size() > 0) {
                Poco::JSON::Object::Ptr candidate = candidates->getObject(0);

                // Check for finish reason
                if (candidate->has("finishReason") &&
                    candidate->getValue<std::string>("finishReason") != "STOP") {
                    std::string finishReason = candidate->getValue<std::string>("finishReason");
                    std::cout << "ModelApiClient: WARNING - Response was truncated. Finish reason: " << finishReason << std::endl;
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
                                std::cout << "ModelApiClient: Successfully extracted text, length: " << responseText.length() << " bytes" << std::endl;
                            }
                        }
                    }
                }
            }
        }

        if (responseText.empty()) {
            std::cout << "ModelApiClient: ERROR - No text found in Gemini response" << std::endl;
            LOG_ERR("No text found in Gemini response");
            if (callback) {
                std::string emptyCommands = "{}";
                std::string emptySnippet = "";
                callback(emptyCommands, emptySnippet, "No text found in Gemini response");
            }
            return;
        }

        LOG_INF("Extracted text from Gemini response: " << COOLProtocol::getAbbreviatedMessage(responseText));

        // Extract conversation (everything except code blocks)
        std::string conversation = responseText;

        // Extract snippet - look for code blocks
        std::cout << "ModelApiClient: Looking for code snippets in response..." << std::endl;
        std::string snippet = "";
        size_t codeStart = responseText.find("```");
        if (codeStart != std::string::npos) {
            size_t codeEnd = responseText.find("```", codeStart + 3);
            if (codeEnd != std::string::npos) {
                // Extract the code without the backticks
                snippet = responseText.substr(codeStart + 3, codeEnd - codeStart - 3);

                // Remove language identifier if present
                size_t newlinePos = snippet.find('\n');
                if (newlinePos != std::string::npos && newlinePos < 20) {
                    snippet = snippet.substr(newlinePos + 1);
                }

                // Trim whitespace
                snippet.erase(0, snippet.find_first_not_of(" \t\r\n"));
                snippet.erase(snippet.find_last_not_of(" \t\r\n") + 1);

                std::cout << "ModelApiClient: Found code snippet, length: " << snippet.length() << " bytes" << std::endl;
                LOG_INF("Extracted code snippet: " << COOLProtocol::getAbbreviatedMessage(snippet));
            }
        } else {
            std::cout << "ModelApiClient: No code snippets found in response" << std::endl;
        }

        // Extract UNO commands
        std::cout << "ModelApiClient: Extracting UNO commands from response..." << std::endl;
        Poco::JSON::Array commandsArray;
        std::istringstream iss(responseText);
        std::string line;
        while (std::getline(iss, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            // Check if this line contains a UNO command
            if (line.find(".uno:") != std::string::npos) {
                commandsArray.add(line);
                std::cout << "ModelApiClient: Found UNO command: " << line << std::endl;
                LOG_INF("Found UNO command: " << line);
            }
        }

        if (commandsArray.size() == 0) {
            std::cout << "ModelApiClient: WARNING - No UNO commands found in Gemini response" << std::endl;
            LOG_WRN("No UNO commands found in Gemini response");
        } else {
            std::cout << "ModelApiClient: Found " << commandsArray.size() << " UNO commands in response" << std::endl;
            LOG_INF("Found " << commandsArray.size() << " UNO commands in Gemini response");
        }

        // Create our result structure
        std::cout << "ModelApiClient: Creating formatted response structure..." << std::endl;
        Poco::JSON::Object resultObj;
        resultObj.set("conversation", conversation);
        resultObj.set("snippet", snippet);
        resultObj.set("commands", commandsArray);

        // Create the full response object
        Poco::JSON::Object responseObj;
        responseObj.set("status", "success");
        responseObj.set("model", "gemini-2.0-flash");
        responseObj.set("result", resultObj);

        // Convert to string
        std::ostringstream oss;
        responseObj.stringify(oss);
        std::string formattedResponse = oss.str();

        // Process the response using our existing method
        std::cout << "ModelApiClient: Sending formatted response to handler..." << std::endl;
        handleApiResponse(formattedResponse, callback);
    }
    catch (const std::exception& e) {
        std::cout << "ModelApiClient: ERROR processing Gemini response: " << e.what() << std::endl;
        LOG_ERR("Error processing Gemini response: " << e.what());
        if (callback) {
            std::string errorMsg = "Error processing Gemini response: " + std::string(e.what());
            std::string emptyCommands = "{}";
            std::string emptySnippet = "";
            callback(emptyCommands, emptySnippet, errorMsg);
        }
    }
}
