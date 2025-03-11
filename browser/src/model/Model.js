/* Model.js - Make sure this file is loaded in the build before main.js */
L.Model = L.Class.extend({
	options: {
		position: 'topleft'
	},

	initialize: function (options) {
		L.setOptions(this, options);
		this._messageHandlers = {};
		this._responseCallbacks = {};
		this._commandCounter = 0;
		
		// Register for model response events
		if (window.app && window.app.map) {
			window.app.map.on('modelresponse', this._onModelResponse, this);
		}
	},

	getPosition: function () {
		return this.options.position;
	},

	setPosition: function (position) {
		var map = this._map;

		if (map) {
			map.removeControl(this);
		}

		this.options.position = position;

		if (map) {
			map.addControl(this);
		}

		return this;
	},

	getContainer: function () {
		return this._container;
	},

	addTo: function (map) {
		this.remove();
		this._map = map;

		var container = this._container = this.onAdd(map),
		    pos = this.getPosition(),
		    corner = map._controlCorners[pos];

		L.DomUtil.addClass(container, 'leaflet-control');

		if (pos.indexOf('bottom') !== -1) {
			corner.insertBefore(container, corner.firstChild);
		} else {
			corner.appendChild(container);
		}

		if (this.onAdded) {
			this.onAdded(this._map);
		}

		return this;
	},

	remove: function () {
		if (!this._map) {
			return this;
		}

		L.DomUtil.remove(this._container);

		if (this.onRemove) {
			this.onRemove(this._map);
		}

		this._map = null;

		return this;
	},

	isVisible: function () {
		if (!this._map) {
			return false;
		}
		var corner = this._map._controlCorners[this.options.position];
		return corner.hasChildNodes();
	},

	// ===== EXTERNAL COMMUNICATION API =====

	/**
	 * Execute a command and register a callback for the response
	 * @param {string} command - The command to execute
	 * @param {Function} callback - Callback function to handle the response
	 * @returns {boolean} - Success status
	 */
	executeCommand: function(command, callback) {
		console.log('Model.executeCommand called with:', command);
		if (!command || command.trim() === '') {
			return false;
		}

		if (!window.app || !window.app.socket) {
			console.warn('No socket connection available');
			return false;
		}

		// Generate a unique command ID for tracking responses
		const commandId = 'cmd_' + (++this._commandCounter);
		
		// Format command based on type
		let formattedCommand = command;
		
		// Handle different command types
		if (command.startsWith('model ')) {
			// Model commands - pass through as is
			formattedCommand = command;
		} else if (!command.startsWith('model ') && !command.startsWith('.model:')) {
			// Default to UNO commands if no prefix is specified
			formattedCommand = 'model ' + command;
		}
		
		// Send command to backend
		try {

			window.app.socket.sendMessage(formattedCommand);
			
			
			// Register callback if provided
			if (callback && typeof callback === 'function') {
				this._responseCallbacks[commandId] = {
					command: formattedCommand,
					callback: callback,
					timestamp: Date.now()
				};
				
				// Set timeout to clean up callbacks that never receive a response
				setTimeout(() => {
					if (this._responseCallbacks[commandId]) {
						callback({
							success: false,
							error: 'Command timed out',
							command: formattedCommand
						});
						delete this._responseCallbacks[commandId];
					}
				}, 10000); // 10 second timeout
			}
			
			return true;
		} catch (error) {
			console.error('Error executing command:', error);
			if (callback) {
				callback({
					success: false,
					error: error.message,
					command: formattedCommand
				});
			}
			return false;
		}
	},

	/**
	 * Send a message to the backend
	 * @param {string|object} message - The message to send
	 * @returns {boolean} - Success status
	 */
	sendMessage: function(message) {
		if (!window.app || !window.app.socket) {
			console.warn('No socket connection available');
			return false;
		}

		try {
			if (typeof message === 'object') {
				message = JSON.stringify(message);
			}
			window.app.socket.sendMessage(message);
			return true;
		} catch (error) {
			console.error('Error sending message:', error);
			return false;
		}
	},

	/**
	 * Register a handler for a specific message type
	 * @param {string} messageType - The type of message to handle
	 * @param {Function} handler - The handler function
	 */
	registerMessageHandler: function(messageType, handler) {
		if (!this._messageHandlers[messageType]) {
			this._messageHandlers[messageType] = [];
		}
		this._messageHandlers[messageType].push(handler);
	},

	/**
	 * Unregister a handler for a specific message type
	 * @param {string} messageType - The type of message
	 * @param {Function} handler - The handler function to remove
	 */
	unregisterMessageHandler: function(messageType, handler) {
		if (this._messageHandlers[messageType]) {
			const index = this._messageHandlers[messageType].indexOf(handler);
			if (index !== -1) {
				this._messageHandlers[messageType].splice(index, 1);
			}
		}
	},

	/**
	 * Handle incoming messages from the backend
	 * @param {object} message - The message to handle
	 * @returns {boolean} - Whether the message was handled
	 */
	handleMessage: function(message) {
		if (!message) {
			return false;
		}

		// First check if this is a response to a command
		for (const commandId in this._responseCallbacks) {
			const callbackInfo = this._responseCallbacks[commandId];
			
			// Check if this message is a response to the command
			// This logic will need to be adjusted based on your actual message format
			if (message.command === callbackInfo.command || 
				(message.data && message.data.command === callbackInfo.command)) {
				
				callbackInfo.callback(message);
				delete this._responseCallbacks[commandId];
				return true;
			}
		}

		// Then check registered message handlers
		if (message.type && this._messageHandlers[message.type]) {
			const handlers = this._messageHandlers[message.type];
			let handled = false;
			
			handlers.forEach(handler => {
				if (handler(message)) {
					handled = true;
				}
			});
			
			return handled;
		}

		return false;
	},

	/**
	 * Get context from the document (selection, etc.)
	 * @param {Function} callback - Callback to receive the context
	 */
	getContext: function(callback) {
		if (!callback || typeof callback !== 'function') {
			return false;
		}

		// Implementation depends on your specific needs
		const context = {
			selection: '', // Would be populated with actual selection
			documentType: this._map ? this._map.getDocType() : '',
			viewId: window.app ? window.app.getViewId() : -1
		};
		
		callback(context);
		return true;
	},

	/**
	 * Add a snippet to the document
	 * @param {string} content - The content to add
	 * @param {Function} callback - Optional callback for result
	 */
	addSnippet: function(content, callback) {
		if (!content) {
			if (callback) callback({ success: false, error: 'No content provided' });
			return false;
		}

		// Implementation depends on your specific needs
		// This is a placeholder
		console.log('Adding snippet:', content);
		
		if (callback) {
			callback({ success: true });
		}
		return true;
	},

	/**
	 * Send an input to the model and handle the response
	 * @param {string} input - The input text to send
	 * @param {Function} callback - Callback function to handle the response
	 * @returns {boolean} - Success status
	 */
	sendInput: function(input, callback) {
		console.log('Model.sendInput called with:', input);
		if (!input || input.trim() === '') {
			return false;
		}

		if (!window.app || !window.app.socket) {
			console.warn('No socket connection available');
			if (callback) {
				callback({
					success: false,
					error: 'No socket connection available'
				});
			}
			return false;
		}

		// Generate a unique input ID for tracking responses
		const inputId = 'input_' + (++this._commandCounter);
		
		// Format the input message
		const formattedInput = 'model ' + input;
		
		// Send input to backend
		try {
			window.app.socket.sendMessage(formattedInput);
			
			// Register callback if provided
			if (callback && typeof callback === 'function') {
				this._responseCallbacks[inputId] = {
					command: formattedInput,
					callback: callback,
					timestamp: Date.now()
				};
				
				// Set timeout to clean up callbacks that never receive a response
				setTimeout(() => {
					if (this._responseCallbacks[inputId]) {
						callback({
							success: false,
							error: 'Input response timed out',
							input: input
						});
						delete this._responseCallbacks[inputId];
					}
				}, 10000); // 10 second timeout
			}
			
			return true;
		} catch (error) {
			console.error('Error sending input:', error);
			if (callback) {
				callback({
					success: false,
					error: error.message,
					input: input
				});
			}
			return false;
		}
	},

	/**
	 * Handle model responses from the backend
	 * @param {Object} e - Event object containing the response data
	 * @private
	 */
	_onModelResponse: function(e) {
		if (!e.data) return;
		
		console.log('Model received model response:', e.data);
		
		// Parse the response
		let response = e.data;
		let parsedResponse = null;
		
		// Check if this is a model response
		if (typeof response === 'string' && response.startsWith('modelresponse')) {
			// Extract the actual response text
			const parts = response.split(' ');
			const status = parts[1]; // 'success' or 'error'
			
			if (status === 'error') {
				console.error('Model response error:', response);
				return;
			}
			
			// Skip "modelresponse success" and parse the rest as JSON
			try {
				const jsonStr = parts.slice(2).join(' ');
				parsedResponse = JSON.parse(jsonStr);
				console.log('Parsed model response:', parsedResponse);
			} catch (error) {
				console.error('Error parsing model response:', error);
				return;
			}
		}
		
		if (!parsedResponse) return;
		
		// Process the structured response
		this._processStructuredResponse(parsedResponse);
	},
	
	/**
	 * Process a structured response with conversation, snippet, and commands
	 * @param {Object} response - The structured response object
	 * @private
	 */
	_processStructuredResponse: function(response) {
		console.log('Processing structured response:', response);
		
		// Extract the result if it's nested
		let result = response;
		if (response.result && typeof response.result === 'object') {
			result = response.result;
		}
		
		// Process commands if present
		if (result.commands && Array.isArray(result.commands)) {
			this._executeCommandsSilently(result.commands);
		}
		
		// Process snippet if present
		if (result.snippet) {
			// Fire an event for the snippet that the sidebar can listen for
			if (window.app && window.app.map) {
				window.app.map.fire('modelsnippet', { 
					snippet: result.snippet,
					header: this._extractSnippetHeader(result.snippet)
				});
			}
		}
		
		// Process conversation if present
		if (result.conversation) {
			// Fire an event for the conversation that the sidebar can listen for
			if (window.app && window.app.map) {
				window.app.map.fire('modelconversation', { 
					conversation: result.conversation 
				});
			}
		}
	},
	
	/**
	 * Extract snippet header if it exists
	 * @param {string} snippet - The snippet text
	 * @returns {string} - The extracted header or empty string
	 * @private
	 */
	_extractSnippetHeader: function(snippet) {
		if (typeof snippet === 'string') {
			// Check if the snippet has a header in the format [snippet:Header]
			const snippetMatch = snippet.match(/\[snippet:(.*?)\]/);
			if (snippetMatch && snippetMatch[1]) {
				return snippetMatch[1].trim();
			}
		}
		return '';
	},
	
	/**
	 * Execute commands silently without user interaction
	 * @param {string[]} commands - Array of commands to execute
	 * @private
	 */
	_executeCommandsSilently: function(commands) {
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
	},

	/**
	 * Format a response for display
	 * @param {any} response - The response to format
	 * @returns {string} - Formatted response string
	 */
	formatResponse: function(response) {
		console.log('Formatting response:', response);
		
		// Check if this is a model response
		if (typeof response === 'string' && response.startsWith('modelresponse')) {
			// Extract the actual response text
			const parts = response.split(' ');
			if (parts.length >= 3) {
				// Skip "modelresponse" and status, return the rest
				return parts.slice(2).join(' ');
			}
			return response.substring('modelresponse '.length);
		}
		
		// Handle structured response with conversation, snippet, and commands
		if (response && typeof response === 'object') {
			// Check if the response has the structured format directly
			if (response.conversation !== undefined || response.snippet !== undefined) {
				// Process the structured response
				this._processStructuredResponse(response);
				
				// Return only the conversation part for display
				return response.conversation || '';
			}
			
			// Check if the response has a result property that contains the structured format
			if (response.result && typeof response.result === 'object') {
				const result = response.result;
				if (result.conversation !== undefined || result.snippet !== undefined) {
					// Process the structured response
					this._processStructuredResponse(result);
					
					// Return only the conversation part for display
					return result.conversation || '';
				}
			}
		}
		
		// Handle standard responses
		if (response.success === true) {
			return `✓ Input processed successfully`;
		} else if (response.success === false) {
			return `✗ Error: ${response.error || 'Input processing failed'}`;
		} else if (response.result) {
			return response.result;
		}
		
		// Fallback to JSON string
		return JSON.stringify(response);
	},
});

L.model = function (options) {
	return new L.Model(options);
};
