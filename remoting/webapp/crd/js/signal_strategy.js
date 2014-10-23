// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Abstract interface for various signaling mechanisms.
 *
 * @interface
 * @extends {base.Disposable}
 */
remoting.SignalStrategy = function() {};

/**
 * @enum {number} SignalStrategy states. Possible state transitions:
 *    NOT_CONNECTED -> CONNECTING (connect() called).
 *    CONNECTING -> HANDSHAKE (connected successfully).
 *    HANDSHAKE -> CONNECTED (authenticated successfully).
 *    CONNECTING -> FAILED (connection failed).
 *    HANDSHAKE -> FAILED (authentication failed).
 *    * -> CLOSED (dispose() called).
 */
remoting.SignalStrategy.State = {
  NOT_CONNECTED: 0,
  CONNECTING: 1,
  HANDSHAKE: 2,
  CONNECTED: 3,
  FAILED: 4,
  CLOSED: 5
};

remoting.SignalStrategy.prototype.dispose = function() {};

/**
 * @param {?function(Element):void} onIncomingStanzaCallback Callback to call on
 *     incoming messages.
 */
remoting.SignalStrategy.prototype.setIncomingStanzaCallback =
    function(onIncomingStanzaCallback) {};

/**
 * @param {string} server
 * @param {string} username
 * @param {string} authToken
 */
remoting.SignalStrategy.prototype.connect =
    function(server, username, authToken) {
};

/**
 * Sends a message. Can be called only in CONNECTED state.
 * @param {string} message
 */
remoting.SignalStrategy.prototype.sendMessage = function(message) {};

/** @return {remoting.SignalStrategy.State} Current state */
remoting.SignalStrategy.prototype.getState = function() {};

/** @return {remoting.Error} Error when in FAILED state. */
remoting.SignalStrategy.prototype.getError = function() {};

/** @return {string} Current JID when in CONNECTED state. */
remoting.SignalStrategy.prototype.getJid = function() {};

/**
 * Creates the appropriate signal strategy for the current environment.
 * @param {function(remoting.SignalStrategy.State): void} onStateChangedCallback
 * @return {remoting.SignalStrategy} New signal strategy object.
 */
remoting.SignalStrategy.create = function(onStateChangedCallback) {
  // Only use XMPP when TCP API is available and TLS support is enabled. That's
  // not the case for V1 app (socket API is available only to platform apps)
  // and for Chrome releases before 38.
  if (chrome.socket && chrome.socket.secure) {
    return new remoting.XmppConnection(onStateChangedCallback);
  } else {
    return new remoting.WcsAdapter(onStateChangedCallback);
  }
};
