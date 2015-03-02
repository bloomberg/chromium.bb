// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};


/**
 * @param {string=} jid
 * @param {remoting.SignalStrategy.Type=} type
 *
 * @implements {remoting.SignalStrategy}
 * @constructor
 */
remoting.MockSignalStrategy = function(jid, type) {
  this.jid_ = (jid != undefined) ? jid : "jid@example.com";
  this.type_ = (type != undefined) ? type : remoting.SignalStrategy.Type.XMPP;
  this.onStateChangedCallback_ = null;

  /** @type {remoting.SignalStrategy.State} */
  this.state_ = remoting.SignalStrategy.State.NOT_CONNECTED;

  this.onIncomingStanzaCallback_ = function() {};
  this.dispose = sinon.spy();
  this.connect = sinon.spy();
  this.sendMessage = sinon.spy();
  this.sendConnectionSetupResults = sinon.spy();
};

/**
 * @param {function(remoting.SignalStrategy.State):void} onStateChangedCallback
 *   Callback to call on state change.
 */
remoting.MockSignalStrategy.prototype.setStateChangedCallback = function(
    onStateChangedCallback) {
  this.onStateChangedCallback_ = onStateChangedCallback;
};

/**
 * @param {?function(Element):void} onIncomingStanzaCallback Callback to call on
 *     incoming messages.
 */
remoting.MockSignalStrategy.prototype.setIncomingStanzaCallback =
    function(onIncomingStanzaCallback) {
  this.onIncomingStanzaCallback_ =
      onIncomingStanzaCallback ? onIncomingStanzaCallback
                               : function() {};
};

/** @return {remoting.SignalStrategy.State} */
remoting.MockSignalStrategy.prototype.getState = function() {
  return this.state_;
};

/** @return {remoting.Error} */
remoting.MockSignalStrategy.prototype.getError = function() {
  return remoting.Error.NONE;
};

/** @return {string} */
remoting.MockSignalStrategy.prototype.getJid = function() {
  return this.jid_;
};

/** @return {remoting.SignalStrategy.Type} */
remoting.MockSignalStrategy.prototype.getType = function() {
  return this.type_;
};

/**
 * @param {remoting.SignalStrategy.State} state
 */
remoting.MockSignalStrategy.prototype.setStateForTesting = function(state) {
  this.state_ = state;
  this.onStateChangedCallback_(state);
};
