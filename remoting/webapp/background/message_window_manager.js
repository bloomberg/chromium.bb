// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Namespace for window manager functions.
 * @type {Object}
 */
remoting.MessageWindowManager = {};

/**
 * Mapping from window id to corresponding MessageWindow.
 *
 * @type {Object.<number, remoting.MessageWindow>}
 * @private
 */
remoting.MessageWindowManager.messageWindows_ = {};

/**
 * The next window id to auto-assign.
 * @type {number}
 * @private
 */
remoting.MessageWindowManager.nextId_ = 1;

/**
 * @param {remoting.MessageWindow} window The window to associate
 *     with the window id.
 * @return {number} The window id.
 */
remoting.MessageWindowManager.addMessageWindow = function(window) {
  var id = ++remoting.MessageWindowManager.nextId_;
  remoting.MessageWindowManager.messageWindows_[id] = window;
  return id;
};

/**
 * @param {number} id The window id.
 * @return {remoting.MessageWindow}
 */
remoting.MessageWindowManager.getMessageWindow = function(id) {
  return remoting.MessageWindowManager.messageWindows_[id];
};

/**
 * @param {number} id The window id to delete.
 */
remoting.MessageWindowManager.deleteMessageWindow = function(id) {
  delete remoting.MessageWindowManager.messageWindows_[id];
};

/**
 * Close all of the registered MessageWindows
 */
remoting.MessageWindowManager.closeAllMessageWindows = function() {
  /** @type {Array.<remoting.MessageWindow>} */
  var windows = [];
  // Make a list of the windows to close.
  // We don't delete the window directly in this loop because close() can
  // call deleteMessageWindow which will update messageWindows_.
  for (var win_id in remoting.MessageWindowManager.messageWindows_) {
    /** @type {remoting.MessageWindow} */
    var win = remoting.MessageWindowManager.getMessageWindow(
        /** @type {number} */(win_id));
    base.debug.assert(win != null);
    windows.push(win);
  }
  for (var i = 0; i < windows.length; i++) {
    /** @type {remoting.MessageWindow} */(windows[i]).close();
  }
};

/**
 * Dispatch a message box result to the appropriate callback.
 *
 * @param {Event} event
 * @private
 */
remoting.MessageWindowManager.onMessage_ = function(event) {
  if (typeof(event.data) != 'object') {
    return;
  }

  if (event.data['command'] == 'messageWindowResult') {
    var id = /** @type {number} */ (event.data['id']);
    var result = /** @type {number} */ (event.data['result']);

    if (typeof(id) != 'number' || typeof(result) != 'number') {
      console.log('Poorly formatted id or result');
      return;
    }

    var messageWindow = remoting.MessageWindowManager.getMessageWindow(id);
    if (!messageWindow) {
      console.log('Ignoring unknown message window id:', id);
      return;
    }

    messageWindow.handleResult(result);
    messageWindow.close();
  }
};


window.addEventListener('message', remoting.MessageWindowManager.onMessage_,
                        false);
