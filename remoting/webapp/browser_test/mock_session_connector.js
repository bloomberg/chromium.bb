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
 * @param {HTMLElement} clientContainer Container element for the client view.
 * @param {function(remoting.ClientSession):void} onConnected Callback on
 *     success.
 * @param {function(remoting.Error):void} onError Callback on error.
 * @param {function(string, string):boolean} onExtensionMessage The handler for
 *     protocol extension messages. Returns true if a message is recognized;
 *     false otherwise.
 * @param {function(remoting.Error):void} onConnectionFailed Callback for when
 *     the connection fails.
 * @param {Array<string>} requiredCapabilities Connector capabilities
 *     required by this application.
 * @param {string} defaultRemapKeys The default set of key mappings for the
 *     client session to use.
 * @constructor
 * @implements {remoting.SessionConnector}
 */
remoting.MockSessionConnector = function(clientContainer, onConnected, onError,
                                         onExtensionMessage,
                                         onConnectionFailed,
                                         requiredCapabilities,
                                         defaultRemapKeys) {
  this.clientContainer_ = clientContainer;
  /** @type {function(remoting.ClientSession):void} */
  this.onConnected_ = onConnected;
  this.onError = onError;
  this.onExtensionMessage_ = onExtensionMessage;
  this.onConnectionFailed_ = onConnectionFailed;
  this.requiredCapabilities_ = requiredCapabilities;
  this.defaultRemapKeys_ = defaultRemapKeys;

  /** @type {remoting.DesktopConnectedView.Mode} */
  this.mode_ = remoting.DesktopConnectedView.Mode.ME2ME;

  this.reset();
};

remoting.MockSessionConnector.prototype.reset = function() {
  /** @type {string}  @private */
  this.hostId_ = '';
};

remoting.MockSessionConnector.prototype.connectMe2Me =
    function(host, fetchPin, fetchThirdPartyToken,
             clientPairingId, clientPairedSecret) {
  this.mode_ = remoting.DesktopConnectedView.Mode.ME2ME;
  this.connect_();
};

remoting.MockSessionConnector.prototype.retryConnectMe2Me =
    function(host, fetchPin, fetchThirdPartyToken,
             clientPairingId, clientPairedSecret) {
  this.mode_ = remoting.DesktopConnectedView.Mode.ME2ME;
  this.connect_();
};

remoting.MockSessionConnector.prototype.connectMe2App =
    function(host, fetchThirdPartyToken) {
  this.mode_ = remoting.DesktopConnectedView.Mode.APP_REMOTING;
  this.connect_();
};

remoting.MockSessionConnector.prototype.updatePairingInfo =
    function(clientId, sharedSecret) {
};

remoting.MockSessionConnector.prototype.connectIT2Me =
    function(accessCode) {
  this.mode_ = remoting.DesktopConnectedView.Mode.ME2ME;
  this.connect_();
};

remoting.MockSessionConnector.prototype.reconnect = function() {
  base.debug.assert(this.mode_ == remoting.DesktopConnectedView.Mode.ME2ME);
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

remoting.MockSessionConnector.prototype.connect_ = function() {
  var signalling = new remoting.MockSignalStrategy();
  signalling.setStateForTesting(remoting.SignalStrategy.State.CONNECTED);
  var hostName = 'Mock host';
  var accessCode = '';
  var authenticationMethods = '';
  var hostId = '';
  var hostJid = '';
  var hostPublicKey = '';
  var clientPairingId = '';
  var clientPairedSecret = '';

  /**
   * @param {boolean} offerPairing
   * @param {function(string):void} callback
   */
  var fetchPin = function(offerPairing, callback) {
    window.setTimeout(function() { callback('') }, 0);
  };

  /**
   * @param {string} tokenUrl
   * @param {string} hostPublicKey
   * @param {string} scope
   * @param {function(string, string):void} callback
   */
  var fetchThirdPartyToken = function(tokenUrl, hostPublicKey, scope,
                                      callback) {
    window.setTimeout(function() { callback('', '') }, 0);
  };

  var clientSession = new remoting.ClientSession(
      signalling, this.clientContainer_, hostName,
      accessCode, fetchPin, fetchThirdPartyToken,
      authenticationMethods, hostId, hostJid, hostPublicKey,
      this.mode_, clientPairingId, clientPairedSecret);

  var that = this;
  /** @param {remoting.ClientSession.StateEvent} event */
  var onStateChange = function(event) {
    if (event.current == remoting.ClientSession.State.CONNECTED) {
      that.onConnected_(clientSession);
    }
  };

  clientSession.addEventListener(
      remoting.ClientSession.Events.stateChanged,
      onStateChange);
};


/**
 * @constructor
 * @extends {remoting.SessionConnectorFactory}
 */
remoting.MockSessionConnectorFactory = function() {};

/**
 * @param {HTMLElement} clientContainer Container element for the client view.
 * @param {function(remoting.ClientSession):void} onConnected Callback on
 *     success.
 * @param {function(remoting.Error):void} onError Callback on error.
 * @param {function(string, string):boolean} onExtensionMessage The handler for
 *     protocol extension messages. Returns true if a message is recognized;
 *     false otherwise.
 * @param {function(remoting.Error):void} onConnectionFailed Callback for when
 *     the connection fails.
 * @param {Array<string>} requiredCapabilities Connector capabilities
 *     required by this application.
 * @param {string} defaultRemapKeys The default set of key mappings to use
 *     in the client session.
 * @return {remoting.MockSessionConnector}
 */
remoting.MockSessionConnectorFactory.prototype.createConnector =
    function(clientContainer, onConnected, onError, onExtensionMessage,
             onConnectionFailed, requiredCapabilities, defaultRemapKeys) {
  return new remoting.MockSessionConnector(
      clientContainer, onConnected, onError, onExtensionMessage,
      onConnectionFailed, requiredCapabilities, defaultRemapKeys);
};
