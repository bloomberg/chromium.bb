// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Create a new message window.
 *
 * @param {Object} options Message window create options
 * @constructor
 */
remoting.MessageWindow = function(options) {
  var title = /** @type {string} */ (options.title);
  var message = /** @type {string} */ (options.message);
  var okButtonLabel = /** @type {string} */ (options.buttonLabel);
  var cancelButtonLabel = /** @type {string} */ (options.cancelButtonLabel);
  var onResult = /** @type {function(number):void} */(options.onResult);
  /** @type {number} */
  var duration = 0;
  if (/** @type {number?} */(options.duration)) {
    duration = /** @type {number} */(options.duration);
  }
  /** @type {string} */
  var infobox = '';
  if (/** @type {string?} */(options.infobox)) {
    infobox = /** @type {string} */(options.infobox);
  }
  var onTimeout = /** @type {?function():void} */ (options.onTimeout);

  /** @type {number} */
  this.id_ = remoting.MessageWindowManager.addMessageWindow(this);

  /** @type {?function(number):void} */
  this.onResult_ = onResult;

  /** @type {Window} */
  this.window_ = null;

  /** @type {number} */
  this.timer_ = 0;

  /** @type {Array.<function():void>} */
  this.pendingWindowOperations_ = [];

  /**
   * Callback to call when the timeout expires.
   * @type {?function():void}
   */
  this.onTimeout_ = onTimeout;

  var message_struct = {
    command: 'show',
    id: this.id_,
    title: title,
    message: message,
    infobox: infobox,
    buttonLabel: okButtonLabel,
    cancelButtonLabel: cancelButtonLabel,
    showSpinner: (duration != 0)
  };

  var windowAttributes = {
    bounds: {
      width: 400,
      height: 100
    },
    resizable: false
  };

  /** @type {remoting.MessageWindow} */
  var that = this;

  /** @param {AppWindow} appWindow */
  var onCreate = function(appWindow) {
    that.setWindow_(/** @type {Window} */(appWindow.contentWindow));
    var onLoad = function() {
      appWindow.contentWindow.postMessage(message_struct, '*');
    };
    appWindow.contentWindow.addEventListener('load', onLoad, false);
  };

  chrome.app.window.create('message_window.html', windowAttributes, onCreate);

  if (duration != 0) {
    this.timer_ = window.setTimeout(this.onTimeoutHandler_.bind(this),
                                    duration);
  }
};

/**
 * Called when the timer runs out. This in turn calls the window's
 * timeout handler (if any).
 */
remoting.MessageWindow.prototype.onTimeoutHandler_ = function() {
  this.close();
  if (this.onTimeout_) {
    this.onTimeout_();
  }
};

/**
 * Update the message being shown in the window. This should only be called
 * after the window has been shown.
 *
 * @param {string} message The message.
 */
remoting.MessageWindow.prototype.updateMessage = function(message) {
  if (!this.window_) {
    this.pendingWindowOperations_.push(this.updateMessage.bind(this, message));
    return;
  }

  var message_struct = {
    command: 'update_message',
    message: message
  };
  this.window_.postMessage(message_struct, '*');
};

/**
 * Close the message box and unregister it with the window manager.
 */
remoting.MessageWindow.prototype.close = function() {
  if (!this.window_) {
    this.pendingWindowOperations_.push(this.close.bind(this));
    return;
  }

  if (this.timer_) {
    window.clearTimeout(this.timer_);
  }
  this.timer_ = 0;

  // Unregister the window with the window manager.
  // After this call, events sent to this window will no longer trigger the
  // onResult callback.
  remoting.MessageWindowManager.deleteMessageWindow(this.id_);
  this.window_.close();
  this.window_ = null;
};

/**
 * Dispatch a message box result to the registered callback.
 *
 * @param {number} result The dialog result.
 */
remoting.MessageWindow.prototype.handleResult = function(result) {
  if (this.onResult_) {
    this.onResult_(result);
  }
}

/**
 * Set the window handle and run any pending operations that require it.
 *
 * @param {Window} window
 * @private
 */
remoting.MessageWindow.prototype.setWindow_ = function(window) {
  base.debug.assert(this.window_ == null);
  this.window_ = window;
  for (var i = 0; i < this.pendingWindowOperations_.length; ++i) {
    var pendingOperation = this.pendingWindowOperations_[i];
    pendingOperation();
  }
  this.pendingWindowOperations_ = [];
};

/**
 * Static method to create and show a confirm message box.
 *
 * @param {string} title The title of the message box.
 * @param {string} message The message.
 * @param {string} okButtonLabel The text for the primary button.
 * @param {string} cancelButtonLabel The text for the secondary button.
 * @param {function(number):void} onResult The callback to invoke when the
 *     user closes the message window.
 * @return {remoting.MessageWindow}
 */
remoting.MessageWindow.showConfirmWindow = function(
    title, message, okButtonLabel, cancelButtonLabel, onResult) {
  var options = {
    title: title,
    message: message,
    buttonLabel: okButtonLabel,
    cancelButtonLabel: cancelButtonLabel,
    onResult: onResult
  };
  return new remoting.MessageWindow(options);
};

/**
 * Static method to create and show a simple message box.
 *
 * @param {string} title The title of the message box.
 * @param {string} message The message.
 * @param {string} buttonLabel The text for the primary button.
 * @param {function(number):void} onResult The callback to invoke when the
 *     user closes the message window.
 * @return {remoting.MessageWindow}
 */
remoting.MessageWindow.showMessageWindow = function(
    title, message, buttonLabel, onResult) {
  var options = {
    title: title,
    message: message,
    buttonLabel: buttonLabel,
    onResult: onResult
  };
  return new remoting.MessageWindow(options);
};

/**
 * Static method to create and show an error message box with an "OK" button.
 * The app will close when the user dismisses the message window.
 *
 * @param {string} title The title of the message box.
 * @param {string} message The message.
 * @return {remoting.MessageWindow}
 */
remoting.MessageWindow.showErrorMessage = function(title, message) {
  var options = {
    title: title,
    message: message,
    buttonLabel: chrome.i18n.getMessage(/**i18n-content*/'OK'),
    onResult: remoting.MessageWindow.quitApp
  };
  return new remoting.MessageWindow(options);
};

/**
 * Static method to create and show a timed message box.
 *
 * @param {string} title The title of the message box.
 * @param {string} message The message.
 * @param {string} infobox Additional information to be displayed in an infobox,
 *     or the empty string if there is no additional information.
 * @param {string} buttonLabel The text for the primary button.
 * @param {function(number):void} onResult The callback to invoke when the
 *     user closes the message window.
 * @param {number} duration Time for wait before calling onTime
 * @param {?function():void} onTimeout Callback function.
 * @return {remoting.MessageWindow}
 */
remoting.MessageWindow.showTimedMessageWindow = function(
    title, message, infobox, buttonLabel, onResult, duration, onTimeout) {
  var options = {
    title: title,
    message: message,
    infobox: infobox,
    buttonLabel: buttonLabel,
    onResult: onResult,
    duration: duration,
    onTimeout: onTimeout
  };
  return new remoting.MessageWindow(options);
};

/**
 * Cancel the current connection and close all app windows.
 *
 * @param {number} result The dialog result.
 */
remoting.MessageWindow.quitApp = function(result) {
  remoting.MessageWindowManager.closeAllMessageWindows();
  window.close();
};
