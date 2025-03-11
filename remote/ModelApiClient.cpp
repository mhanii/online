#include <config.h>

#include "ModelApiClient.hpp"
#include "ClientSession.hpp"
#include "Log.hpp"
#include "Protocol.hpp"
#include "MockResponses.hpp"
#include <COOLWSD.hpp>

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>

#include <thread>
#include <sstream>
#include <fstream>

ModelApiClient::ModelApiClient(std::shared_ptr<ClientSession> session)
    : clientSession(session)
    , apiEndpoint("http://localhost:8080/api/models") // Default endpoint
    , isRequestPending(false)
{
    LOG_INF("ModelApiClient created");
}

ModelApiClient::~ModelApiClient()
{
    LOG_INF("ModelApiClient destroyed");
    cancelPendingRequests();
}

bool ModelApiClient::sendModelRequest(const std::string& modelName,
                                      const std::vector<std::string>& params,
                                      std::function<void(const std::string&)> responseCallback)
{
    if (isRequestPending)
    {
        LOG_WRN("Request already pending, canceling previous request");
        cancelPendingRequests();
    }

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
                                       std::function<void(const std::string&)> callback)
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

        // Extract the result from the response
        std::string formattedResponse;

        if (object->has("result"))
        {
            Poco::Dynamic::Var resultVar = object->get("result");

            // Check if result is an object with the new format (conversation, snippet, commands)
            if (resultVar.isStruct())
            {
                try
                {
                    Poco::JSON::Object::Ptr resultObj =
                        resultVar.extract<Poco::JSON::Object::Ptr>();

                    // Pass the structured result directly to the client
                    std::ostringstream oss;
                    object->stringify(oss);
                    formattedResponse = "modelresponse success " + oss.str();
                }
                catch (const std::exception& e)
                {
                    LOG_ERR("Error extracting structured result: " << e.what());
                    // Fallback to string representation
                    std::string resultText = object->getValue<std::string>("result");
                    formattedResponse = "modelresponse success " + resultText;
                }
            }
            else
            {
                // Handle regular string result
                std::string resultText = object->getValue<std::string>("result");
                formattedResponse = "modelresponse success " + resultText;
            }
        }
        else
        {
            formattedResponse = "modelresponse success No result provided in the response";
        }

        LOG_INF("Sending formatted response to client: "
                << COOLProtocol::getAbbreviatedMessage(formattedResponse));

        // Call the callback with the formatted response
        if (callback && clientSession)
        {
            // Get the DocumentBroker from the ClientSession
            std::shared_ptr<DocumentBroker> docBroker = clientSession->getDocumentBroker();
            if (docBroker)
            {
                // Execute the callback on the correct thread
                docBroker->addCallback([callback, formattedResponse]()
                                       { callback(formattedResponse); });
            }
            else
            {
                LOG_ERR("No DocumentBroker found for session");
                callback("modelresponse error No DocumentBroker found");
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERR("Error parsing API response: " << e.what());
        if (callback && clientSession)
        {
            std::string errorResponse = "modelresponse error " + std::string(e.what());

            // Get the DocumentBroker from the ClientSession
            std::shared_ptr<DocumentBroker> docBroker = clientSession->getDocumentBroker();
            if (docBroker)
            {
                // Execute the callback on the correct thread
                docBroker->addCallback([callback, errorResponse]() { callback(errorResponse); });
            }
            else
            {
                LOG_ERR("No DocumentBroker found for session");
                callback("modelresponse error No DocumentBroker found");
            }
        }
    }
}

