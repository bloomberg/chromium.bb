// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

/** @enum */
var BlackholeState = {
  PENDING: 0,
  OPEN: 1,
  BLOCKED: 2
};

/**
 * A SignalStrategy wrapper that performs DNS blackhole check.
 *
 * @param {remoting.SignalStrategy} signalStrategy
 * @constructor
 * @implements {remoting.SignalStrategy}
 */
remoting.DnsBlackholeChecker = function(signalStrategy) {
  /** @private */
  this.signalStrategy_ = signalStrategy;
  this.signalStrategy_.setStateChangedCallback(
      this.onWrappedSignalStrategyStateChanged_.bind(this));

  /** @private {?function(remoting.SignalStrategy.State):void} */
  this.onStateChangedCallback_ = null;

  /** @private */
  this.state_ = remoting.SignalStrategy.State.NOT_CONNECTED;

  /** @private */
  this.blackholeState_ = BlackholeState.PENDING;

  /** @private {?XMLHttpRequest} */
  this.xhr_ = null;
};

/**
 * @const
 * @private
 */
remoting.DnsBlackholeChecker.URL_TO_REQUEST_ =
    "https://chromoting-client.talkgadget.google.com/talkgadget/oauth/" +
    "chrome-remote-desktop-client";

/**
 * @param {?function(remoting.SignalStrategy.State):void} onStateChangedCallback
 *   Callback to call on state change.
 */
remoting.DnsBlackholeChecker.prototype.setStateChangedCallback = function(
    onStateChangedCallback) {
  this.onStateChangedCallback_ = onStateChangedCallback;
};

/**
 * @param {?function(Element):void} onIncomingStanzaCallback Callback to call on
 *     incoming messages.
 */
remoting.DnsBlackholeChecker.prototype.setIncomingStanzaCallback =
    function(onIncomingStanzaCallback) {
  this.signalStrategy_.setIncomingStanzaCallback(onIncomingStanzaCallback);
};

/** @return {remoting.SignalStrategy.Type} The signal strategy type. */
remoting.DnsBlackholeChecker.prototype.getType = function() {
  return this.signalStrategy_.getType();
};

/**
 * @param {string} server
 * @param {string} username
 * @param {string} authToken
 */
remoting.DnsBlackholeChecker.prototype.connect = function(server,
                                                          username,
                                                          authToken) {
  base.debug.assert(this.onStateChangedCallback_ != null);

  this.signalStrategy_.connect(server, username, authToken);

  this.xhr_ = remoting.xhr.start({
    method: 'GET',
    url: remoting.DnsBlackholeChecker.URL_TO_REQUEST_,
    onDone: this.onHttpRequestDone_.bind(this)
  });
};

remoting.DnsBlackholeChecker.prototype.getState = function() {
  return this.state_;
};

remoting.DnsBlackholeChecker.prototype.getError = function() {
  if (this.blackholeState_ == BlackholeState.BLOCKED) {
    return remoting.Error.NOT_AUTHORIZED;
  }

  return this.signalStrategy_.getError();
};

remoting.DnsBlackholeChecker.prototype.getJid = function() {
  base.debug.assert(this.state_ == remoting.SignalStrategy.State.CONNECTED);
  return this.signalStrategy_.getJid();
};

remoting.DnsBlackholeChecker.prototype.dispose = function() {
  if (this.xhr_) {
    this.xhr_.abort();
    this.xhr_ = null;
  }
  base.dispose(this.signalStrategy_);
  this.setState_(remoting.SignalStrategy.State.CLOSED);
};

/**
 * @param {remoting.LogToServer} logToServer The LogToServer instance for the
 *     connection.
 */
remoting.DnsBlackholeChecker.prototype.sendConnectionSetupResults = function(
    logToServer) {
  this.signalStrategy_.sendConnectionSetupResults(logToServer)
};

/** @param {string} message */
remoting.DnsBlackholeChecker.prototype.sendMessage = function(message) {
  base.debug.assert(this.state_ == remoting.SignalStrategy.State.CONNECTED);
  this.signalStrategy_.sendMessage(message);
};

/** @param {remoting.SignalStrategy.State} state */
remoting.DnsBlackholeChecker.prototype.onWrappedSignalStrategyStateChanged_ =
    function(state) {
  switch (this.blackholeState_) {
    case BlackholeState.PENDING:
      // Stay in HANDSHAKE state if we are still waiting for the HTTP request.
      if (state != remoting.SignalStrategy.State.CONNECTED) {
        this.setState_(state);
      }
      break;
    case BlackholeState.OPEN:
      this.setState_(state);
      break;
    case BlackholeState.BLOCKED:
      // In case DNS blackhole is active the external state stays FAILED.
      break;
  }
};

/**
 * @param {XMLHttpRequest} xhr
 * @private
 */
remoting.DnsBlackholeChecker.prototype.onHttpRequestDone_ = function(xhr) {
  this.xhr_ = null;
  if (xhr.status >= 200 && xhr.status <= 299) {
    console.log("DNS blackhole check succeeded.");
    this.blackholeState_ = BlackholeState.OPEN;
    if (this.signalStrategy_.getState() ==
        remoting.SignalStrategy.State.CONNECTED) {
      this.setState_(remoting.SignalStrategy.State.CONNECTED);
    }
  } else {
    console.error("DNS blackhole check failed: " + xhr.status + " " +
                  xhr.statusText + ". Response URL: " + xhr.responseURL +
                  ". Response Text: " + xhr.responseText);
    this.blackholeState_ = BlackholeState.BLOCKED;
    base.dispose(this.signalStrategy_);
    this.setState_(remoting.SignalStrategy.State.FAILED);
  }
}

/**
 * @param {remoting.SignalStrategy.State} newState
 * @private
 */
remoting.DnsBlackholeChecker.prototype.setState_ = function(newState) {
  if (this.state_ != newState) {
    this.state_ = newState;
    this.onStateChangedCallback_(this.state_);
  }
};

}());
