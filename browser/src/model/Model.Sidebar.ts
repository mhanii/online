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
 * JSDialog.ModelSidebar
 */

/* global app */
interface ModelSidebarOptions {
    animSpeed: number;
}

interface MessageItem {
    type: 'command' | 'response';
    content: string;
    timestamp: Date;
}

class ModelSidebar {
    options: ModelSidebarOptions;
    map: any;
    container: HTMLDivElement;
    builder: any;
    targetDeckCommand: string;
    
    // Command input elements
    commandInput: HTMLInputElement;
    commandButton: HTMLButtonElement;
    commandHistory: string[] = [];
    historyIndex: number = -1;
    
    // Conversation elements
    conversationContainer: HTMLDivElement;
    messages: MessageItem[] = [];
    
    // DOM elements
    headerElement: HTMLDivElement;
    contentElement: HTMLDivElement;
    commandAreaElement: HTMLDivElement;

    constructor(
        map: any,
        options: ModelSidebarOptions = {
            animSpeed: 1000, /* Default speed: to be used on load */
        },
    ) {
        this.options = options;
        this.onAdd(map);
    }

    onAdd(map: ReturnType<typeof L.map>) {
        this.map = map;

        app.events.on('resize', this.onResize.bind(this));

        this.builder = new L.control.jsDialogBuilder({
            mobileWizard: this,
            map: map,
            cssClass: 'jsdialog ModelSidebar',
        });
        
        // Create main container
        this.container = L.DomUtil.create(
            'div',
            'ModelSidebar-container',
            $('#ModelSidebar-panel').get(0),
        );
        
        // Create sidebar header
        this.headerElement = L.DomUtil.create('div', 'ModelSidebar-header', this.container);
        const titleElement = L.DomUtil.create('h2', 'ModelSidebar-title', this.headerElement);
        titleElement.innerText = 'Model Sidebar';
        
        const closeButton = L.DomUtil.create('button', 'ModelSidebar-close-btn', this.headerElement);
        closeButton.innerHTML = '&times;';
        closeButton.setAttribute('aria-label', 'Close sidebar');
        closeButton.addEventListener('click', () => this.closeModelSidebar());
        
        // Create content area with tabs
        const tabsContainer = L.DomUtil.create('div', 'ModelSidebar-tabs', this.container);
        
        const contentTab = L.DomUtil.create('button', 'ModelSidebar-tab', tabsContainer);
        contentTab.innerText = 'Content';
        contentTab.classList.add('active');
        
        const chatTab = L.DomUtil.create('button', 'ModelSidebar-tab', tabsContainer);
        chatTab.innerText = 'Commands';
        
        // Create tabbed content areas
        this.contentElement = L.DomUtil.create('div', 'ModelSidebar-content tab-panel', this.container);
        this.contentElement.classList.add('active');
        
        this.conversationContainer = L.DomUtil.create('div', 'ModelSidebar-content tab-panel conversation-container', this.container);
        
        // Add tab switching functionality
        contentTab.addEventListener('click', () => {
            contentTab.classList.add('active');
            chatTab.classList.remove('active');
            this.contentElement.classList.add('active');
            this.conversationContainer.classList.remove('active');
        });
        
        chatTab.addEventListener('click', () => {
            chatTab.classList.add('active');
            contentTab.classList.remove('active');
            this.conversationContainer.classList.add('active');
            this.contentElement.classList.remove('active');
        });
        
        // Create command input area
        this.commandAreaElement = L.DomUtil.create('div', 'ModelSidebar-command-area', this.container);
        const commandContainer = L.DomUtil.create('div', 'command-input-container', this.commandAreaElement);
        
        // Create command input field
        this.commandInput = L.DomUtil.create('input', 'command-input', commandContainer) as HTMLInputElement;
        this.commandInput.type = 'text';
        this.commandInput.placeholder = 'Enter UNO command...';
        this.commandInput.setAttribute('aria-label', 'Command input');
        
        // Add key event listeners
        this.commandInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                this.executeCommand();
            } else if (e.key === 'ArrowUp') {
                this.navigateHistory(-1);
                e.preventDefault();
            } else if (e.key === 'ArrowDown') {
                this.navigateHistory(1);
                e.preventDefault();
            }
        });
        
        // Create send button
        this.commandButton = L.DomUtil.create('button', 'command-send-btn', commandContainer) as HTMLButtonElement;
        this.commandButton.innerText = 'Send';
        this.commandButton.setAttribute('aria-label', 'Send command');
        this.commandButton.addEventListener('click', () => this.executeCommand());
        
        // Add command tooltip (hidden by default)
        const commandTooltip = L.DomUtil.create('div', 'command-history-tooltip', this.commandAreaElement);
        commandTooltip.innerText = 'Use ↑ ↓ keys to navigate command history';
        
        // Show tooltip when input field is focused
        this.commandInput.addEventListener('focus', () => {
            if (this.commandHistory.length > 0) {
                commandTooltip.classList.add('visible');
                setTimeout(() => {
                    commandTooltip.classList.remove('visible');
                }, 3000);
            }
        });

        this.map.on('modelsidebar', this.onModelSidebar, this);
        this.map.on('jsdialogupdate', this.onJSUpdate, this);
        this.map.on('jsdialogaction', this.onJSAction, this);
        
        // Listen for command responses
        // app.socket.addEventListener('message', (e: MessageEvent) => {
        //     try {
        //         const response = JSON.parse(e.data);
        //         // Filter to only handle responses related to commands
        //         if (response.command && this.isCommandResponse(response)) {
        //             this.addResponseMessage(this.formatCommandResponse(response));
        //         }
        //     } catch (error) {
        //         // Skip non-JSON messages
        //     }
        // });
    }
    
    isCommandResponse(response: any): boolean {
        // Check if this is a command response
        // This will need to be adjusted based on the actual response structure
        return response.command === 'commandresult' || 
               (response.command === 'reply' && response.success !== undefined);
    }
    
    formatCommandResponse(response: any): string {
        // Format the response for display
        if (response.success === true) {
            return `✓ Command executed successfully`;
        } else if (response.success === false) {
            return `✗ Error: ${response.error || 'Command failed'}`;
        } else if (response.result) {
            return response.result;
        }
        return JSON.stringify(response);
    }
    
    addCommandMessage(command: string) {
        const message: MessageItem = {
            type: 'command',
            content: command,
            timestamp: new Date()
        };
        this.messages.push(message);
        this.renderConversation();
        this.addResponseMessage("Testing response..");
    }
    
    addResponseMessage(response: string) {
        const message: MessageItem = {
            type: 'response',
            content: response,
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
            emptyMessage.innerText = 'No commands have been sent yet.';
            return;
        }
        
        // Create message bubbles for each message
        this.messages.forEach(message => {
            const messageElement = L.DomUtil.create('div', `message-bubble ${message.type}`, this.conversationContainer);
            
            // Add icon based on message type
            const iconElement = L.DomUtil.create('span', 'message-icon', messageElement);
            iconElement.innerText = message.type === 'command' ? '➤' : '←';
            
            // Add content
            const contentElement = L.DomUtil.create('div', 'message-content', messageElement);
            contentElement.innerText = message.content;
            
            // Add timestamp
            const timeString = message.timestamp.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
            const timeElement = L.DomUtil.create('div', 'message-time', messageElement);
            timeElement.innerText = timeString;
        });
        
        // Scroll to bottom
        this.conversationContainer.scrollTop = this.conversationContainer.scrollHeight;
    }

    onRemove() {
        this.map.off('modelsidebar');
        this.map.off('jsdialogupdate', this.onJSUpdate, this);
        this.map.off('jsdialogaction', this.onJSAction, this);
    }

    isVisible(): boolean {
        return $('#ModelSidebar-dock-wrapper').hasClass('visible');
    }

    closeModelSidebar() {
        $('#ModelSidebar-dock-wrapper').removeClass('visible');
        this.map._onResize();

        if (!this.map.editorHasFocus()) {
            this.map.fire('editorgotfocus');
            this.map.focus();
        }

        this.map.uiManager.setDocTypePref('ShowModelSidebar', false);
    }
    
    executeCommand() {
        const command = this.commandInput.value.trim();
        if (!command) return;
        
        // Add to command history
        if (!this.commandHistory.includes(command)) {
            this.commandHistory.push(command);
            // Limit history size
            if (this.commandHistory.length > 50) {
                this.commandHistory.shift();
            }
        }
        this.historyIndex = this.commandHistory.length;
        
        // Format command if needed
        let formattedCommand = command;
        if (!command.startsWith('uno ') && !command.startsWith('.uno:')) {
            formattedCommand = '.uno:' + command;
        }
        
        // Add to conversation
        this.addCommandMessage(formattedCommand);
        
        // Send command to backend
        if (formattedCommand.startsWith('.uno:')) {
            app.socket.sendMessage('uno ' + formattedCommand);
        } else {
            app.socket.sendMessage(formattedCommand);
        }
        
        // Log command
        window.app.console.log('ModelSidebar: Command sent: ' + formattedCommand);
        
        // Clear input
        this.commandInput.value = '';
        this.commandInput.focus();
        
        // Switch to chat tab to see the command
        document.querySelector('.sidebar-tab:nth-child(2)').dispatchEvent(new Event('click'));
    }
    
    navigateHistory(direction: number) {
        if (this.commandHistory.length === 0) return;
        
        this.historyIndex += direction;
        
        if (this.historyIndex < 0) {
            this.historyIndex = 0;
        } else if (this.historyIndex > this.commandHistory.length) {
            this.historyIndex = this.commandHistory.length;
        }
        
        if (this.historyIndex === this.commandHistory.length) {
            this.commandInput.value = '';
        } else {
            this.commandInput.value = this.commandHistory[this.historyIndex];
        }
        
        // Position cursor at the end
        setTimeout(() => {
            this.commandInput.selectionStart = this.commandInput.value.length;
            this.commandInput.selectionEnd = this.commandInput.value.length;
        }, 0);
    }

    onJSUpdate(e: FireEvent) {
        var data = e.data;

        if (data.jsontype !== 'ModelSidebar') return;
        if (!this.contentElement) return;
        if (!this.builder) return;

        // reduce unwanted warnings in console
        if (data.control && data.control.id === 'addonimage') {
            window.app.console.log('Ignored update for control: ' + data.control.id);
            return;
        }

        if (data.control && this.getTargetDeck() === this.commandForDeck('NavigatorDeck')) {
            this.markNavigatorTreeView(data.control);
        }

        // Clear content area
        $(this.contentElement).empty();
        
        // Update only the content area
        this.builder.updateWidget(this.contentElement, data.control);
    }

    onJSAction(e: FireEvent) {
        var data = e.data;

        if (data.jsontype !== 'ModelSidebar') return;
        if (!this.builder) return;
        if (!this.contentElement) return;

        var innerData = data.data;
        if (!innerData) return;

        var controlId = innerData.control_id;

        // Panels share the same name for main containers, do not execute actions for them
        // if panel has to be shown or hidden, full update will appear
        if (
            controlId === 'contents' ||
            controlId === 'Panel' ||
            controlId === 'titlebar' ||
            controlId === 'addonimage'
        ) {
            window.app.console.log(
                'Ignored action: ' +
                    innerData.action_type +
                    ' for control: ' +
                    controlId,
            );
            return;
        }

        this.builder.executeAction(this.contentElement, innerData);
    }

    onResize() {
        var wrapper = document.getElementById('ModelSidebar-dock-wrapper');
        if (wrapper) {
            wrapper.style.maxHeight =
                document.getElementById('document-container').getBoundingClientRect()
                    .height + 'px';
        }
    }

    unsetSelectedModelSidebar() {
        this.map.uiManager.setDocTypePref('PropertyDeck', false);
        this.map.uiManager.setDocTypePref('SdSlideTransitionDeck', false);
        this.map.uiManager.setDocTypePref('SdCustomAnimationDeck', false);
        this.map.uiManager.setDocTypePref('SdMasterPagesDeck', false);
        this.map.uiManager.setDocTypePref('NavigatorDeck', false);
        this.map.uiManager.setDocTypePref('StyleListDeck', false);
        this.map.uiManager.setDocTypePref('A11yCheckDeck', false);
    }

    commandForDeck(deckId: string): string {
        if (deckId === 'PropertyDeck') return '.uno:ModelSidebarDeck.PropertyDeck';
        else if (deckId === 'SdSlideTransitionDeck')
            return '.uno:SlideChangeWindow';
        else if (deckId === 'SdCustomAnimationDeck') return '.uno:CustomAnimation';
        else if (deckId === 'SdMasterPagesDeck') return '.uno:MasterSlidesPanel';
        else if (deckId === 'NavigatorDeck') return '.uno:Navigator';
        else if (deckId === 'StyleListDeck')
            return '.uno:ModelSidebarDeck.StyleListDeck';
        else if (deckId === 'A11yCheckDeck')
            return '.uno:ModelSidebarDeck.A11yCheckDeck';
        return '';
    }

    setupTargetDeck(unoCommand: string) {
        this.targetDeckCommand = unoCommand;
    }

    getTargetDeck(): string {
        return this.targetDeckCommand;
    }

    changeDeck(unoCommand: string | null) {
        if (unoCommand !== null && unoCommand !== undefined)
            app.socket.sendMessage('uno ' + unoCommand);
        this.setupTargetDeck(unoCommand);
    }

    onModelSidebar(data: FireEvent) {
        var ModelSidebarData = data.data;
        this.builder.setWindowId(ModelSidebarData.id);
        
        // Only clear the content area
        $(this.contentElement).empty();

        if (
            ModelSidebarData.action === 'close' ||
            window.app.file.disableModelSidebar ||
            this.map.isReadOnlyMode()
        ) {
            this.closeModelSidebar();
        } else if (ModelSidebarData.children) {
            for (var i = ModelSidebarData.children.length - 1; i >= 0; i--) {
                if (
                    ModelSidebarData.children[i].type !== 'deck' ||
                    ModelSidebarData.children[i].visible === false
                ) {
                    ModelSidebarData.children.splice(i, 1);
                    continue;
                }

                if (
                    typeof ModelSidebarData.children[i].id === 'string' &&
                    ModelSidebarData.children[i].id.startsWith('Navigator')
                ) {
                    this.markNavigatorTreeView(ModelSidebarData);
                }
            }

            if (ModelSidebarData.children.length) {
                this.onResize();

                if (
                    ModelSidebarData.children &&
                    ModelSidebarData.children[0] &&
                    ModelSidebarData.children[0].id
                ) {
                    this.unsetSelectedModelSidebar();
                    var currentDeck = ModelSidebarData.children[0].id;
                    this.map.uiManager.setDocTypePref(currentDeck, true);
                    if (this.targetDeckCommand) {
                        var stateHandler = this.map['stateChangeHandler'];
                        var isCurrent = stateHandler
                            ? stateHandler.getItemValue(this.targetDeckCommand)
                            : false;
                        // just to be sure check with other method
                        if (isCurrent === 'false' || !isCurrent)
                            isCurrent =
                                this.targetDeckCommand === this.commandForDeck(currentDeck);
                        if (this.targetDeckCommand && (isCurrent === 'false' || !isCurrent))
                            this.changeDeck(this.targetDeckCommand);
                    } else {
                        this.changeDeck(this.targetDeckCommand);
                    }
                }

                // Build content in the content area only
                this.builder.build(this.contentElement, [ModelSidebarData]);
                
                if (!this.isVisible()) {
                    $('#ModelSidebar-dock-wrapper').addClass('visible');
                }

                this.map.uiManager.setDocTypePref('ShowModelSidebar', true);
            } else {
                this.closeModelSidebar();
            }
        }
    }

    markNavigatorTreeView(data: WidgetJSON): boolean {
        if (!data) return false;

        if (data.type === 'treelistbox') {
            (data as TreeWidgetJSON).draggable = false;
            return true;
        }

        for (const i in data.children) {
            if (this.markNavigatorTreeView(data.children[i])) {
                return true;
            }
        }

        return false;
    }
}

JSDialog.ModelSidebar = function (map: any, options: ModelSidebarOptions) {
    return new ModelSidebar(map, options);
};