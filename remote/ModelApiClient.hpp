#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <Poco/JSON/Array.h>

// Forward declaration
class ClientSession;

/**
 * ModelApiClient - Handles communication with external model APIs
 *
 * This class is responsible for sending requests to and receiving responses
 * from external model APIs. It provides an abstraction layer between the
 * RController and the actual API endpoints.
 */
class ModelApiClient {
public:
    // Constructor
    explicit ModelApiClient(std::shared_ptr<ClientSession> session);

    // Destructor
    virtual ~ModelApiClient();

    // Send a model request to the API
    bool sendModelRequest(const std::string& modelName,
                         const std::vector<std::string>& params,
                         std::function<void(const std::string&)> responseCallback);

    // Cancel any pending requests
    void cancelPendingRequests();

    // Set API endpoint
    void setApiEndpoint(const std::string& endpoint);

    // Get the current API endpoint
    std::string getApiEndpoint() const;

private:
    std::shared_ptr<ClientSession> clientSession;
    std::string apiEndpoint;
    bool isRequestPending;

    // Internal method to handle API responses
    void handleApiResponse(const std::string& response,
                           std::function<void(const std::string&)> callback);

    // Perform the actual HTTP request
    bool performHttpRequest(const std::string& url,
                           const std::string& payload,
                           std::function<void(const std::string&)> callback);

    // Check if an endpoint is available
    bool isEndpointAvailable(const std::string& url);

    // Create a mock response for testing
    std::string createMockResponse(const std::string& payload);

    // Generate a model response
    std::string generateModelResponse(Poco::JSON::Array::Ptr params);
};
