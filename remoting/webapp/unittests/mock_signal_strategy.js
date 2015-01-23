// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

remoting.MockSignalStrategy = function(jid, type) {
  this.jid_ = (jid != undefined) ? jid : "jid@example.com";
  this.type_ = (type != undefined) ? type : remoting.SignalStrategy.Type.XMPP;
  this.onStateChangedCallback_ = null;
  this.state_ = null;
  this.onIncomingStanzaCallback_ = function() {};
  this.dispose = sinon.spy();
  this.connect = sinon.spy();
  this.sendMessage = sinon.spy();
  this.sendConnectionSetupResults = sinon.spy();
};

remoting.MockSignalStrategy.prototype.setStateChangedCallback = function(
    onStateChangedCallback) {
  this.onStateChangedCallback_ = onStateChangedCallback;
};

remoting.MockSignalStrategy.prototype.setIncomingStanzaCallback =
    function(onIncomingStanzaCallback) {
  this.onIncomingStanzaCallback_ =
      onIncomingStanzaCallback ? onIncomingStanzaCallback
                               : function() {};
};

remoting.MockSignalStrategy.prototype.getState = function(message) {
  return this.state_;
};

remoting.MockSignalStrategy.prototype.getError = function(message) {
  return remoting.Error.UNKNOWN;
};

remoting.MockSignalStrategy.prototype.getJid = function(message) {
  return this.jid_;
};

remoting.MockSignalStrategy.prototype.getType = function(message) {
  return this.type_;
};

remoting.MockSignalStrategy.prototype.setStateForTesting = function(state) {
  this.state_ = state;
  this.onStateChangedCallback_(state);
};

remoting.MockSignalStrategy.prototype.dispose = function() {
  this.state_ = remoting.SignalStrategy.State.CLOSED;
  this.onStateChangedCallback_(this.state_);
};
