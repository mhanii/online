# Mock Response System for LLM API Testing

This document describes the mock response system used for testing the LLM API integration without making actual HTTP requests.

## Overview

The mock response system allows you to test the LLM API integration by providing predefined responses for specific model and prompt combinations. This is useful for:

1. Development and testing without an active LLM API endpoint
2. Ensuring consistent responses for automated testing
3. Testing edge cases and error handling

## How It Works

The system uses a JSON file (`mock_responses.json`) to store predefined responses. When a request is made to the LLM API, the system:

1. Extracts the model name and prompt from the request
2. Looks for a matching response in the JSON file
3. Returns the predefined response instead of making an HTTP request

## Mock Response JSON Format

The `mock_responses.json` file has the following structure:

```json
{
  "responses": [
    {
      "model": "model_name",
      "prompt": "prompt_text",
      "response": {
        "status": "success",
        "model": "model_name",
        "result": "response_text"
      }
    },
    ...
  ]
}
```

Each entry in the `responses` array contains:
- `model`: The name of the model (e.g., "gpt4")
- `prompt`: The prompt text to match
- `response`: The complete response object to return

## Response Matching Logic

The system uses the following logic to find a matching response:

1. First, it looks for an exact match of both model name and prompt
2. If no exact match is found, it looks for a match with the same model name and a wildcard prompt ("*")
3. If still no match, it looks for a default response (model="default", prompt="*")
4. If no match is found at all, it generates a generic response

## Snippet Format for UNO Commands

The response text can include snippets that contain UNO commands to be executed. The format is:

```
[snippet:Title of the Snippet]
.uno:Command1 {"Param1":{"type":"string","value":"Value1"}}
.uno:Command2
.uno:Command3 {"Param2":{"type":"long","value":123}}
[/snippet]
```

These snippets will be parsed by the RController, and the UNO commands will be executed.

## How to Add New Mock Responses

To add a new mock response:

1. Open the `mock_responses.json` file
2. Add a new entry to the `responses` array with your model name, prompt, and response
3. Make sure the response follows the correct format with status, model, and result fields
4. If you want to include UNO commands, use the snippet format described above

## Switching Between Mock and Real Requests

The `performHttpRequest` method in `ModelApiClient.cpp` contains two sections:
1. MOCK RESPONSE MODE - Active by default
2. REAL HTTP REQUEST MODE - Commented out

To switch between them:
1. Comment out the MOCK RESPONSE MODE section
2. Uncomment the REAL HTTP REQUEST MODE section

## Example Mock Responses

The default `mock_responses.json` includes examples for:
- Text formatting commands
- Document operations
- Paragraph formatting
- Table operations
- Special "Hani" command that demonstrates multiple UNO commands

## Troubleshooting

If you encounter issues with the mock response system:

1. Check that the `mock_responses.json` file exists in the correct location
2. Verify that the JSON format is valid
3. Check the logs for any error messages related to parsing the JSON file
4. Ensure that the UNO commands in your snippets are correctly formatted 