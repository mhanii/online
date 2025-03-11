# RController System

The RController system provides a way to handle model-related commands from client sessions. It intercepts commands that start with the "model" keyword and forwards them to an external API for processing.

## Components

### RController

The `RController` class is responsible for:
- Intercepting and processing model-related commands
- Managing the lifecycle of controller instances
- Forwarding responses back to the client

### ModelApiClient

The `ModelApiClient` class handles the communication with external model APIs:
- Sends HTTP requests to the API endpoint
- Processes API responses
- Provides asynchronous communication

## Usage

### Client-Side Commands

To use the model API from the client side, send a command in the following format:

```
model <model_name> <action> [parameters...]
```

For example:
```
model gpt4 generate "Write a short poem about coding"
```

### Server-Side Integration

The system is integrated with the `ClientSession` class:
- Each client session has its own `RController` instance
- Commands starting with "model" are intercepted and processed by the RController
- Responses are sent back to the client via the ClientSession

### API Configuration

The default API endpoint is `http://localhost:8080/api/models`, but this can be configured:

```cpp
// Get the controller for a session
auto controller = RController::getForSession(sessionId);
if (controller) {
    // Get the API client
    auto apiClient = controller->getApiClient();
    if (apiClient) {
        // Set a custom API endpoint
        apiClient->setApiEndpoint("https://api.example.com/models");
    }
}
```

## Response Format

Responses from the model API are sent back to the client in the following format:

```
modelresponse <status> <model_name> <json_response>
```

For example:
```
modelresponse success gpt4 {"model":"gpt4","result":"Here is a poem about coding..."}
```

## Error Handling

Errors are reported back to the client with an appropriate status:

```
modelresponse error <error_type> <error_message>
```

Error types include:
- `invalid_syntax`: Invalid command syntax
- `http_error`: Error communicating with the API
- `parsing_error`: Error parsing the API response

## Thread Safety

The RController system is thread-safe:
- Controller instances are managed with a mutex
- API requests are processed asynchronously
- Callbacks are used to handle responses 