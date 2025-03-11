/* -*- js-indent-level: 8 -*- */
/*
 * Copyright the Collabora Online contributors.
 *
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
 * JSDialog.ChatSidebar
 */

/* global app */
interface ChatSidebarOptions {
    animSpeed: number;
    collapsed: boolean;
}

interface MessageItem {
    type: 'input' | 'response';
    content: string;
    contentType?: 'text' | 'snippet'; // Default is 'text'
    snippetHeader?: string; // Optional header for snippets (action name or line range)
    timestamp: Date;
}

class ChatSidebar {
    options: ChatSidebarOptions;
    map: any;
    container: HTMLDivElement;
    builder: any;
    targetDeckCommand: string;
    
    // Input elements
    chatInput: HTMLTextAreaElement;
    chatSendButton: HTMLButtonElement;
    chatHistory: string[] = [];
    chatHistoryIndex: number = -1;
    
    
    // Conversation elements
    conversationContainer: HTMLDivElement;
    messages: MessageItem[] = [];
    
    // DOM elements
    headerElement: HTMLDivElement;
    contentElement: HTMLDivElement;
    inputAreaElement: HTMLDivElement;
    collapseButton: HTMLButtonElement;
    isCollapsed: boolean = false;
    
    waitingForResponse: boolean = false;
    currentInputId: number | null = null;

    constructor(
        map: any,
        options: ChatSidebarOptions = {
            animSpeed: 300, /* Default speed: to be used on load */
            collapsed: false,
        },
    ) {
        this.options = options;
        this.isCollapsed = options.collapsed;
        this.onAdd(map);
    }

