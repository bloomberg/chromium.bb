// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @constructor
 */
function MessageWindowImpl() {
  /**
   * Used to prevent multiple responses due to the closeWindow handler.
   *
   * @private {boolean}
   */
  this.sentReply_ = false;

  window.addEventListener('message', this.onMessage_.bind(this), false);
};

/**
 * @param {Window} parentWindow The id of the window that showed the message.
 * @param {number} messageId The identifier of the message, as supplied by the
 *     parent.
 * @param {number} result 0 if window was closed without pressing a button;
 *     otherwise the index of the button pressed (e.g., 1 = primary).
 * @private
 */
MessageWindowImpl.prototype.sendReply_ = function(
    parentWindow, messageId, result) {
  // Only forward the first reply that we receive.
  if (!this.sentReply_) {
    var message = {
      command: 'messageWindowResult',
      id: messageId,
      result: result
    };
    parentWindow.postMessage(message, '*');
    this.sentReply_ = true;
  } else {
    // Make sure that the reply we're ignoring is from the window close.
    base.debug.assert(result == 0);
  }
};

/**
 * Updates the button label text.
 * Hides the button if the label is null or undefined.
 *
 * @param{HTMLElement} button
 * @param{?string} label
 * @private
 */
MessageWindowImpl.prototype.updateButton_ = function(button, label) {
  if (label) {
    button.innerText = label;
  }
  button.hidden = !Boolean(label);
};

/**
 * Event-handler callback, invoked when the parent window supplies the
 * message content.
 *
 * @param{Event} event
 * @private
 */
MessageWindowImpl.prototype.onMessage_ = function(event) {
  var command = /** @type {string} */ (event.data['command']);
  if (command !== 'show' && command !== 'update') {
    console.error('Unexpected message: ' + command);
    return;
  }

  // Validate the message.
  var messageId = /** @type {number} */ (event.data['id']);
  var title = /** @type {string} */ (event.data['title']);
  var message = /** @type {string} */ (event.data['message']);
  var infobox = /** @type {string} */ (event.data['infobox']);
  var buttonLabel = /** @type {string} */ (event.data['buttonLabel']);
  var cancelButtonLabel = /** @type {string} */
      (event.data['cancelButtonLabel']);
  var showSpinner = /** @type {boolean} */ (event.data['showSpinner']);

  // Many of these fields are optional for either the 'show' or 'update'
  // message. These vars are used to mark the optional fields to allow
  // them to be undefined.
  var optionalFieldShow = command === 'show';
  var optionalFieldUpdate = command === 'update';
  if (isNumber(messageId) ||
      isString(title, optionalFieldUpdate) ||
      isString(message, optionalFieldUpdate) ||
      isString(infobox, optionalFieldUpdate) ||
      isString(buttonLabel, optionalFieldUpdate) ||
      isString(cancelButtonLabel, optionalFieldShow || optionalFieldUpdate) ||
      isBoolean(showSpinner, optionalFieldUpdate) {
    console.log('Bad ' + command + ' message: ' + event.data);
    return;
  }

  var button = document.getElementById('button-primary');
  var cancelButton = document.getElementById('button-secondary');
  var messageDiv = document.getElementById('message');
  var infoboxDiv = document.getElementById('infobox');

  if (isString(title)) {
    document.getElementById('title').innerText = title;
    document.querySelector('title').innerText = title;
  }
  if (isString(message) {
    messageDiv.innerText = message;
  }
  if (isString(infobox)) {
    if (infobox != '') {
      infoboxDiv.innerText = infobox;
    } else {
      infoboxDiv.hidden = true;
    }
  }
  if (isBoolean(showSpinner)) {
    if (showSpinner) {
      messageDiv.classList.add('waiting');
      messageDiv.classList.add('prominent');
    } else {
      messageDiv.classList.remove('waiting');
      messageDiv.classList.remove('prominent');
    }
  }
  this.updateButton_(button, buttonLabel);
  this.updateButton_(cancelButton, cancelButtonLabel);

  base.resizeWindowToContent(true);

  if (command === 'show') {
    // Set up click-handlers for the buttons.
    button.addEventListener(
        'click', this.sendReply_.bind(this, event.source, messageId, 1), false);
    cancelButton.addEventListener(
        'click', this.sendReply_.bind(this, event.source, messageId, 0), false);

    var buttonToFocus = (cancelButtonLabel) ? cancelButton : button;
    buttonToFocus.focus();

    // Add a close handler in case the window is closed without clicking one
    // of the buttons. This will send a 0 as the result.
    // Note that when a button is pressed, this will result in sendReply_
    // being called multiple times (once for the button, once for close).
    chrome.app.window.current().onClosed.addListener(
        this.sendReply_.bind(this, event.source, messageId, 0));

    chrome.app.window.current().show();
  }
};

var messageWindow = new MessageWindowImpl();
