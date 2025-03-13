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
	height?: string; // Optional height property
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
			animSpeed: 300 /* Default speed: to be used on load */,
			collapsed: false,
			height: '100%', // Default height
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

		// Set height from options
		if (this.options.height) {
			this.container.style.height = this.options.height;
		}

		// Create sidebar header
		this.headerElement = L.DomUtil.create(
			'div',
			'ChatSidebar-header',
			this.container,
		);

		const titleElement = L.DomUtil.create(
			'h2',
			'ChatSidebar-title',
			this.headerElement,
		);
		titleElement.innerText = 'Chat';

		// Create header buttons container
		const headerButtons = L.DomUtil.create(
			'div',
			'ChatSidebar-header-buttons',
			this.headerElement,
		);

		// Create collapse button with improved styling
		this.collapseButton = L.DomUtil.create(
			'button',
			'ChatSidebar-collapse-btn',
			headerButtons,
		) as HTMLButtonElement;
		this.collapseButton.innerHTML =
			'<span class="collapse-icon">&#10094;</span>'; // Left-pointing angle bracket
		this.collapseButton.setAttribute('aria-label', 'Collapse sidebar');
		this.collapseButton.addEventListener('click', () => this.toggleCollapse());

		// Create conversation wrapper to control size
		const conversationWrapper = L.DomUtil.create(
			'div',
			'ModelSidebar-conversation-wrapper',
			this.container,
		);

		// Create conversation container
		this.conversationContainer = L.DomUtil.create(
			'div',
			'ModelSidebar-conversation-container',
			conversationWrapper,
		);

		// Create input area
		this.inputAreaElement = L.DomUtil.create(
			'div',
			'sidebar-input-area',
			this.container,
		);

		// Create input container
		const inputContainer = L.DomUtil.create(
			'div',
			'ModelSidebar-input-container',
			this.inputAreaElement,
		);

		// Create input field
		this.chatInput = L.DomUtil.create(
			'textarea',
			'ModelSidebar-input-field',
			inputContainer,
		) as HTMLTextAreaElement;
		this.chatInput.placeholder = 'Type a message...';
		this.chatInput.setAttribute('aria-label', 'Message input');
		this.chatInput.setAttribute('rows', '1');

		// Create footer for buttons
		const inputFooter = L.DomUtil.create('div', 'ModelSidebar-input-footer', inputContainer);

		// Create left buttons container
		const leftButtons = L.DomUtil.create(
			'div',
			'ModelSidebar-input-footer-left',
			inputFooter,
		);

		// Move browser/help button to the leftmost position
		const browserButton = L.DomUtil.create(
			'button',
			'ModelSidebar-action-btn',
			leftButtons,
		) as HTMLButtonElement;
		browserButton.innerHTML =
			'<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2ZM12 20C7.59 20 4 16.41 4 12C4 7.59 7.59 4 12 4C16.41 4 20 7.59 20 12C20 16.41 16.41 20 12 20ZM11 15H13V17H11V15ZM11 7H13V13H11V7Z" fill="currentColor"/></svg>';
		browserButton.setAttribute('aria-label', 'Help');
		browserButton.setAttribute('title', 'Help');

		// Create image button
		const imageButton = L.DomUtil.create(
			'button',
			'ModelSidebar-action-btn',
			leftButtons,
		) as HTMLButtonElement;
		imageButton.innerHTML =
			'<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M19 3H5C3.9 3 3 3.9 3 5V19C3 20.1 3.9 21 5 21H19C20.1 21 21 20.1 21 19V5C21 3.9 20.1 3 19 3ZM19 19H5V5H19V19ZM13.96 12.29L11.21 15.83L9.25 13.47L6.5 17H17.5L13.96 12.29Z" fill="currentColor"/></svg>';
		imageButton.setAttribute('aria-label', 'Add image');
		imageButton.setAttribute('title', 'Add image');

		// Add web browser button
		const webBrowserButton = L.DomUtil.create(
			'button',
			'ModelSidebar-action-btn',
			leftButtons,
		) as HTMLButtonElement;
		webBrowserButton.innerHTML =
			'<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2ZM11 19.93C7.05 19.44 4 16.08 4 12C4 11.38 4.08 10.79 4.21 10.21L9 15V16C9 17.1 9.9 18 11 18V19.93ZM17.9 17.39C17.64 16.58 16.9 16 16 16H15V13C15 12.45 14.55 12 14 12H8V10H10C10.55 10 11 9.55 11 9V7H13C14.1 7 15 6.1 15 5V4.59C17.93 5.78 20 8.65 20 12C20 14.08 19.2 15.97 17.9 17.39Z" fill="currentColor"/></svg>';
		webBrowserButton.setAttribute('aria-label', 'Browse web');
		webBrowserButton.setAttribute('title', 'Browse web');

		// Create send button in the footer with icon
		this.chatSendButton = L.DomUtil.create(
			'button',
			'ModelSidebar-send-btn',
			inputFooter,
		) as HTMLButtonElement;
		this.chatSendButton.innerHTML =
			'<svg width="14" height="14" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M2.01 21L23 12 2.01 3 2 10l15 2-15 2z" fill="currentColor"/></svg>';
		this.chatSendButton.setAttribute('aria-label', 'Send message (Enter)');
		this.chatSendButton.addEventListener('click', () => this.executeInput());

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

		this.collapseButton.innerHTML =
			'<span class="collapse-icon">&#10095;</span>'; // Right-pointing angle bracket
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

		this.collapseButton.innerHTML =
			'<span class="collapse-icon">&#10094;</span>'; // Left-pointing angle bracket
		this.collapseButton.setAttribute('aria-label', 'Collapse sidebar');

		this.map._onResize();
	}

	isInputResponse(response: any): boolean {
		// Check if this is an input response
		return (
			response.command === 'commandresult' ||
			(response.command === 'reply' && response.success !== undefined)
		);
	}

	// Process the structured response with conversation, snippet, and commands
	processStructuredResponse(response: any) {
		console.log('Processing structured response:', response);

		// Extract the result if it's nested
		let result = response;
		if (response.result && typeof response.result === 'object') {
			result = response.result;
		}

		// We no longer receive commands from the backend
		// The commands are now executed directly on the server side

		// Process snippet if present
		if (result.snippet) {
			// Extract snippet header if present
			const snippetHeader = this._extractSnippetHeader(result.snippet);

			// Add the snippet to the conversation
			this.addResponseMessage(result.snippet, 'snippet', snippetHeader);
		}
	}

	// Helper method to extract snippet header if it exists
	_extractSnippetHeader(snippet: string): string | undefined {
		if (typeof snippet === 'string') {
			// Check if the snippet has a header in the format [snippet:Header]
			const snippetMatch = snippet.match(/\[snippet:(.*?)\]/);
			if (snippetMatch && snippetMatch[1]) {
				return snippetMatch[1].trim();
			}
		}
		return undefined;
	}

	addInputMessage(input: string) {
		const message: MessageItem = {
			type: 'input',
			content: input,
			timestamp: new Date(),
		};
		this.messages.push(message);
		this.renderConversation();
	}

	addResponseMessage(
		response: string,
		contentType: 'text' | 'snippet' = 'text',
		snippetHeader?: string,
	) {
		const message: MessageItem = {
			type: 'response',
			content: response,
			contentType: contentType,
			snippetHeader: snippetHeader,
			timestamp: new Date(),
		};
		this.messages.push(message);
		this.renderConversation();
	}

	renderConversation() {
		// Clear current conversation
		$(this.conversationContainer).empty();

		if (this.messages.length === 0) {
			const emptyMessage = L.DomUtil.create(
				'div',
				'ModelSidebar-conversation-empty',
				this.conversationContainer,
			);

			// Create welcome message with title, subtitle and examples
			const welcomeTitle = L.DomUtil.create(
				'div',
				'ModelSidebar-welcome-title',
				emptyMessage,
			);
			welcomeTitle.innerText = 'How can I help you today?';

			const welcomeSubtitle = L.DomUtil.create(
				'div',
				'ModelSidebar-welcome-subtitle',
				emptyMessage,
			);
			welcomeSubtitle.innerText =
				'You can ask me to perform document operations or help with formatting.';

			const welcomeExamples = L.DomUtil.create(
				'div',
				'ModelSidebar-welcome-examples',
				emptyMessage,
			);

			// Add example inputs that users can click on
			const examples = [
				'SelectAll',
				'Bold',
				'InsertPageBreak',
				'InsertTable',
				'Help',
			];

			examples.forEach((example) => {
				const exampleItem = L.DomUtil.create(
					'div',
					'ModelSidebar-example-item',
					welcomeExamples,
				);
				exampleItem.innerText = example;

				exampleItem.addEventListener('click', () => {
					this.chatInput.value = example;
					this.chatInput.focus();
				});
			});

			// Force the conversation container to be visible
			this.conversationContainer.style.position = 'relative';
			this.conversationContainer.style.height = 'auto';
			this.conversationContainer.style.minHeight = '300px';

			return;
		}

		// Reset to default positioning if there are messages
		this.conversationContainer.style.position = 'absolute';
		this.conversationContainer.style.height = '100%';
		this.conversationContainer.style.minHeight = 'auto';

		// Create message bubbles for each message
		this.messages.forEach((message, index) => {
			const messageElement = L.DomUtil.create(
				'div',
				`ModelSidebar-message-bubble ${message.type}`,
				this.conversationContainer,
			);

			// Handle different message types with different styling
			if (message.type === 'response' && message.contentType !== 'snippet') {
				// For text responses, display directly without a box/container
				// Format the content as plain text, not JSON
				let displayContent = message.content;

				// For response messages, try to clean up the content
				if (message.type === 'response') {
					try {
						// Check if the content is JSON and convert it to plain text
						if (typeof displayContent === 'string') {
							// First check if it's a JSON string
							if (displayContent.startsWith('{') || displayContent.startsWith('[')) {
								const jsonObj = JSON.parse(displayContent);
								if (jsonObj.conversation) {
									displayContent = jsonObj.conversation;
								} else if (jsonObj.result && jsonObj.result.conversation) {
									displayContent = jsonObj.result.conversation;
								}
							}

							// Check for markdown code blocks and remove them
							const codeBlockRegex = /```(?:\w+)?\s*([\s\S]*?)```/g;
							displayContent = displayContent.replace(codeBlockRegex, '');

							// Remove snippet headers if present
							const snippetHeaderRegex = /\[snippet:.*?\]/g;
							displayContent = displayContent.replace(snippetHeaderRegex, '');

							// Trim any excess whitespace
							displayContent = displayContent.trim();
						}
					} catch (e) {
						// If parsing fails, use the original content
						console.log('Error processing content:', e);
					}
				}

				// Create container for direct content to match width of user messages
				const responseContainer = L.DomUtil.create(
					'div',
					'ModelSidebar-response-container',
					messageElement
				);

				// Create direct content element inside the container
				const contentElement = L.DomUtil.create(
					'div',
					'ModelSidebar-message-content-direct',
					responseContainer
				);

				contentElement.innerText = displayContent;
			} else {
				// For user inputs and snippets, use containers but with smaller headers
				const contentContainer = L.DomUtil.create(
					'div',
					`ModelSidebar-content-container ${message.type}`,
					messageElement,
				);

				// Add header with buttons
				const headerElement = L.DomUtil.create(
					'div',
					`ModelSidebar-message-header ${message.type}`,
					contentContainer,
				);

				// Create title for header
				const titleElement = L.DomUtil.create(
					'div',
					'ModelSidebar-message-title',
					headerElement,
				);

				if (message.type === 'input') {
					titleElement.innerText = 'Input';
				} else if (message.contentType === 'snippet') {
					titleElement.innerText = message.snippetHeader || 'Text Snippet';
				} else {
					titleElement.innerText = 'Response';
				}

				// Add buttons container
				const buttonsContainer = L.DomUtil.create(
					'div',
					'ModelSidebar-message-buttons',
					headerElement,
				);

				// Add copy button for all message types
				const copyButton = L.DomUtil.create(
					'button',
					'ModelSidebar-copy-button',
					buttonsContainer,
				);
				copyButton.setAttribute('aria-label', 'Copy message');
				copyButton.innerHTML = 'Copy';
				copyButton.addEventListener('click', () =>
					this.copyToClipboard(message.content),
				);

				// Add stop button to the last input message if waiting for response
				if (
					message.type === 'input' &&
					index === this.messages.length - 1 &&
					this.waitingForResponse
				) {
					const stopButton = L.DomUtil.create(
						'button',
						'ModelSidebar-stop-button',
						buttonsContainer,
					);
					stopButton.setAttribute('aria-label', 'Stop response');
					stopButton.innerHTML = 'Stop';
					stopButton.addEventListener('click', () => this.stopResponse());
				}

				// Add apply button for snippets
				if (message.contentType === 'snippet') {
					const applyButton = L.DomUtil.create(
						'button',
						'ModelSidebar-apply-button',
						buttonsContainer,
					);
					applyButton.setAttribute('aria-label', 'Apply snippet');
					applyButton.innerHTML = 'Apply';
					applyButton.addEventListener('click', () =>
						this.applySnippet(message.content),
					);
				}

				// Add content based on message type
				if (message.contentType === 'snippet') {
					// Add snippet content
					const snippetContent = L.DomUtil.create(
						'pre',
						'ModelSidebar-snippet-content',
						contentContainer,
					);
					snippetContent.innerText = message.content;
				} else {
					// Add regular text content
					const contentElement = L.DomUtil.create(
						'div',
						'ModelSidebar-message-content',
						contentContainer,
					);
					contentElement.innerText = message.content;
				}
			}

			// Add footer with generating indicator for the last input if waiting for response
			if (message.type === 'input' && index === this.messages.length - 1 && this.waitingForResponse) {
				const footerElement = L.DomUtil.create(
					'div',
					'ModelSidebar-input-box-footer',
					messageElement,
				);

				footerElement.innerHTML =
					'Generating<div class="ModelSidebar-generating-dots"><div class="ModelSidebar-generating-dot"></div><div class="ModelSidebar-generating-dot"></div><div class="ModelSidebar-generating-dot"></div></div>';
			}
		});

		// Scroll to bottom
		this.conversationContainer.scrollTop = this.conversationContainer.scrollHeight;
	}

	copyToClipboard(text: string) {
		navigator.clipboard
			.writeText(text)
			.then(() => {
				console.log('Text copied to clipboard');
			})
			.catch((err) => {
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
		if (!input) return;

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

		// Reset input area height with fixed value
		const inputAreaHeight = 92; // Base height with minimum input
		document.documentElement.style.setProperty('--input-area-height', inputAreaHeight + 'px');

		// Force the conversation container to adjust its height
		const conversationWrapper = document.querySelector('.ModelSidebar-conversation-wrapper');
		if (conversationWrapper) {
			// Use a percentage-based calculation to ensure it doesn't overflow
			(conversationWrapper as HTMLElement).style.maxHeight = `calc(95% - ${inputAreaHeight}px)`;

			// Ensure the parent container doesn't grow beyond its bounds
			const container = document.querySelector('.ChatSidebar-container');
			if (container) {
				(container as HTMLElement).style.maxHeight = '95%';
			}
		}

		// Set waiting state - but keep input enabled
		this.waitingForResponse = true;
		this.chatInput.disabled = false; // Keep input enabled
		this.chatSendButton.disabled = false; // Keep send button enabled
		this.renderConversation(); // Re-render to show "Generating..." indicator

		// Use the Model's sendInput method
		if (this.map && this.map.model) {
			const inputId = Date.now();
			this.currentInputId = inputId;

			this.map.model.sendInput(input, (response: any) => {
				// Only process if this is still the current input
				if (this.currentInputId === inputId) {
					// Handle response if it's not already handled by onModelResponse
					// This is a fallback in case the response doesn't trigger onModelResponse
					if (response && typeof response === 'object') {
						const formattedResponse = this.map.model.formatResponse(response);
						if (formattedResponse && formattedResponse.trim()) {
							this.addResponseMessage(formattedResponse);
						}
					}

					// Reset waiting state
					this.waitingForResponse = false;
					this.currentInputId = null;
				}
			});
		} else {
			console.warn('Model not available for input processing');
			this.addResponseMessage(
				'Error: Unable to process input - model not available',
			);

			// Reset waiting state
			this.waitingForResponse = false;
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
		if (this.chatHistoryIndex > this.chatHistory.length)
			this.chatHistoryIndex = this.chatHistory.length;

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
				data: e.data,
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
				this.addResponseMessage(
					this.map.model.formatResponse(data.data.response),
				);
			} else if (this.map && this.map.model) {
				// Pass to model for handling
				this.map.model.handleMessage({
					type: 'chatsidebar',
					source: 'sidebar',
					data: data.data,
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
			100, // Max height is 100px (reduced from 120px)
		);

		// Set the new height
		this.chatInput.style.height = newHeight + 'px';

		// Update the input area height - base height includes footer (28px) + padding
		const inputAreaHeight = Math.min(
			92 + (newHeight - 40), // Base height (92px) + additional height
			152, // Max height is 152px (reduced from 172px)
		);

		// Update CSS variable for dynamic conversation wrapper height
		document.documentElement.style.setProperty(
			'--input-area-height',
			inputAreaHeight + 'px',
		);

		// Force the conversation container to adjust its height
		const conversationWrapper = document.querySelector('.ModelSidebar-conversation-wrapper');
		if (conversationWrapper) {
			// Use a percentage-based calculation to ensure it doesn't overflow
			(conversationWrapper as HTMLElement).style.maxHeight = `calc(95% - ${inputAreaHeight}px)`;

			// Ensure the parent container doesn't grow beyond its bounds
			const container = document.querySelector('.ChatSidebar-container');
			if (container) {
				(container as HTMLElement).style.maxHeight = '95%';
			}
		}
	}

	onModelResponse(e: FireEvent) {
		console.log('Sidebar received model response:', e.data);

		// Reset waiting state and re-enable input
		this.waitingForResponse = false;
		this.chatInput.disabled = false;
		this.chatSendButton.disabled = false;

		if (!e.data) return;

		// Check if this is a model response
		if (typeof e.data === 'string' && e.data.startsWith('modelresponse')) {
			const parts = e.data.split(' ');
			const status = parts[1]; // 'success' or 'error'

			if (status === 'error') {
				console.error('Model response error:', e.data);
				this.addResponseMessage('Error: ' + parts.slice(2).join(' '));
				return;
			}

			// Extract the JSON part (everything after "modelresponse success ")
			const jsonStr = e.data.substring('modelresponse success '.length);

			try {
				// Parse the JSON response
				const response = JSON.parse(jsonStr);
				console.log('Parsed model response:', response);

				// Handle conversation if present
				if (response.conversation) {
					// Clean up conversation text
					const cleanConversation = this.cleanMarkdownText(response.conversation);

					if (cleanConversation) {
						this.addResponseMessage(cleanConversation);
					}
				}

				// Handle snippet if present
				if (response.snippet) {
					// Clean up snippet text (but preserve code formatting)
					const cleanSnippet = this.cleanSnippetText(response.snippet);

					// Add snippet message
					this.addResponseMessage(cleanSnippet, 'snippet');
				}

				// If neither snippet nor conversation is present, add a generic response
				if (!response.snippet && !response.conversation) {
					this.addResponseMessage('Received empty response from model.');
				}
			} catch (error) {
				console.error('Error parsing model response:', error);
				// If parsing fails, add the raw text as a response
				this.addResponseMessage('Error parsing response: ' + jsonStr);
			}
		}
	}

	// Helper method to clean markdown formatting from text
	cleanMarkdownText(text: string): string {
		if (!text) return '';

		let cleanText = text;

		// Remove markdown code blocks
		cleanText = cleanText.replace(/```(?:json|js|javascript|typescript|ts|html|css|bash|sh|python|py|java|c|cpp|csharp|cs|go|ruby|php|swift|kotlin|rust|sql|xml|yaml|yml|toml|ini|markdown|md|text|txt|plaintext|plain)?\s*[\s\S]*?```/g, '');

		// Remove snippet headers
		cleanText = cleanText.replace(/\[snippet:.*?\]/g, '');

		// Remove inline code
		cleanText = cleanText.replace(/`([^`]+)`/g, '$1');

		// Remove excessive newlines (more than 2 in a row)
		cleanText = cleanText.replace(/\n{3,}/g, '\n\n');

		// Trim excess whitespace
		cleanText = cleanText.trim();

		return cleanText;
	}

	// Helper method to clean snippet text but preserve code formatting
	cleanSnippetText(text: string): string {
		if (!text) return '';

		let cleanText = text;

		// Extract code from markdown code blocks if present
		const codeBlockRegex = /```(?:json|js|javascript|typescript|ts|html|css|bash|sh|python|py|java|c|cpp|csharp|cs|go|ruby|php|swift|kotlin|rust|sql|xml|yaml|yml|toml|ini|markdown|md|text|txt|plaintext|plain)?\s*([\s\S]*?)```/;
		const match = cleanText.match(codeBlockRegex);

		if (match && match[1]) {
			// Use the code inside the code block
			cleanText = match[1].trim();
		}

		// Remove snippet headers
		cleanText = cleanText.replace(/\[snippet:.*?\]/g, '');

		// Trim excess whitespace
		cleanText = cleanText.trim();

		return cleanText;
	}

	// Helper method to check if a response contains snippets
	containsSnippet(response: string): boolean {
		// Check for snippet markers [snippet:...]
		return response.includes('[snippet:') && response.includes(']');
	}

	// Helper method to split a response into text and snippets
	splitResponseIntoParts(
		response: string,
	): { type: 'text' | 'snippet'; content: string; header?: string }[] {
		const parts: {
			type: 'text' | 'snippet';
			content: string;
			header?: string;
		}[] = [];
		const snippetRegex = /\[snippet:(.*?)\]([\s\S]*?)\[\/snippet\]/g;

		let lastIndex = 0;
		let match;

		while ((match = snippetRegex.exec(response)) !== null) {
			// Add text before snippet if any
			if (match.index > lastIndex) {
				const textContent = response.substring(lastIndex, match.index).trim();
				if (textContent) {
					parts.push({ type: 'text', content: textContent });
				}
			}

			// Add snippet with header
			const header = match[1].trim();
			const snippetContent = match[2].trim();
			if (snippetContent) {
				parts.push({
					type: 'snippet',
					content: snippetContent,
					header: header,
				});
			}

			lastIndex = match.index + match[0].length;
		}

		// Add remaining text after the last snippet if any
		if (lastIndex < response.length) {
			const textContent = response.substring(lastIndex).trim();
			if (textContent) {
				parts.push({ type: 'text', content: textContent });
			}
		}

		return parts;
	}

	applySnippet(snippet: string) {
		console.log('Applying snippet:', snippet);

		// Clean up the snippet before applying
		let cleanSnippet = snippet;

		// Remove any markdown code block markers
		cleanSnippet = cleanSnippet.replace(/```(?:\w+)?\s*([\s\S]*?)```/g, '$1');

		// Remove any snippet headers
		cleanSnippet = cleanSnippet.replace(/\[snippet:.*?\]/g, '');

		// Trim whitespace
		cleanSnippet = cleanSnippet.trim();

		console.log('Cleaned snippet for application:', cleanSnippet);

		// Execute the snippet as an input
		if (this.map && this.map.model) {
			// Send the input without showing a confirmation message
			// to avoid cluttering the conversation
			this.map.model.sendInput(cleanSnippet, (response: any) => {
				// Only show a message if there's an error
				if (response && response.success === false) {
					this.addResponseMessage(
						'Failed to apply snippet: ' + (response.error || 'Unknown error'),
						'text',
					);
				}
			});
		}
	}
}

JSDialog.ChatSidebar = function (map: any, options: ChatSidebarOptions) {
	return new ChatSidebar(map, options);
};