    onAdd(map: ReturnType<typeof L.map>) {
        this.map = map;
        
        // Ensure the map has a model
        if (!this.map.model) {
            console.log('Creating new model for map');
            this.map.model = new L.Model();
        }

        app.events.on('resize', this.onResize.bind(this));

        this.builder = new L.control.jsDialogBuilder({
            mobileWizard: this,
            map: map,
            cssClass: 'jsdialog ChatSidebar',
        });
        
        // Create main container
        this.container = L.DomUtil.create(
            'div',
            'ChatSidebar-container',
            $('#ChatSidebar-panel').get(0),
        );
        
        // Apply modern styling with flexbox layout - increased width
        this.container.style.cssText = `
            width: 360px !important;
            max-width: 360px !important;
            height: 100% !important;
            display: flex !important;
            flex-direction: column !important;
            background-color: #ffffff !important;
            border-radius: 8px !important;
            box-shadow: 0 2px 10px rgba(0, 0, 0, 0.08) !important;
            overflow: hidden !important;
        `;
        
        // Create sidebar header
        this.headerElement = L.DomUtil.create('div', 'ChatSidebar-header', this.container);
        
        // Apply modern styling to header
        this.headerElement.style.cssText = `
            display: flex !important;
            justify-content: space-between !important;
            align-items: center !important;
            padding: 20px !important;
            border-bottom: 1px solid #e0e0e0 !important;
            background-color: #f8f9fa !important;
        `;
        
        const titleElement = L.DomUtil.create('h2', 'ChatSidebar-title', this.headerElement);
        titleElement.innerText = 'Chat';
        
        // Apply modern styling to title
        titleElement.style.cssText = `
            margin: 0 !important;
            font-size: 16px !important;
            font-weight: 500 !important;
            color: #202124 !important;
        `;
        
        // Create header buttons container
        const headerButtons = L.DomUtil.create('div', 'ChatSidebar-header-buttons', this.headerElement);
        
        // Create collapse button with improved styling
        this.collapseButton = L.DomUtil.create('button', 'ChatSidebar-collapse-btn', headerButtons) as HTMLButtonElement;
        this.collapseButton.innerHTML = '<span class="collapse-icon">&#10094;</span>'; // Left-pointing angle bracket
        this.collapseButton.setAttribute('aria-label', 'Collapse sidebar');
        this.collapseButton.addEventListener('click', () => this.toggleCollapse());
        
        // Apply strong inline styles to override any other styles
        this.collapseButton.style.cssText = `
            background-color: transparent !important;
            border: none !important;
            cursor: pointer !important;
            padding: 4px 8px !important;
            font-size: 14px !important;
            color: #555 !important;
            display: flex !important;
            align-items: center !important;
            justify-content: center !important;
            width: 28px !important;
            height: 28px !important;
            min-width: unset !important;
            border-radius: 4px !important;
        `;
        
        // Create conversation wrapper to control size
        const conversationWrapper = L.DomUtil.create('div', 'conversation-wrapper', this.container);
        
        // Apply modern styling to conversation wrapper
        conversationWrapper.style.cssText = `
            flex: 1 !important;
            overflow: hidden !important;
            padding: 20px !important;
        `;
        
        // Create conversation container
        this.conversationContainer = L.DomUtil.create('div', 'conversation-container', conversationWrapper);
        
        // Apply modern styling to conversation container
        this.conversationContainer.style.cssText = `
            height: 100% !important;
            overflow-y: auto !important;
            padding-right: 8px !important;
            scrollbar-width: thin !important;
            scrollbar-color: #dadce0 transparent !important;
            width: 100% !important;
            box-sizing: border-box !important;
        `;
        
        // Create input area
        this.inputAreaElement = L.DomUtil.create('div', 'sidebar-input-area', this.container);
        
        // Apply modern styling to input area
        this.inputAreaElement.style.cssText = `
            border-top: 1px solid #e0e0e0 !important;
            background-color: #f8f9fa !important;
            padding: 16px !important;
            border-radius: 0 0 8px 8px !important;
        `;
        
        // Create input container
        const inputContainer = L.DomUtil.create('div', 'input-container', this.inputAreaElement);
        
        // Apply modern styling to input container
        inputContainer.style.cssText = `
            display: flex !important;
            flex-direction: column !important;
            background-color: #ffffff !important;
            border-radius: 12px !important;
            border: 1px solid #e0e0e0 !important;
            box-shadow: 0 2px 6px rgba(0, 0, 0, 0.05) !important;
            overflow: hidden !important;
            width: 100% !important;
            max-width: 100% !important;
            box-sizing: border-box !important;
        `;
        
        // Create input field
        this.chatInput = L.DomUtil.create('textarea', 'input-field', inputContainer) as HTMLTextAreaElement;
        this.chatInput.placeholder = 'Type a message...';
        this.chatInput.setAttribute('aria-label', 'Message input');
        this.chatInput.setAttribute('rows', '1');
        
        // Apply modern styling to input field
        this.chatInput.style.cssText = `
            border: none !important;
            outline: none !important;
            resize: none !important;
            padding: 12px 16px !important;
            font-size: 14px !important;
            line-height: 1.5 !important;
            font-family: inherit !important;
            background-color: transparent !important;
            word-wrap: break-word !important;
            overflow-wrap: break-word !important;
            overflow-x: hidden !important;
        `;
        
        // Create footer for buttons
        const inputFooter = L.DomUtil.create('div', 'input-footer', inputContainer);
        
        // Apply styling to footer - reduced height to one-third
        inputFooter.style.cssText = `
            display: flex !important;
            justify-content: space-between !important;
            align-items: center !important;
            padding: 2px 12px !important;
            border-top: 1px solid #f0f0f0 !important;
            height: 28px !important;
        `;
        
        // Create left buttons container
        const leftButtons = L.DomUtil.create('div', 'input-footer-left', inputFooter);
        leftButtons.style.cssText = `
            display: flex !important;
            gap: 8px !important;
        `;
        
        // Move browser/help button to the leftmost position
        const browserButton = L.DomUtil.create('button', 'input-action-btn', leftButtons) as HTMLButtonElement;
        browserButton.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2ZM12 20C7.59 20 4 16.41 4 12C4 7.59 7.59 4 12 4C16.41 4 20 7.59 20 12C20 16.41 16.41 20 12 20ZM11 15H13V17H11V15ZM11 7H13V13H11V7Z" fill="currentColor"/></svg>';
        browserButton.setAttribute('aria-label', 'Help');
        browserButton.setAttribute('title', 'Help');
        
        // Apply styling to browser button
        browserButton.style.cssText = `
            background-color: transparent !important;
            border: none !important;
            cursor: pointer !important;
            padding: 4px !important;
            border-radius: 6px !important;
            color: #5f6368 !important;
            display: flex !important;
            align-items: center !important;
            justify-content: center !important;
            transition: background-color 0.2s !important;
        `;
        
        // Add hover effect
        browserButton.addEventListener('mouseover', () => {
            browserButton.style.backgroundColor = '#f0f0f0';
        });
        browserButton.addEventListener('mouseout', () => {
            browserButton.style.backgroundColor = 'transparent';
        });
        
        // Create image button
        const imageButton = L.DomUtil.create('button', 'input-action-btn', leftButtons) as HTMLButtonElement;
        imageButton.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M19 3H5C3.9 3 3 3.9 3 5V19C3 20.1 3.9 21 5 21H19C20.1 21 21 20.1 21 19V5C21 3.9 20.1 3 19 3ZM19 19H5V5H19V19ZM13.96 12.29L11.21 15.83L9.25 13.47L6.5 17H17.5L13.96 12.29Z" fill="currentColor"/></svg>';
        imageButton.setAttribute('aria-label', 'Add image');
        imageButton.setAttribute('title', 'Add image');
        
        // Apply styling to image button
        imageButton.style.cssText = `
            background-color: transparent !important;
            border: none !important;
            cursor: pointer !important;
            padding: 4px !important;
            border-radius: 6px !important;
            color: #5f6368 !important;
            display: flex !important;
            align-items: center !important;
            justify-content: center !important;
            transition: background-color 0.2s !important;
        `;
        
        // Add hover effect
        imageButton.addEventListener('mouseover', () => {
            imageButton.style.backgroundColor = '#f0f0f0';
        });
        imageButton.addEventListener('mouseout', () => {
            imageButton.style.backgroundColor = 'transparent';
        });
        
        // Add browser icon (globe icon)
        const webBrowserButton = L.DomUtil.create('button', 'input-action-btn', leftButtons) as HTMLButtonElement;
        webBrowserButton.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2ZM11 19.93C7.05 19.44 4 16.08 4 12C4 11.38 4.08 10.79 4.21 10.21L9 15V16C9 17.1 9.9 18 11 18V19.93ZM17.9 17.39C17.64 16.58 16.9 16 16 16H15V13C15 12.45 14.55 12 14 12H8V10H10C10.55 10 11 9.55 11 9V7H13C14.1 7 15 6.1 15 5V4.59C17.93 5.78 20 8.65 20 12C20 14.08 19.2 15.97 17.9 17.39Z" fill="currentColor"/></svg>';
        webBrowserButton.setAttribute('aria-label', 'Browse web');
        webBrowserButton.setAttribute('title', 'Browse web');
        
        // Apply styling to web browser button
        webBrowserButton.style.cssText = `
            background-color: transparent !important;
            border: none !important;
            cursor: pointer !important;
            padding: 4px !important;
            border-radius: 6px !important;
            color: #5f6368 !important;
            display: flex !important;
            align-items: center !important;
            justify-content: center !important;
            transition: background-color 0.2s !important;
        `;
        
        // Add hover effect
        webBrowserButton.addEventListener('mouseover', () => {
            webBrowserButton.style.backgroundColor = '#f0f0f0';
        });
        webBrowserButton.addEventListener('mouseout', () => {
            webBrowserButton.style.backgroundColor = 'transparent';
        });
        
        // Create send button in the footer with icon
        this.chatSendButton = L.DomUtil.create('button', 'input-send-btn', inputFooter) as HTMLButtonElement;
        this.chatSendButton.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M2.01 21L23 12 2.01 3 2 10l15 2-15 2z" fill="currentColor"/></svg>';
        this.chatSendButton.setAttribute('aria-label', 'Send message (Enter)');
        this.chatSendButton.addEventListener('click', () => this.executeInput());
        
        // Apply modern styling to send button - smaller with icon
        this.chatSendButton.style.cssText = `
            background-color: transparent !important;
            border: none !important;
            cursor: pointer !important;
            padding: 4px !important;
            border-radius: 4px !important;
            color: #1a73e8 !important;
            display: flex !important;
            align-items: center !important;
            justify-content: center !important;
            transition: background-color 0.2s !important;
        `;
        
        // Add hover effect
        this.chatSendButton.addEventListener('mouseover', () => {
            this.chatSendButton.style.backgroundColor = '#f0f0f0';
        });
        this.chatSendButton.addEventListener('mouseout', () => {
            this.chatSendButton.style.backgroundColor = 'transparent';
        });
        
        // Set up auto-growing functionality
        this.setupAutoGrowingInput();
        
        // Add key event listeners
        this.chatInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                this.executeInput();
            }
        });

        // Allow paste
        this.chatInput.addEventListener('paste', (e) => {
            e.stopPropagation(); // Allow default paste behavior
        });
        
        this.map.on('chatsidebar', this.onChatSidebar, this);
        this.map.on('jsdialogupdate', this.onJSUpdate, this);
        this.map.on('jsdialogaction', this.onJSAction, this);
        
        // Listen for model responses
        this.map.on('modelresponse', this.onModelResponse, this);
        
        // Set initial collapse state
        if (this.isCollapsed) {
            this.collapse(false);
        }
    }
    
    // Toggle sidebar collapse state
    toggleCollapse() {
        if (this.isCollapsed) {
            this.expand();
        } else {
            this.collapse();
        }
    }
    
    // Collapse the sidebar
    collapse(animate = true) {
        this.isCollapsed = true;
        const wrapper = $('#ChatSidebar-dock-wrapper');
        
        if (!animate) {
            wrapper.addClass('collapsed no-transition');
            setTimeout(() => wrapper.removeClass('no-transition'), 50);
        } else {
            wrapper.addClass('collapsed');
        }
        
        this.collapseButton.innerHTML = '<span class="collapse-icon">&#10095;</span>'; // Right-pointing angle bracket
        this.collapseButton.setAttribute('aria-label', 'Expand sidebar');
        
        this.map._onResize();
    }
    
    // Expand the sidebar
    expand(animate = true) {
        this.isCollapsed = false;
        const wrapper = $('#ChatSidebar-dock-wrapper');
        
        if (!animate) {
            wrapper.removeClass('collapsed').addClass('no-transition');
            setTimeout(() => wrapper.removeClass('no-transition'), 50);
        } else {
            wrapper.removeClass('collapsed');
        }
        
        this.collapseButton.innerHTML = '<span class="collapse-icon">&#10094;</span>'; // Left-pointing angle bracket
        this.collapseButton.setAttribute('aria-label', 'Collapse sidebar');
        
        this.map._onResize();
    }
    
    isInputResponse(response: any): boolean {
        // Check if this is an input response
        return response.command === 'commandresult' || 
               (response.command === 'reply' && response.success !== undefined);
    }
    
    // Process the structured response with conversation, snippet, and commands
    processStructuredResponse(response: any) {
        console.log('Processing structured response:', response);
        
        // If there are commands to execute, do it silently
        if (response.commands && Array.isArray(response.commands)) {
            // Execute each command silently
            this.executeCommandsSilently(response.commands);
        }
        
        // If there's a snippet, add it as a separate message
        if (response.snippet) {
            // Extract snippet header if it exists
            let snippetHeader = '';
            if (typeof response.snippet === 'string') {
                // Check if the snippet has a header in the format [snippet:Header]
                const snippetMatch = response.snippet.match(/\[snippet:(.*?)\]/);
                if (snippetMatch && snippetMatch[1]) {
                    snippetHeader = snippetMatch[1];
                }
            }
            
            // Add the snippet after a small delay to ensure it appears after the conversation
            setTimeout(() => {
                this.addResponseMessage(response.snippet, 'snippet', snippetHeader);
            }, 100);
        }
        
        // If there's a conversation part, it will be handled by the calling method
        // which will return it for display in the normal message flow
    }
    
    // Execute commands silently without user interaction
    // Note: This method is kept for backward compatibility with structured responses
    executeCommandsSilently(commands: string[]) {
        console.log('Executing commands silently:', commands);
        
        if (!window.app || !window.app.socket) {
            console.warn('Socket not available for sending commands');
            return;
        }
        
        // Execute each command with a small delay between them
        commands.forEach((command, index) => {
            setTimeout(() => {
                if (window.app && window.app.socket) {
                    console.log(`Silently executing command: ${command}`);
                    window.app.socket.sendMessage(command);
                }
            }, index * 200); // 200ms delay between commands
        });
    }
    
    addInputMessage(input: string) {
        const message: MessageItem = {
            type: 'input',
            content: input,
            timestamp: new Date()
        };
        this.messages.push(message);
        this.renderConversation();
    }
    
    addResponseMessage(response: string, contentType: 'text' | 'snippet' = 'text', snippetHeader?: string) {
        const message: MessageItem = {
            type: 'response',
            content: response,
            contentType: contentType,
            snippetHeader: snippetHeader,
            timestamp: new Date()
        };
        this.messages.push(message);
        this.renderConversation();
    }
    
    renderConversation() {
        // Clear current conversation
        $(this.conversationContainer).empty();
        
        if (this.messages.length === 0) {
            const emptyMessage = L.DomUtil.create('div', 'conversation-empty', this.conversationContainer);
            
            // Apply modern styling to empty message
            emptyMessage.style.cssText = `
                display: flex !important;
                flex-direction: column !important;
                align-items: center !important;
                justify-content: center !important;
                height: 100% !important;
                padding: 24px !important;
                text-align: center !important;
            `;
            
            // Create welcome message with title, subtitle and examples
            const welcomeTitle = L.DomUtil.create('div', 'welcome-title', emptyMessage);
            welcomeTitle.innerText = 'How can I help you today?';
            
            // Apply modern styling to welcome title
            welcomeTitle.style.cssText = `
                font-size: 18px !important;
                font-weight: 500 !important;
                color: #202124 !important;
                margin-bottom: 12px !important;
            `;
            
            const welcomeSubtitle = L.DomUtil.create('div', 'welcome-subtitle', emptyMessage);
            welcomeSubtitle.innerText = 'You can ask me to perform document operations or help with formatting.';
            
            // Apply modern styling to welcome subtitle
            welcomeSubtitle.style.cssText = `
                font-size: 14px !important;
                color: #5f6368 !important;
                margin-bottom: 24px !important;
                line-height: 1.5 !important;
            `;
            
            const welcomeExamples = L.DomUtil.create('div', 'welcome-examples', emptyMessage);
            
            // Apply modern styling to welcome examples
            welcomeExamples.style.cssText = `
                display: flex !important;
                flex-direction: column !important;
                gap: 8px !important;
                width: 100% !important;
                max-width: 280px !important;
            `;
            
            // Add example inputs that users can click on
            const examples = [
                'SelectAll',
                'Bold',
                'InsertPageBreak',
                'InsertTable',
                'Help'
            ];
            
            examples.forEach(example => {
                const exampleItem = L.DomUtil.create('div', 'welcome-example-item', welcomeExamples);
                exampleItem.innerText = example;
                
                // Apply modern styling to example item
                exampleItem.style.cssText = `
                    padding: 10px 16px !important;
                    background-color: #f1f3f4 !important;
                    border-radius: 8px !important;
                    cursor: pointer !important;
                    font-size: 14px !important;
                    color: #202124 !important;
                    transition: background-color 0.2s !important;
                `;
                
                // Add hover effect
                exampleItem.addEventListener('mouseover', () => {
                    exampleItem.style.backgroundColor = '#e8eaed';
                });
                exampleItem.addEventListener('mouseout', () => {
                    exampleItem.style.backgroundColor = '#f1f3f4';
                });
                
                exampleItem.addEventListener('click', () => {
                    this.chatInput.value = example;
                    this.chatInput.focus();
                });
            });
            
            return;
        }
        
        // Create message bubbles for each message
        this.messages.forEach((message, index) => {
            const messageElement = L.DomUtil.create('div', `message-bubble ${message.type}`, this.conversationContainer);
            
            // Apply modern styling to message bubble - different styling for input vs response
            if (message.type === 'input') {
                messageElement.style.cssText = `
                    margin-bottom: 16px !important;
                    max-width: 90% !important;
                    border-radius: 0 12px 12px 12px !important;
                    overflow: hidden !important;
                    box-shadow: 0 1px 2px rgba(0, 0, 0, 0.1) !important;
                    background-color: #e3f2fd !important; 
                    margin-left: auto !important;
                `;
                
                // Add header for input messages
                const headerElement = L.DomUtil.create('div', 'input-box-header', messageElement);
                
                // Apply modern styling to header
                headerElement.style.cssText = `
                    display: flex !important;
                    justify-content: flex-end !important;
                    padding: 8px 12px 4px !important;
                    background-color: rgba(0, 0, 0, 0.03) !important;
                `;
                
                const buttonsContainer = L.DomUtil.create('div', 'input-box-buttons', headerElement);
                
                // Apply styling to buttons container
                buttonsContainer.style.cssText = `
                    display: flex !important;
                    gap: 4px !important;
                `;
                
                // Add copy button
                const copyButton = L.DomUtil.create('button', 'copy-button', buttonsContainer);
                copyButton.setAttribute('aria-label', 'Copy input');
                copyButton.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M16 1H4C2.9 1 2 1.9 2 3V17H4V3H16V1ZM19 5H8C6.9 5 6 5.9 6 7V21C6 22.1 6.9 23 8 23H19C20.1 23 21 22.1 21 21V7C21 5.9 20.1 5 19 5ZM19 21H8V7H19V21Z" fill="currentColor"/></svg>';
                copyButton.addEventListener('click', () => this.copyToClipboard(message.content));
                
                // Apply modern styling to copy button
                copyButton.style.cssText = `
                    background-color: transparent !important;
                    border: none !important;
                    cursor: pointer !important;
                    padding: 4px !important;
                    border-radius: 4px !important;
                    color: #5f6368 !important;
                    display: flex !important;
                    align-items: center !important;
                    justify-content: center !important;
                `;
                
                // Add stop button to the last input message if waiting for response
                if (index === this.messages.length - 1 && 
                    (this.waitingForResponse || this.messages.length % 2 !== 0)) {
                    const stopButton = L.DomUtil.create('button', 'stop-button', buttonsContainer);
                    stopButton.setAttribute('aria-label', 'Stop response');
                    stopButton.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M6 6h12v12H6z" fill="currentColor"/></svg>';
                    stopButton.addEventListener('click', () => this.stopResponse());
                    
                    // Apply modern styling to stop button
                    stopButton.style.cssText = `
                        background-color: transparent !important;
                        border: none !important;
                        cursor: pointer !important;
                        padding: 4px !important;
                        border-radius: 4px !important;
                        color: #d93025 !important;
                        display: flex !important;
                        align-items: center !important;
                        justify-content: center !important;
                    `;
                }
                
                // Add content
                const contentElement = L.DomUtil.create('div', 'message-content', messageElement);
                contentElement.innerText = message.content;
                
                // Apply modern styling to content
                contentElement.style.cssText = `
                    padding: 8px 12px 12px !important;
                    font-size: 14px !important;
                    line-height: 1.5 !important;
                    color: #202124 !important;
                    word-break: break-word !important;
                    overflow-wrap: break-word !important;
                    white-space: pre-wrap !important;
                    max-width: 100% !important;
                    box-sizing: border-box !important;
                `;
                
                // Add footer with generating indicator for the last input if waiting for response
                if (index === this.messages.length - 1 && this.waitingForResponse) {
                    const footerElement = L.DomUtil.create('div', 'input-box-footer', messageElement);
                    
                    // Apply modern styling to footer
                    footerElement.style.cssText = `
                        padding: 8px 12px !important;
                        font-size: 12px !important;
                        color: #5f6368 !important;
                        background-color: rgba(0, 0, 0, 0.03) !important;
                        display: flex !important;
                        align-items: center !important;
                    `;
                    
                    footerElement.innerHTML = 'Generating<div class="generating-dots"><div class="generating-dot"></div><div class="generating-dot"></div><div class="generating-dot"></div></div>';
                    
                    // Add styling for generating dots
                    const style = document.createElement('style');
                    style.textContent = `
                        .generating-dots {
                            display: flex;
                            margin-left: 8px;
                        }
                        .generating-dot {
                            width: 6px;
                            height: 6px;
                            border-radius: 50%;
                            background-color: #5f6368;
                            margin: 0 2px;
                            animation: pulse 1.5s infinite ease-in-out;
                        }
                        .generating-dot:nth-child(2) {
                            animation-delay: 0.2s;
                        }
                        .generating-dot:nth-child(3) {
                            animation-delay: 0.4s;
                        }
                        @keyframes pulse {
                            0%, 100% {
                                opacity: 0.4;
                            }
                            50% {
                                opacity: 1;
                            }
                        }
                    `;
                    document.head.appendChild(style);
                }
            } else if (message.contentType === 'snippet') {
                // Create a snippet container with a different style
                const snippetElement = L.DomUtil.create('div', 'snippet-container', messageElement);
                
                // Add header with buttons and title if provided
                const headerElement = L.DomUtil.create('div', 'snippet-header', snippetElement);
                
                // Style the header with reduced padding
                headerElement.style.cssText = `
                    display: flex !important;
                    justify-content: space-between !important;
                    align-items: center !important;
                    padding: 8px 12px !important;
                    background-color: #f1f3f4 !important;
                    border-bottom: 1px solid #e1e4e8 !important;
                    font-size: 12px !important;
                    color: #5f6368 !important;
                `;
                
                // Add context button with '@' symbol at the top left
                const contextButton = L.DomUtil.create('button', 'context-button', headerElement);
                contextButton.setAttribute('aria-label', 'Context');
                contextButton.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2ZM12 20C7.59 20 4 16.41 4 12C4 7.59 7.59 4 12 4C16.41 4 20 7.59 20 12C20 16.41 16.41 20 12 20ZM11 7H13V13H11V7ZM11 15H13V17H11V15Z" fill="currentColor"/></svg>';
                
                // Apply modern styling to context button
                contextButton.style.cssText = `
                    background-color: transparent !important;
                    border: none !important;
                    cursor: pointer !important;
                    padding: 4px !important;
                    border-radius: 4px !important;
                    color: #5f6368 !important;
                    display: flex !important;
                    align-items: center !important;
                    justify-content: center !important;
                `;
                
                // Add snippet title if provided
                if (message.snippetHeader) {
                    const titleElement = L.DomUtil.create('div', 'snippet-title', headerElement);
                    titleElement.innerText = message.snippetHeader;
                    
                    // Style the title
                    titleElement.style.cssText = `
                        flex-grow: 1 !important;
                        text-align: center !important;
                        font-weight: 500 !important;
                        margin: 0 8px !important;
                        white-space: nowrap !important;
                        overflow: hidden !important;
                        text-overflow: ellipsis !important;
                    `;
                }
                
                // Add buttons container
                const buttonsContainer = L.DomUtil.create('div', 'snippet-buttons', headerElement);
                
                // Style the buttons container
                buttonsContainer.style.cssText = `
                    display: flex !important;
                    gap: 4px !important;
                `;
                
                // Add copy button
                const copyButton = L.DomUtil.create('button', 'copy-button', buttonsContainer);
                copyButton.setAttribute('aria-label', 'Copy snippet');
                copyButton.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M16 1H4C2.9 1 2 1.9 2 3V17H4V3H16V1ZM19 5H8C6.9 5 6 5.9 6 7V21C6 22.1 6.9 23 8 23H19C20.1 23 21 22.1 21 21V7C21 5.9 20.1 5 19 5ZM19 21H8V7H19V21Z" fill="currentColor"/></svg>';
                copyButton.addEventListener('click', () => this.copyToClipboard(message.content));
                
                // Apply modern styling to copy button
                copyButton.style.cssText = `
                    background-color: transparent !important;
                    border: none !important;
                    cursor: pointer !important;
                    padding: 4px !important;
                    border-radius: 4px !important;
                    color: #5f6368 !important;
                    display: flex !important;
                    align-items: center !important;
                    justify-content: center !important;
                `;
                
                // Add apply button
                const applyButton = L.DomUtil.create('button', 'apply-button', buttonsContainer);
                applyButton.setAttribute('aria-label', 'Apply snippet');
                applyButton.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M9 16.2L4.8 12l-1.4 1.4L9 19 21 7l-1.4-1.4L9 16.2z" fill="currentColor"/></svg>';
                applyButton.addEventListener('click', () => this.applySnippet(message.content));
                
                // Apply modern styling to apply button
                applyButton.style.cssText = `
                    background-color: transparent !important;
                    border: none !important;
                    cursor: pointer !important;
                    padding: 4px !important;
                    border-radius: 4px !important;
                    color: #1a73e8 !important;
                    display: flex !important;
                    align-items: center !important;
                    justify-content: center !important;
                `;
                
                // Add snippet content
                const snippetContent = L.DomUtil.create('pre', 'snippet-content', snippetElement);
                snippetContent.innerText = message.content;
                
                // Style the snippet content with improved overflow handling
                snippetContent.style.cssText = `
                    margin: 0 !important;
                    padding: 12px !important;
                    overflow-x: auto !important;
                    font-family: 'SFMono-Regular', Consolas, 'Liberation Mono', Menlo, monospace !important;
                    font-size: 13px !important;
                    line-height: 1.5 !important;
                    color: #24292e !important;
                    background-color: #f6f8fa !important;
                    border-radius: 0 0 8px 8px !important;
                    white-space: pre-wrap !important;
                    word-break: break-word !important;
                    overflow-wrap: break-word !important;
                    max-width: 100% !important;
                    box-sizing: border-box !important;
                `;
            } else {
                // For regular response messages
                messageElement.style.cssText = `
                    margin-bottom: 16px !important;
                    max-width: 90% !important;
                    border-radius: 12px 0 12px 12px !important;
                    overflow: hidden !important;
                    box-shadow: 0 1px 2px rgba(0, 0, 0, 0.1) !important;
                    background-color: #f5f5f5 !important; 
                    margin-right: auto !important;
                `;
                
                // Add content
                const contentElement = L.DomUtil.create('div', 'message-content', messageElement);
                contentElement.innerText = message.content;
                
                // Apply modern styling to content
                contentElement.style.cssText = `
                    padding: 8px 12px 12px !important;
                    font-size: 14px !important;
                    line-height: 1.5 !important;
                    color: #202124 !important;
                    word-break: break-word !important;
                    overflow-wrap: break-word !important;
                    white-space: pre-wrap !important;
                    max-width: 100% !important;
                    box-sizing: border-box !important;
                `;
            }
        });
        
        // Scroll to bottom
        this.conversationContainer.scrollTop = this.conversationContainer.scrollHeight;
    }
    
    copyToClipboard(text: string) {
        navigator.clipboard.writeText(text).then(() => {
            console.log('Text copied to clipboard');
        }).catch(err => {
            console.error('Could not copy text: ', err);
        });
    }

    onRemove() {
        this.map.off('chatsidebar');
        this.map.off('jsdialogupdate', this.onJSUpdate, this);
        this.map.off('jsdialogaction', this.onJSAction, this);
        this.map.off('modelresponse', this.onModelResponse, this);
    }

    isVisible(): boolean {
        return true; // Always visible now
    }

    closeChatSidebar() {
        // This method is no longer needed but kept for compatibility
        this.collapse();
    }
    
    executeInput() {
        const input = this.chatInput.value.trim();
        if (!input || this.waitingForResponse) return;
        
        // Add to input history
        if (!this.chatHistory.includes(input)) {
            this.chatHistory.push(input);
            // Limit history size
            if (this.chatHistory.length > 50) {
                this.chatHistory.shift();
            }
        }
        this.chatHistoryIndex = this.chatHistory.length;
        
        // Add to conversation
        this.addInputMessage(input);
        
        // Clear input and reset its size
        this.chatInput.value = '';
        this.chatInput.style.height = '40px'; // Reset to minimum height
        document.documentElement.style.setProperty('--input-area-height', '92px'); // Reset input area height with reduced footer
        
        // Set waiting state
        this.waitingForResponse = true;
        this.chatInput.disabled = true;
        this.chatSendButton.disabled = true;
        this.renderConversation(); // Re-render to show stop button and "Generating..." indicator
        
        // Use the Model's sendInput method
        if (this.map && this.map.model) {
            const inputId = Date.now();
            this.currentInputId = inputId;
            
            this.map.model.sendInput(input, (response: any) => {
                // Only process if this is still the current input
                if (this.currentInputId === inputId) {
                    // Handle response
                    this.addResponseMessage(this.map.model.formatResponse(response));
                    
                    // Reset waiting state
                    this.waitingForResponse = false;
                    this.currentInputId = null;
                    this.chatInput.disabled = false;
                    this.chatSendButton.disabled = false;
                    this.chatInput.focus();
                }
            });
        } else {
            console.warn('Model not available for input processing');
            this.addResponseMessage('Error: Unable to process input - model not available');
            
            // Reset waiting state
            this.waitingForResponse = false;
            this.chatInput.disabled = false;
            this.chatSendButton.disabled = false;
        }
    }
    
    stopResponse() {
        if (this.waitingForResponse && this.currentInputId) {
            // Add a message indicating the response was stopped
            this.addResponseMessage('Response stopped by user.');
            
            // Reset waiting state
            this.waitingForResponse = false;
            this.currentInputId = null;
            this.chatInput.disabled = false;
            this.chatSendButton.disabled = false;
            this.chatInput.focus();
        
            // Re-render conversation to remove stop button and "Generating..." indicator
            this.renderConversation();
        }
    }
    
    navigateHistory(direction: number) {
        if (this.chatHistory.length === 0) return;
        
        this.chatHistoryIndex += direction;
        
        // Clamp history index
        if (this.chatHistoryIndex < 0) this.chatHistoryIndex = 0;
        if (this.chatHistoryIndex > this.chatHistory.length) this.chatHistoryIndex = this.chatHistory.length;
        
        // Set input value
        if (this.chatHistoryIndex === this.chatHistory.length) {
            this.chatInput.value = '';
        } else {
            this.chatInput.value = this.chatHistory[this.chatHistoryIndex];
        }
        
        // Move cursor to end
        setTimeout(() => {
            this.chatInput.selectionStart = this.chatInput.value.length;
            this.chatInput.selectionEnd = this.chatInput.value.length;
        }, 0);
    }

    onJSUpdate(e: FireEvent) {
        // Pass to model for handling
        if (this.map && this.map.model) {
            this.map.model.handleMessage({
                type: 'update',
                source: 'sidebar',
                data: e.data
            });
        }
    }

    onJSAction(e: FireEvent) {
        if (!e.data) return;
        
        const data = e.data;
        
        // Handle other actions
        if (data.action === 'chat-sidebar') {
            this.onChatSidebar(data);
        }
    }

    onResize() {
        // Handle resize events
    }
    
    onChatSidebar(data: FireEvent) {
        // Handle chat sidebar events
        if (data && data.data) {
            if (data.data.response) {
                this.addResponseMessage(this.map.model.formatResponse(data.data.response));
            } else if (this.map && this.map.model) {
                // Pass to model for handling
                this.map.model.handleMessage({
                    type: 'chatsidebar',
                    source: 'sidebar',
                    data: data.data
                });
            }
        }
    }

    setupAutoGrowingInput() {
        // Initial adjustment
        this.adjustInputHeight();
        
        // Add input event listener for auto-growing
        this.chatInput.addEventListener('input', () => {
            this.adjustInputHeight();
        });
        
        // Also adjust on window resize
        window.addEventListener('resize', () => {
            this.adjustInputHeight();
        });
    }
    
    adjustInputHeight() {
        // Reset height to auto to get the correct scrollHeight
        this.chatInput.style.height = 'auto';
        
        // Calculate new height (clamped between min and max)
        const newHeight = Math.min(
            Math.max(this.chatInput.scrollHeight, 40), // Min height is 40px
            150 // Max height is 150px
        );
        
        // Set the new height
        this.chatInput.style.height = newHeight + 'px';
        
        // Update the input area height - base height includes footer (28px) + padding
        const inputAreaHeight = Math.min(
            92 + (newHeight - 40), // Base height (92px) + additional height
            202 // Max height is 202px
        );
        
        // Update CSS variable for dynamic conversation wrapper height
        document.documentElement.style.setProperty('--input-area-height', inputAreaHeight + 'px');
    }

    onModelResponse(e: FireEvent) {
        if (!e.data) return;
        
        console.log('Sidebar received model response:', e.data);
        
        // Check if this is a model response
        if (typeof e.data === 'string' && e.data.startsWith('modelresponse')) {
            // Add the response to the conversation
            this.addResponseMessage(this.map.model.formatResponse(e.data));
        } else if (typeof e.data === 'object') {
            // Handle structured response
            if (e.data.conversation !== undefined || e.data.snippet !== undefined || e.data.commands !== undefined) {
                // Process the structured response
                this.processStructuredResponse(e.data);
                
                // Add the conversation part to the chat
                if (e.data.conversation) {
                    this.addResponseMessage(e.data.conversation);
                }
            }
        }
    }
    
    // Helper method to check if a response contains snippets
    containsSnippet(response: string): boolean {
        // Check for snippet markers [snippet:...]
        return response.includes('[snippet:') && response.includes(']');
    }
    
    // Helper method to split a response into text and snippets
    splitResponseIntoParts(response: string): {type: 'text' | 'snippet', content: string, header?: string}[] {
        const parts: {type: 'text' | 'snippet', content: string, header?: string}[] = [];
        const snippetRegex = /\[snippet:(.*?)\]([\s\S]*?)\[\/snippet\]/g;
        
        let lastIndex = 0;
        let match;
        
        while ((match = snippetRegex.exec(response)) !== null) {
            // Add text before snippet if any
            if (match.index > lastIndex) {
                const textContent = response.substring(lastIndex, match.index).trim();
                if (textContent) {
                    parts.push({type: 'text', content: textContent});
                }
            }
            
            // Add snippet with header
            const header = match[1].trim();
            const snippetContent = match[2].trim();
            if (snippetContent) {
                parts.push({type: 'snippet', content: snippetContent, header: header});
            }
            
            lastIndex = match.index + match[0].length;
        }
        
        // Add remaining text after the last snippet if any
        if (lastIndex < response.length) {
            const textContent = response.substring(lastIndex).trim();
            if (textContent) {
                parts.push({type: 'text', content: textContent});
            }
        }
        
        return parts;
    }

    applySnippet(snippet: string) {
        console.log('Applying snippet:', snippet);
        
        // Execute the snippet as an input
        if (this.map && this.map.model) {
            // You might want to show a confirmation or feedback message
            this.addResponseMessage("Applying: " + snippet, 'text');
            
            // Send the input
            this.map.model.sendInput(snippet, (response: any) => {
                // Handle response if needed
                if (response && response.success === false) {
                    this.addResponseMessage("Failed to apply snippet: " + (response.error || "Unknown error"), 'text');
                }
            });
        }
    }
}

JSDialog.ChatSidebar = function (map: any, options: ChatSidebarOptions) {
    return new ChatSidebar(map, options);
};