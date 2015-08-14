// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Simplified SignalStrategy implementation that wraps an underlying signal
 * strategy and forwards messages when it is CONNECTED. It is used only to
 * log status messages during IT2Me connection setup, and only those methods
 * required for that are implemented.
 *
 * TODO(jamiewalch): Remove this class once a signal strategy is no longer
 *     required for logging (crbug.com/497269).
 *
 * @constructor
 * @param {remoting.SignalStrategy} underlying The underlying implementation.
 * @implements {remoting.SignalStrategy}
 */
remoting.BufferedSignalStrategy = function(underlying) {
  /** @private */
  this.underlying_ = underlying;
  /** @private {Array<string>} */
  this.pendingMessages_ = [];

  this.underlying_.setStateChangedCallback(this.flush_.bind(this));
};

remoting.BufferedSignalStrategy.prototype.dispose = function() {
  this.underlying_.dispose();
};

/**
 * Sends a message. Can be called only in CONNECTED state.
 * @param {string} message
 */
remoting.BufferedSignalStrategy.prototype.sendMessage = function(message) {
  this.pendingMessages_.push(message);
  this.flush_();
};

/**
 * If the underlying implementation is connected, flush all pending messages.
 * @private
 */
remoting.BufferedSignalStrategy.prototype.flush_ = function() {
  if (this.underlying_.getState() !== remoting.SignalStrategy.State.CONNECTED) {
    return;
  }
  for (var i = 0; i < this.pendingMessages_.length; ++i) {
    this.underlying_.sendMessage(this.pendingMessages_[i]);
  }
  this.pendingMessages_ = [];
};


// The following methods are not used by LogToServer and are not implemented.

remoting.BufferedSignalStrategy.prototype.setStateChangedCallback =
    function(onStateChangedCallback) {
  console.error('Unexpected setStateChangedCallback().');
};

remoting.BufferedSignalStrategy.prototype.setIncomingStanzaCallback =
    function(onIncomingStanzaCallback) {
  console.error('Unexpected setIncomingStanzaCallback().');
};

remoting.BufferedSignalStrategy.prototype.connect =
    function(server, username, authToken) {
  console.error('Unexpected connect().');
};

remoting.BufferedSignalStrategy.prototype.getState = function() {
  console.error('Unexpected getState().');
};

remoting.BufferedSignalStrategy.prototype.getError = function() {
  console.error('Unexpected getError().');
};

remoting.BufferedSignalStrategy.prototype.getJid = function() {
  console.error('Unexpected getJid().');
};

remoting.BufferedSignalStrategy.prototype.getType = function() {
  console.error('Unexpected getType().');
};
