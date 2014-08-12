// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class to communicate with the It2me Host component via Native Messaging.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 */
remoting.It2MeHostFacade = function() {
  /**
   * @type {number}
   * @private
   */
  this.nextId_ = 0;

  /**
   * @type {?chrome.runtime.Port}
   * @private
   */
  this.port_ = null;

  /**
   * @type {string}
   * @private
   */
  this.accessCode_ = '';

  /**
   * @type {number}
   * @private
   */
  this.accessCodeLifetime_ = 0;

  /**
   * @type {string}
   * @private
   */
  this.clientId_ = '';

  /**
   * @type {boolean}
   * @private
   */
  this.initialized_ = false;

  /**
   * @type {function():void}
   * @private
   */
  this.onInitialized_ = function() {};

  /**
   * Called if Native Messaging host has failed to start.
   * @private
   * */
  this.onInitializationFailed_ = function() {};

  /**
   * Called if the It2Me Native Messaging host sends a malformed message:
   * missing required attributes, attributes with incorrect types, etc.
   * @param {remoting.Error} error
   * @private
   */
  this.onError_ = function(error) {};

  /**
   * @type {function(remoting.HostSession.State):void}
   * @private
   */
  this.onStateChanged_ = function() {};

  /**
   * @type {function(boolean):void}
   * @private
   */
  this.onNatPolicyChanged_ = function() {};
};

/**
 * Sets up connection to the Native Messaging host process and exchanges
 * 'hello' messages. If Native Messaging is not supported or if the it2me
 * native messaging host is not installed, onInitializationFailed is invoked.
 * Otherwise, onInitialized is invoked.
 *
 * @param {function():void} onInitialized Called after successful
 *     initialization.
 * @param {function():void} onInitializationFailed Called if cannot connect to
 *     the native messaging host.
 * @return {void}
 */
remoting.It2MeHostFacade.prototype.initialize =
    function(onInitialized, onInitializationFailed) {
  this.onInitialized_ = onInitialized;
  this.onInitializationFailed_ = onInitializationFailed;

  try {
    this.port_ = chrome.runtime.connectNative(
        'com.google.chrome.remote_assistance');
    this.port_.onMessage.addListener(this.onIncomingMessage_.bind(this));
    this.port_.onDisconnect.addListener(this.onHostDisconnect_.bind(this));
    this.port_.postMessage({type: 'hello'});
  } catch (err) {
    console.log('Native Messaging initialization failed: ',
                /** @type {*} */ (err));
    onInitializationFailed();
    return;
  }
};

/**
 * @param {string} email The user's email address.
 * @param {string} authServiceWithToken Concatenation of the auth service
 *     (e.g. oauth2) and the access token.
 * @param {function(remoting.HostSession.State):void} onStateChanged Callback to
 *     invoke when the host state changes.
 * @param {function(boolean):void} onNatPolicyChanged Callback to invoke when
 *     the nat traversal policy changes.
 * @param {function(string):void} logDebugInfo Callback allowing the plugin
 *     to log messages to the debug log.
 * @param {string} xmppServerAddress XMPP server host name (or IP address) and
 *     port.
 * @param {boolean} xmppServerUseTls Whether to use TLS on connections to the
 *     XMPP server
 * @param {string} directoryBotJid XMPP JID for the remoting directory server
 *     bot.
 * @param {function(remoting.Error):void} onError Callback to invoke in case of
 *     an error.
 * @return {void}
 */
remoting.It2MeHostFacade.prototype.connect =
    function(email, authServiceWithToken, onStateChanged, onNatPolicyChanged,
             logDebugInfo, xmppServerAddress, xmppServerUseTls, directoryBotJid,
             onError) {
  if (!this.port_) {
    console.error(
        'remoting.It2MeHostFacade.connect() without initialization.');
    onError(remoting.Error.UNEXPECTED);
    return;
  }

  this.onStateChanged_ = onStateChanged;
  this.onNatPolicyChanged_ = onNatPolicyChanged;
  this.onErrorHandler_ = onError;
  this.port_.postMessage({
    type: 'connect',
    userName: email,
    authServiceWithToken: authServiceWithToken,
    xmppServerAddress: xmppServerAddress,
    xmppServerUseTls: xmppServerUseTls,
    directoryBotJid: directoryBotJid
  });
};