bool ModelApiClient::performHttpRequest(const std::string& url, const std::string& payload,
                                        std::function<void(const std::string&)> callback)
{
    (void)url;
    // MOCK RESPONSE MODE - Read from JSON file instead of making HTTP request
    // Comment out this section when you want to use real HTTP requests
    LOG_INF("Using mock response from JSON file instead of HTTP request");

    try
    {
        // Parse the payload to get the model and prompt
        Poco::JSON::Parser payloadParser;
        Poco::Dynamic::Var payloadResult = payloadParser.parse(payload);
        Poco::JSON::Object::Ptr payloadObj = payloadResult.extract<Poco::JSON::Object::Ptr>();

        std::string modelName = payloadObj->getValue<std::string>("model");
        Poco::JSON::Array::Ptr params = payloadObj->getArray("params");
        std::string prompt = "";
        if (params && params->size() > 0)
        {
            try
            {
                prompt = params->getElement<std::string>(0);
            }
            catch (const std::exception&)
            {
                prompt = "*"; // Default prompt if we can't extract it
            }
        }

        LOG_INF("Looking for mock response for model: " << modelName << ", prompt: " << prompt);

        // Get the mock response using the MockResponses class
        std::string responseStr = MockResponses::getMockResponse(
            modelName, prompt, COOLWSD::FileServerRoot + "/remote/mock_responses.json");

        // Call the callback with the mock response
        if (callback)
        {
            // Add a small delay to simulate network latency (optional)
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Process the response
            handleApiResponse(responseStr, callback);
        }

        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERR("Error processing mock response: " << e.what());
        if (callback)
        {
            callback("{\"status\":\"error\",\"message\":\"Error processing mock response: " +
                     std::string(e.what()) + "\"}");
        }
        return false;
    }

    // REAL HTTP REQUEST MODE - Uncomment this section when you want to use real HTTP requests
    /*
    LOG_INF("Sending HTTP request to: " << url);

    try {
        // Create a session
        Poco::Net::HTTPClientSession session(Poco::URI(url).getHost(), Poco::URI(url).getPort());

        // Create a request
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, Poco::URI(url).getPathAndQuery(), Poco::Net::HTTPMessage::HTTP_1_1);
        request.setContentType("application/json");
        request.setContentLength(payload.length());

        // Send the request
        std::ostream& os = session.sendRequest(request);
        os << payload;

        // Get the response
        Poco::Net::HTTPResponse response;
        std::istream& rs = session.receiveResponse(response);

        // Read the response
        std::string responseStr;
        Poco::StreamCopier::copyToString(rs, responseStr);

        LOG_INF("Received HTTP response: " << COOLProtocol::getAbbreviatedMessage(responseStr));

        // Process the response
        handleApiResponse(responseStr, callback);

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERR("Error sending HTTP request: " << e.what());
        if (callback) {
            callback("{\"status\":\"error\",\"message\":\"Error sending HTTP request: " +
                     std::string(e.what()) + "\"}");
        }
        return false;
    }
    */
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

        // Generate a response based on the model
        std::string responseText;
        if (modelName == "gpt4" || modelName == "GPT-4")
        {
            responseText = generateModelResponse(params);
        }
        else
        {
            responseText = "This is a test response from the " + modelName +
                           " model. The API endpoint is not available, so this is a mock response.";
        }

        // Set the result in the response object
        responseObj.set("result", responseText);

        // Convert to string
        std::ostringstream oss;
        responseObj.stringify(oss);
        return oss.str();
    }
    catch (const std::exception& e)
    {
        // If parsing fails, return a generic response
        LOG_ERR("Error parsing payload: " << e.what());
        return "{\"status\":\"success\",\"model\":\"test\",\"result\":\"This is a test response. "
               "The API endpoint is not available, and the payload could not be parsed.\"}";
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
        MockResponses::getMockResponse("gpt4", prompt, "remote/mock_responses.json");
    if (!mockResponse.empty())
    {
        return mockResponse;
    }

    // Fallback responses if no mock response is found

    // Check if the prompt is "Hani" and return special UNO API commands
    if (prompt == "Hani" || prompt == "hani")
    {
        return "I'll add some styled text at the cursor position:\n\n"
               "[snippet:Insert Text at Cursor]"
               ".uno:InsertText {\"Text\":{\"type\":\"string\",\"value\":\"Hello, Hani!\"}}\n"
               ".uno:CharFontName {\"FontName\":{\"type\":\"string\",\"value\":\"Arial\"}}\n"
               ".uno:Bold\n"
               ".uno:FontHeight {\"FontHeight\":{\"type\":\"float\",\"value\":20}}\n"
               ".uno:CharBackColor {\"BackColor\":{\"type\":\"long\",\"value\":16777215}}"
               "[/snippet]\n\n"
               "This will insert 'Hello, Hani!' at the cursor position, make it bold, set the font "
               "to Arial, "
               "increase the font size, and add a background color.";
    }

    // Default response with both text and snippet formats
    return "Here's a demonstration of both text types in the sidebar:\n\n"
           "This is regular text that will be displayed normally in the conversation. "
           "It can include multiple paragraphs and will flow naturally in the message bubble.\n\n"
           "Below is a snippet that demonstrates text formatting commands:\n\n"
           "[snippet:Text Formatting Commands]"
           ".uno:Bold\n"
           ".uno:Italic\n"
           ".uno:Underline"
           "[/snippet]\n\n"
           "You can also see another snippet with document operations:\n\n"
           "[snippet:Document Operations]"
           ".uno:SelectAll\n"
           ".uno:Copy\n"
           ".uno:Paste"
           "[/snippet]\n\n"
           "And here's a snippet showing a range of lines to modify:\n\n"
           "[snippet:Lines 10-15]"
           "This text would replace lines 10-15 in the document.\n"
           "It can span multiple lines and will be displayed with proper formatting."
           "[/snippet]\n\n"
           "After the snippets, we can continue with regular text. This demonstrates how both "
           "types "
           "can be mixed in a single response to provide a better user experience.";
}
