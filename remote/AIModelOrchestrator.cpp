#include <config.h>

#include "AIModelOrchestrator.hpp"
#include "LLMProviderManager.hpp"
#include "PromptLayer.hpp"
#include "CompletionLayer.hpp"
#include "ActionLayer.hpp"
#include "ClientSession.hpp"
#include "RController.hpp"
#include "Log.hpp"

#include <iostream>

AIModelOrchestrator::AIModelOrchestrator(std::shared_ptr<ClientSession> session)
    : clientSession(session)
    , isRequestPending(false)
{
    LOG_INF("AIModelOrchestrator created");

    // Initialize the provider manager
    providerManager = std::make_shared<LLMProviderManager>(session);

    // Initialize the layers
    promptLayer = std::make_shared<PromptLayer>(session, providerManager);
    completionLayer = std::make_shared<CompletionLayer>(session, providerManager);
    actionLayer = std::make_shared<ActionLayer>(session, providerManager);

    // Get or create the RController
    std::string sessionId = session->getId();
    rController = RController::getForSession(sessionId);
    if (!rController) {
        rController = RController::createForSession(session);
        if (!rController) {
            LOG_ERR("Failed to create RController for session " << sessionId);
        } else {
            LOG_INF("Created new RController for session " << sessionId);
        }
    }

    // Set default system prompts
    systemPrompts["prompt"] = "Think a step by step,analyze user inputs and separate them into content generation and document manipulation components. Produce three outputs: text generation instructions, command generation instructions, and feedback when needed. USER: <input> ASSISTANT: {\"text_gen_prompt\": \"<content generation instructions>\", \"commands_gen_prompt\": \"<styling and formatting commands>\", \"feedback\": \"<clarification if needed>\"} Processing Guidelines: Never include styling in text_gen_prompt. Commands can reference specific content. If request is unclear, both prompts should be empty and feedback should ask for clarification. Example 1: USER: Write me a paragraph about Mediterranean diet ASSISTANT: {\"text_gen_prompt\": \"Write an elaborated and well-detailed paragraph about Mediterranean diet\", \"commands_gen_prompt\": \"Format with appropriate spacing,identation.Add styles like bold,italic, shadowed etc.. when it is relevant\", \"feedback\": \"\"} Example 2: USER: Make first two lines bold and third paragraph blue ASSISTANT: {\"text_gen_prompt\": \"\", \"commands_gen_prompt\": \"Go to first two lines and make them bold and then go to third paragraph and make it blue\", \"feedback\": \"\"} Example 3: USER: Create a document about renewable energy with bullet points ASSISTANT: {\"text_gen_prompt\": \"Create a high-quality well-explained text about renewable enerygy. Add some advantages and disadvantages.\", \"commands_gen_prompt\": \"Format the part of advantages and disadvanteges with bullet points. Use good styling practices such as bold title, highlight important words, indentation etc...\", \"feedback\": \"\"} Example 4: USER: unclear request ASSISTANT: {\"text_gen_prompt\": \"\", \"commands_gen_prompt\": \"\", \"feedback\": \"Could you please clarify what you'd like me to help you with?\"}";

    systemPrompts["completion"] = "Think a step by step, generate helpful, accurate, and concise responses. Return results as JSON with 'text' containing the generated text and 'feedback' explaining what was done. Example: {\"text\": \"The Mediterranean diet is rich in vegetables, fruits, whole grains, and healthy fats.\", \"feedback\": \"Generated a concise description of Mediterranean diet focusing on key components.\"}";

    systemPrompts["action"] = "You generate styled text with UNO commands for document editors, producing JSON responses with just UNO styling commands.\n\nInput Format: You will receive two pieces of information:\n1. USER INPUT: The original request from the user\n2. GENERATED TEXT: The text content generated based on the user's request\n\nYour task is to think a step by step, analyze those components and generate appropriate UNO commands to format the generated text according to the user's intent.\n\nResponse Structure: commands: Array of UNO commands\n\nCommand Format: Each command must be just the command name. Commands are separated by commas. Each command must be surrounded by quotes. Example: [\"Bold\", \"InsertText {\\\"Text\\\":{\\\"type\\\":\\\"string\\\",\\\"value\\\":\\\"Hello\\\"}}\", \"Bold\"]\n\nKey Styling Guidelines: Use hierarchical styling for document sections. Create visual contrast between elements. Apply content-specific styling for different document types. Adapt styling based on context (reports, letters, resumes, etc.)\n\nImportant Implementation Notes: Most styling commands act as toggles (must be turned off explicitly). Color values are numeric (e.g., 0 for black, 16711680 for blue). Text cursor position follows insertion.\n\nAvailable UNO Commands with Parameters:\nText Insertion:\n- InsertText {\\\"Text\\\":{\\\"type\\\":\\\"string\\\",\\\"value\\\":\\\"Your text here\\\"}} - Add text\n- InsertPara - Create a new line/paragraph\n- InsertAnnotation {\\\"Author\\\":{\\\"type\\\":\\\"string\\\",\\\"value\\\":\\\"Author name\\\"},\\\"Html\\\":{\\\"type\\\":\\\"string\\\",\\\"value\\\":\\\"Your comment\\\"}} - Add a comment\n- InsertPagebreak - Create a new page\n- SetHyperlink {\\\"Hyperlink.Text\\\":{\\\"type\\\":\\\"string\\\",\\\"value\\\":\\\"Link text\\\"},\\\"Hyperlink.URL\\\":{\\\"type\\\":\\\"string\\\",\\\"value\\\":\\\"http://example.com\\\"}} - Insert a hyperlink\n\nShapes (Selected):\n- SymbolShapes.heart - Insert a heart shape\n- BasicShapes.rectangle - Insert a rectangle shape\n- FillColor {\\\"FillColor.Color\\\":{\\\"type\\\":\\\"long\\\",\\\"value\\\":0}} - Change shape fill color\n\nText Styling:\n- CharBackColor {\\\"CharBackColor.Color\\\":{\\\"type\\\":\\\"long\\\",\\\"value\\\":0}} - Highlight text\n- LeftPara - Align text left\n- RightPara - Align text right\n- CenterPara - Center align text\n- FontColor {\\\"FontColor.Color\\\":{\\\"type\\\":\\\"long\\\",\\\"value\\\":0}} - Change font color\n- SwBackspace - Delete selected text\n- Strikeout - Apply strikethrough\n- Underline - Underline text\n- Bold - Make text bold\n- Grow - Increase font size\n- Shrink - Decrease font size\n\nSelection and Navigation:\n- SelectAll - Select all content\n- SelectWord - Select current word\n- GoToEndOfLine - Move cursor to end of line\n- GoToStartOfPage - Move cursor to start of page\n\nTables:\n- InsertTable { \\\"Columns\\\": { \\\"type\\\": \\\"long\\\",\\\"value\\\": 2 }, \\\"Rows\\\": { \\\"type\\\": \\\"long\\\",\\\"value\\\": 3 }} - Insert a table\n- JumpToNextCell - Navigate to next cell in table\n- JumpToPrevCell - Navigate to previous cell in table\n\nDocument Structure:\n- IncrementIndent - Increase paragraph indentation\n- DecrementIndent - Decrease paragraph indentation\n- DefaultBullet - Create unordered list\n- DefaultNumbering - Create ordered list\n\nExample Use Case - Creating a Heading:\n[\"Bold\", \"Grow\", \"Grow\", \"InsertText {\\\"Text\\\":{\\\"type\\\":\\\"string\\\",\\\"value\\\":\\\"Document Title\\\"}}\", \"Bold\", \"InsertPara\"]";

    // Apply the default system prompts
    promptLayer->setSystemPrompt(systemPrompts["prompt"]);
    completionLayer->setSystemPrompt(systemPrompts["completion"]);
    actionLayer->setSystemPrompt(systemPrompts["action"]);
}