/**
 * @return {void}
 */
remoting.It2MeHostFacade.prototype.disconnect = function() {
  if (this.port_)
    this.port_.postMessage({type: 'disconnect'});
};

/**
 * @return {string}
 */
remoting.It2MeHostFacade.prototype.getAccessCode = function() {
  return this.accessCode_
};

/**
 * @return {number}
 */
remoting.It2MeHostFacade.prototype.getAccessCodeLifetime = function() {
  return this.accessCodeLifetime_
};

/**
 * @return {string}
 */
remoting.It2MeHostFacade.prototype.getClient = function() {
  return this.clientId_;
};

/**
 * Handler for incoming messages.
 *
 * @param {Object} message The received message.
 * @return {void}
 * @private
 */
remoting.It2MeHostFacade.prototype.onIncomingMessage_ =
    function(message) {
  var type = getStringAttr(message, 'type');

  switch (type) {
    case 'helloResponse':
      var version = getStringAttr(message, 'version');
      console.log('Host version: ', version);
      this.initialized_ = true;
      // A "hello" request is sent immediately after the native messaging host
      // is started. We can proceed to the next task once we receive the
      // "helloReponse".
      this.onInitialized_();
      break;

    case 'connectResponse':
      console.log('connectResponse received');
      // Response to the "connect" request. No action is needed until we
      // receive the corresponding "hostStateChanged" message.
      break;

    case 'disconnectResponse':
      console.log('disconnectResponse received');
      // Response to the "disconnect" request. No action is needed until we
      // receive the corresponding "hostStateChanged" message.
      break;

    case 'hostStateChanged':
      var stateString = getStringAttr(message, 'state');
      console.log('hostStateChanged received: ', stateString);
      var state = remoting.HostSession.State.fromString(stateString);

      switch (state) {
        case remoting.HostSession.State.RECEIVED_ACCESS_CODE:
          var accessCode = getStringAttr(message, 'accessCode');
          var accessCodeLifetime = getNumberAttr(message, 'accessCodeLifetime');
          this.onReceivedAccessCode_(accessCode, accessCodeLifetime);
          break;

        case remoting.HostSession.State.CONNECTED:
          var client = getStringAttr(message, 'client');
          this.onConnected_(client);
          break;
      }
      this.onStateChanged_(state);
      break;

    case 'natPolicyChanged':
      var natTraversalEnabled = getBooleanAttr(message, 'natTraversalEnabled');
      this.onNatPolicyChanged_(natTraversalEnabled);
      break;

    case 'error':
      console.error(getStringAttr(message, 'description'));
      this.onError_(remoting.Error.UNEXPECTED);
      break;

    default:
      throw 'Unexpected native message: ' + message;
  }
};

/**
 * @param {string} accessCode
 * @param {number} accessCodeLifetime
 * @return {void}
 * @private
 */
remoting.It2MeHostFacade.prototype.onReceivedAccessCode_ =
    function(accessCode, accessCodeLifetime) {
  this.accessCode_ = accessCode;
  this.accessCodeLifetime_ = accessCodeLifetime;
};

/**
 * @param {string} clientId
 * @return {void}
 * @private
 */
remoting.It2MeHostFacade.prototype.onConnected_ = function(clientId) {
  this.clientId_ = clientId;
};

/**
 * @return {void}
 * @private
 */
remoting.It2MeHostFacade.prototype.onHostDisconnect_ = function() {
  if (!this.initialized_) {
    // If the host is disconnected before it is initialized, it probably means
    // the host is not propertly installed (or not installed at all).
    // E.g., if the host manifest is not present we get "Specified native
    // messaging host not found" error. If the host manifest is present but
    // the host binary cannot be found we get the "Native host has exited"
    // error.
    console.log('Native Messaging initialization failed: ' +
                chrome.runtime.lastError.message);
    this.onInitializationFailed_();
  } else {
    console.error('Native Messaging port disconnected.');
    this.port_ = null;
    this.onError_(remoting.Error.UNEXPECTED);
  }
}
