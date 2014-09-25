// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Mock implementation of SessionConnector for testing.
 * @suppress {checkTypes}
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @implements {remoting.SessionConnector}
 */
remoting.MockSessionConnector = function(clientContainer, onConnected, onError,
                                         onExtensionMessage) {
  this.clientContainer_ = clientContainer;
  this.onConnected_ = onConnected;
  this.onError = onError;
  this.onExtensionMessage_ = onExtensionMessage;
  this.reset();
};

remoting.MockSessionConnector.prototype.reset = function() {
  this.pairingRequested_ = false;
};

remoting.MockSessionConnector.prototype.connectMe2Me =
    function(host, fetchPin, fetchThirdPartyToken,
             clientPairingId, clientPairedSecret) {
  this.mode_ = remoting.ClientSession.Mode.ME2ME;
  this.connect_();
};

remoting.MockSessionConnector.prototype.updatePairingInfo =
    function(clientId, sharedSecret) {
};

remoting.MockSessionConnector.prototype.connectIT2Me =
    function(accessCode) {
  this.mode_ = remoting.ClientSession.Mode.ME2ME;
  this.connect_();
};

remoting.MockSessionConnector.prototype.reconnect = function() {
  base.debug.assert(this.mode_ == remoting.ClientSession.Mode.ME2ME);
  this.connect_();
};

remoting.MockSessionConnector.prototype.cancel = function() {
};

remoting.MockSessionConnector.prototype.getConnectionMode = function() {
  return this.mode_;
};

remoting.MockSessionConnector.prototype.getHostId = function() {
  return this.hostId_;
};

remoting.MockSessionConnector.prototype.requestPairing = function() {
  this.pairingRequested_ = true;
};

remoting.MockSessionConnector.prototype.pairingRequested = function() {
  return this.pairingRequested_;
};

remoting.MockSessionConnector.prototype.connect_ = function() {
  var signalling = new remoting.MockSignalStrategy();
  var hostName = 'Mock host';
  var accessCode = '';
  var authenticationMethods = '';
  var hostId = '';
  var hostJid = '';
  var hostPublicKey = '';
  var clientPairingId = '';
  var clientPairedSecret = '';
  var fetchPin = function(offerPairing, callback) {
    window.setTimeout(function() { callback('') }, 0);
  };
  var fetchThirdPartyToken = function(tokenUrl, scope, callback) {
    window.setTimeout(function() { callback('', '') }, 0);
  };

  var clientSession = new remoting.ClientSession(
      signalling, this.clientContainer_, hostName,
      accessCode, fetchPin, fetchThirdPartyToken,
      authenticationMethods, hostId, hostJid, hostPublicKey,
      this.mode_, clientPairingId, clientPairedSecret);

  var onStateChange = function(event) {
    if (event.current == remoting.ClientSession.State.CONNECTED) {
      this.onConnected_(clientSession);
    }
  }.bind(this);

  clientSession.addEventListener(
      remoting.ClientSession.Events.stateChanged,
      onStateChange);
  clientSession.createPluginAndConnect(this.onExtensionMessage_);
};


/**
 * @constructor
 * @extends {remoting.SessionConnectorFactory}
 */
remoting.MockSessionConnectorFactory = function() {};

remoting.MockSessionConnectorFactory.prototype.createConnector =
    function(clientContainer, onConnected, onError, onExtensionMessage) {
  return new remoting.MockSessionConnector(
      clientContainer, onConnected, onError, onExtensionMessage);
};