AIModelOrchestrator::~AIModelOrchestrator()
{
    LOG_INF("AIModelOrchestrator destroyed");
    cancelPendingRequests();
}

bool AIModelOrchestrator::processModelRequest(
    const std::string& prompt,
    std::function<void(const std::string&, const std::string&, const std::string&)> responseCallback)
{
    if (isRequestPending)
    {
        LOG_WRN("Request already pending, canceling previous request");
        cancelPendingRequests();
    }

    // Set the request as pending
    isRequestPending = true;

    LOG_INF("Processing model request with prompt length: " << prompt.length());

    // Create a callback for when the entire pipeline is complete
    auto finalCallback = [this, responseCallback](
        const std::string& commands,
        const std::string& text,
        const std::string& feedback)
    {
        // Mark the request as no longer pending
        isRequestPending = false;

        // Execute commands if available
        if (!commands.empty() && commands != "[]") {
            LOG_INF("Dispatching commands via RController: " << commands);
            if (rController) {
                bool success = rController->dispatchCommands(commands);
                LOG_INF("Command dispatch " << (success ? "successful" : "failed"));
                std::cout << "Command dispatch " << (success ? "successful" : "failed") << std::endl;
            } else {
                LOG_ERR("RController not available for command dispatch");
            }
        } else {
            LOG_INF("No commands to dispatch");
        }

        // Call the original callback
        if (responseCallback)
        {
            responseCallback(commands, text, feedback);
        }
    };

    // Start the pipeline with the prompt layer
    return promptLayer->processPrompt(
        prompt,
        systemPrompts["prompt"],
        [this, finalCallback, prompt](
            const std::string& text_gen_prompt,
            const std::string& commands_gen_prompt,
            const std::string& feedback)
        {
            std::cout << "text_gen_prompt: " << text_gen_prompt << std::endl;
            std::cout << "commands_gen_prompt: " << commands_gen_prompt << std::endl;
            std::cout << "feedback: " << feedback << std::endl;

            // If we have feedback, return it directly
            if (!feedback.empty()) {
                finalCallback("", "", feedback);
                return;
            }

            // Track if we need to process text generation
            bool hasTextPrompt = !text_gen_prompt.empty();
            bool hasCommandsPrompt = !commands_gen_prompt.empty();

            // If we have neither, return empty response
            if (!hasTextPrompt && !hasCommandsPrompt) {
                finalCallback("", "", "");
                return;
            }

            // If we only have commands prompt, process directly through action layer
            if (!hasTextPrompt && hasCommandsPrompt) {
                actionLayer->processGeneratedText(
                    commands_gen_prompt,
                    "",  // No text
                    "",  // No feedback
                    finalCallback);
                return;
            }

            // If we have text prompt, process it through completion layer
            completionLayer->generateCompletion(
                text_gen_prompt,
                systemPrompts["completion"],
                [this, commands_gen_prompt, hasCommandsPrompt, finalCallback](
                    const std::string& text,
                    const std::string& completionFeedback)
                {

                    // If we also have commands prompt, continue to the action layer
                    std::cout << "text: " << text << std::endl;
                    std::cout << "completionFeedback: " << completionFeedback << std::endl;
                    if (hasCommandsPrompt) {
                        // Modify the commands_gen_prompt to include the original user prompt and generated text
                        std::string enhancedPrompt = "USER INPUT: " + commands_gen_prompt + "\n\nGENERATED TEXT: " + text ;
                        std::cout << "enhancedPrompt: " << enhancedPrompt << std::endl;
                        actionLayer->processGeneratedText(
                            enhancedPrompt,
                            text,
                            "",
                            finalCallback);
                    } else {
                        // If no commands prompt, just pass through the text
                        finalCallback("", text, "");
                    }
                });
        });
}

void AIModelOrchestrator::cancelPendingRequests()
{
    if (isRequestPending)
    {
        LOG_INF("Canceling pending requests");
        isRequestPending = false;

        // Cancel requests in all layers
        if (completionLayer)
        {
            completionLayer->cancelPendingRequests();
        }

        if (providerManager)
        {
            providerManager->cancelPendingRequests();
        }
    }
}

void AIModelOrchestrator::setProviderEndpoint(const std::string& providerName, const std::string& endpoint)
{
    if (providerManager)
    {
        providerManager->setProviderEndpoint(providerName, endpoint);
        LOG_INF("Set endpoint for provider " << providerName << ": " << endpoint);
    }
}

std::string AIModelOrchestrator::getProviderEndpoint(const std::string& providerName) const
{
    if (providerManager)
    {
        return providerManager->getProviderEndpoint(providerName);
    }
    return "";
}

void AIModelOrchestrator::setSystemPrompt(const std::string& layerName, const std::string& systemPrompt)
{
    // Store the system prompt
    systemPrompts[layerName] = systemPrompt;

    // Apply it to the appropriate layer
    if (layerName == "prompt" && promptLayer)
    {
        promptLayer->setSystemPrompt(systemPrompt);
    }
    else if (layerName == "completion" && completionLayer)
    {
        completionLayer->setSystemPrompt(systemPrompt);
    }
    else if (layerName == "action" && actionLayer)
    {
        actionLayer->setSystemPrompt(systemPrompt);
    }

    LOG_INF("Set system prompt for layer " << layerName);
}