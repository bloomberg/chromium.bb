// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Connect set-up state machine for Me2Me and IT2Me
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @type {remoting.ClientSession} The client session object, set once the
 *     connector has invoked its onOk callback.
 * TODO(garykac): Have this owned by someone instead of being global.
 */
remoting.clientSession = null;

/**
 * @type {remoting.DesktopConnectedView} The client session view object, set
 *     once the connector has invoked its onOk callback.
 * TODO(garykac): Have this owned by someone instead of being global.
 */
remoting.desktopConnectedView = null;

/**
 * @param {HTMLElement} clientContainer Container element for the client view.
 * @param {function(remoting.ConnectionInfo):void} onConnected Callback on
 *     success.
 * @param {function(!remoting.Error):void} onError Callback on error.
 * @param {function(!remoting.Error):void} onConnectionFailed Callback for when
 *     the connection fails.
 * @param {Array<string>} requiredCapabilities Connector capabilities
 *     required by this application.
 * @param {string} defaultRemapKeys The default set of key mappings for the
 *     client session to use.
 * @constructor
 * @implements {remoting.SessionConnector}
 */
remoting.SessionConnectorImpl = function(clientContainer, onConnected, onError,
                                         onConnectionFailed,
                                         requiredCapabilities,
                                         defaultRemapKeys) {
  /** @private {HTMLElement} */
  this.clientContainer_ = clientContainer;

  /** @private {function(remoting.ConnectionInfo):void} */
  this.onConnected_ = onConnected;

  /** @private {function(!remoting.Error):void} */
  this.onError_ = onError;

  /** @private {function(!remoting.Error):void} */
  this.onConnectionFailed_ = onConnectionFailed;

  /** @private {Array<string>} */
  this.requiredCapabilities_ = requiredCapabilities;

  /** @private {string} */
  this.defaultRemapKeys_ = defaultRemapKeys;

  /** @private {remoting.DesktopConnectedView.Mode} */
  this.connectionMode_ = remoting.DesktopConnectedView.Mode.ME2ME;

  /** @private {remoting.SignalStrategy} */
  this.signalStrategy_ = null;

  /** @private {remoting.SmartReconnector} */
  this.reconnector_ = null;

  /** @private */
  this.bound_ = {
    onStateChange : this.onStateChange_.bind(this)
  };

  // Initialize/declare per-connection state.
  this.resetConnection_();
};

/**
 * Reset the per-connection state so that the object can be re-used for a
 * second connection. Note the none of the shared WCS state is reset.
 * @private
 */
remoting.SessionConnectorImpl.prototype.resetConnection_ = function() {
  this.removePlugin_();

  /** @private {remoting.Host} */
  this.host_ = null;

  /** @private {boolean} */
  this.logHostOfflineErrors_ = false;

  /** @private {remoting.ClientPlugin} */
  this.plugin_ = null;

  /** @private {remoting.ClientSession} */
  this.clientSession_ = null;

  /** @private {remoting.DesktopConnectedView} */
  this.connectedView_ = null;

  /** @private {XMLHttpRequest} */
  this.pendingXhr_ = null;

  /** @private {remoting.CredentialsProvider} */
  this.credentialsProvider_ = null;

  /** @private {Object<string,remoting.ProtocolExtension>} */
  this.protocolExtensions_ = {};

  /**
   * True once a session has been created and we've started the extensions.
   * This is used to immediately start any extensions that are registered
   * after the CONNECTED state change.
   * @private {boolean}
   */
  this.protocolExtensionsStarted_ = false;
};

/**
 * Initiate a Me2Me connection.
 *
 * This doesn't report host-offline errors because the connection will
 * be retried and retryConnectMe2Me is responsible for reporting these errors.
 *
 * @param {remoting.Host} host The Me2Me host to which to connect.
 * @param {function(boolean, function(string):void):void} fetchPin Function to
 *     interactively obtain the PIN from the user.
 * @param {string} clientPairingId The client id issued by the host when
 *     this device was paired, if it is already paired.
 * @param {string} clientPairedSecret The shared secret issued by the host when
 *     this device was paired, if it is already paired.
 * @return {void} Nothing.
 */
remoting.SessionConnectorImpl.prototype.connectMe2Me =
    function(host, fetchPin, fetchThirdPartyToken,
             clientPairingId, clientPairedSecret) {
  this.logHostOfflineErrors_ = false;
  var credentialsProvider = new remoting.CredentialsProvider({
    fetchPin: fetchPin,
    pairingInfo: {clientId: clientPairingId, sharedSecret: clientPairedSecret},
    fetchThirdPartyToken: fetchThirdPartyToken
  });
  this.connect(
      remoting.DesktopConnectedView.Mode.ME2ME, host, credentialsProvider);
};

/**
 * Retry connecting to a Me2Me host after a connection failure.
 *
 * This is the same as connectMe2Me except that is will log errors if the
 * host is offline.
 *
 * @param {remoting.Host} host The Me2Me host to refresh.
 * @return {void} Nothing.
 */
remoting.SessionConnectorImpl.prototype.retryConnectMe2Me = function(host) {
  this.logHostOfflineErrors_ = true;
  this.connect(remoting.DesktopConnectedView.Mode.ME2ME, host,
               this.credentialsProvider_);
};

/**
 * Initiate a Me2App connection.
 *
 * @param {remoting.Host} host The Me2Me host to which to connect.
 * @param {function(string, string, string,
 *                  function(string, string): void): void}
 *     fetchThirdPartyToken Function to obtain a token from a third party
 *     authenticaiton server.
 * @return {void} Nothing.
 */
remoting.SessionConnectorImpl.prototype.connectMe2App =
    function(host, fetchThirdPartyToken) {
  this.connectionMode_ = remoting.DesktopConnectedView.Mode.APP_REMOTING;
  this.logHostOfflineErrors_ = true;
  var credentialsProvider = new remoting.CredentialsProvider({
    fetchThirdPartyToken : fetchThirdPartyToken
  });
  this.connect(
      remoting.DesktopConnectedView.Mode.APP_REMOTING, host,
      credentialsProvider);
};

/**
 * Update the pairing info so that the reconnect function will work correctly.
 *
 * @param {string} clientId The paired client id.
 * @param {string} sharedSecret The shared secret.
 */
remoting.SessionConnectorImpl.prototype.updatePairingInfo =
    function(clientId, sharedSecret) {
  var pairingInfo = this.credentialsProvider_.getPairingInfo();
  pairingInfo.clientId = clientId;
  pairingInfo.sharedSecret = sharedSecret;
};

/**
 * Initiates a connection.
 *
 * @param {remoting.DesktopConnectedView.Mode} mode
 * @param {remoting.Host} host the Host to connect to.
 * @param {remoting.CredentialsProvider} credentialsProvider
 * @return {void} Nothing.
 * @private
 */
remoting.SessionConnectorImpl.prototype.connect =
    function(mode, host, credentialsProvider) {
  // Cancel any existing connect operation.
  this.cancel();
  this.connectionMode_ = mode;
  this.host_ = host;
  this.credentialsProvider_ = credentialsProvider;
  this.connectSignaling_();
};

/**
 * Reconnect a closed connection.
 *
 * @return {void} Nothing.
 */
remoting.SessionConnectorImpl.prototype.reconnect = function() {
  if (this.connectionMode_ == remoting.DesktopConnectedView.Mode.IT2ME) {
    console.error('reconnect not supported for IT2Me.');
    return;
  }
  this.logHostOfflineErrors_ = false;
  this.connect(this.connectionMode_, this.host_, this.credentialsProvider_);
};

/**
 * Cancel a connection-in-progress.
 */
remoting.SessionConnectorImpl.prototype.cancel = function() {
  this.resetConnection_();
};

/**
 * Get the connection mode (Me2Me or IT2Me)
 *
 * @return {remoting.DesktopConnectedView.Mode}
 */
remoting.SessionConnectorImpl.prototype.getConnectionMode = function() {
  return this.connectionMode_;
};

/**
 * Get host ID.
 *
 * @return {string}
 */
remoting.SessionConnectorImpl.prototype.getHostId = function() {
  return this.host_.hostId;
};

/**
 * @private
 */
remoting.SessionConnectorImpl.prototype.connectSignaling_ = function() {
  base.dispose(this.signalStrategy_);
  this.signalStrategy_ = null;

  /** @type {remoting.SessionConnectorImpl} */
  var that = this;

  /** @param {string} token */
  function connectSignalingWithToken(token) {
    remoting.identity.getUserInfo().then(
        connectSignalingWithTokenAndUserInfo.bind(null, token),
        remoting.Error.handler(that.onError_));
  }

  /**
   * Success callback for when the email and fullName have been retrieved
   * for this user.
   * Note that the full name will be null unless the webapp has requested
   * and been granted the userinfo.profile permission.
   *
   * @param {string} token
   * @param {{email: string, name: string}} userInfo
   */
  function connectSignalingWithTokenAndUserInfo(token, userInfo) {
    that.signalStrategy_.connect(remoting.settings.XMPP_SERVER, userInfo.email,
                                 token);
  }

  this.signalStrategy_ = remoting.SignalStrategy.create();
  this.signalStrategy_.setStateChangedCallback(
      this.onSignalingState_.bind(this));

  remoting.identity.getToken().then(
      connectSignalingWithToken,
      remoting.Error.handler(this.onError_));
};

/**
 * @private
 * @param {remoting.SignalStrategy.State} state
 */
remoting.SessionConnectorImpl.prototype.onSignalingState_ = function(state) {
  switch (state) {
    case remoting.SignalStrategy.State.CONNECTED:
      // Proceed only if the connection hasn't been canceled.
      if (this.host_.jabberId) {
        this.createSession_();
      }
      break;

    case remoting.SignalStrategy.State.FAILED:
      this.onError_(this.signalStrategy_.getError());
      break;
  }
};

/**
 * Creates ClientSession object.
 */
remoting.SessionConnectorImpl.prototype.createSession_ = function() {
  // In some circumstances, the WCS <iframe> can get reloaded, which results
  // in a new clientJid and a new callback. In this case, remove the old
  // client plugin before instantiating a new one.
  this.removePlugin_();

  var pluginContainer = this.clientContainer_.querySelector(
      '.client-plugin-container');

  this.plugin_ = remoting.ClientPlugin.factory.createPlugin(
      pluginContainer, this.requiredCapabilities_);

  var that = this;
  this.host_.options.load().then(function(){
    that.plugin_.initialize(that.onPluginInitialized_.bind(that));
  });
};

/**
 * @param {boolean} initialized
 * @private
 */
remoting.SessionConnectorImpl.prototype.onPluginInitialized_ = function(
    initialized) {
  if (!initialized) {
    console.error('ERROR: remoting plugin not loaded');
    this.pluginError_(new remoting.Error(remoting.Error.Tag.MISSING_PLUGIN));
    return;
  }

  if (!this.plugin_.isSupportedVersion()) {
    console.error('ERROR: bad plugin version');
    this.pluginError_(new remoting.Error(
        remoting.Error.Tag.BAD_PLUGIN_VERSION));
    return;
  }

  this.clientSession_ = new remoting.ClientSession(
      this.plugin_, this.host_, this.signalStrategy_, this.connectionMode_,
      this.onProtocolExtensionMessage_.bind(this));
  remoting.clientSession = this.clientSession_;

  this.connectedView_ = new remoting.DesktopConnectedView(
      this.plugin_, this.clientContainer_, this.host_,
      this.connectionMode_, this.defaultRemapKeys_);
  remoting.desktopConnectedView = this.connectedView_;

  this.clientSession_.logHostOfflineErrors(this.logHostOfflineErrors_);
  this.clientSession_.addEventListener(
      remoting.ClientSession.Events.stateChanged,
      this.bound_.onStateChange);

  this.plugin_.connect(
      this.host_, this.signalStrategy_.getJid(), this.credentialsProvider_);
};

/**
 * @param {!remoting.Error} error
 * @private
 */
remoting.SessionConnectorImpl.prototype.pluginError_ = function(error) {
  this.signalStrategy_.setIncomingStanzaCallback(null);
  this.clientSession_.disconnect(error);
  this.removePlugin_();
};

/** @private */
remoting.SessionConnectorImpl.prototype.removePlugin_ = function() {
  if (this.clientSession_) {
    this.clientSession_.removePlugin();
  }
  this.clientSession_ = null;
  remoting.clientSession = null;

  base.dispose(this.connectedView_);
  this.connectedView_ = null;
  remoting.desktopConnectedView = null;

  base.dispose(this.plugin_);
  this.plugin_ = null;
};

/**
 * @param {remoting.ProtocolExtension} extension
 */
remoting.SessionConnectorImpl.prototype.registerProtocolExtension =
    function(extension) {
  var types = extension.getExtensionTypes();

  // Make sure we don't have an extension of that type already registered.
  for (var i=0, len=types.length; i < len; i++) {
    if (types[i] in this.protocolExtensions_) {
      console.error(
          'Attempt to register multiple extensions of the same type: ', type);
      return;
    }
  }

  for (var i=0, len=types.length; i < len; i++) {
    var type = types[i];
    this.protocolExtensions_[type] = extension;
    if (this.protocolExtensionsStarted_) {
      this.startProtocolExtension_(type);
    }
  }
};

/** @private */
remoting.SessionConnectorImpl.prototype.initProtocolExtensions_ = function() {
  base.debug.assert(!this.protocolExtensionsStarted_);
  for (var type in this.protocolExtensions_) {
    this.startProtocolExtension_(type);
  }
  this.protocolExtensionsStarted_ = true;
};

/**
 * @param {string} type
 * @private
 */
remoting.SessionConnectorImpl.prototype.startProtocolExtension_ =
    function(type) {
  var extension = this.protocolExtensions_[type];
  extension.startExtension(this.plugin_.sendClientMessage.bind(this.plugin_));
};

/**
 * Called when an extension message needs to be handled.
 *
 * @param {string} type The type of the extension message.
 * @param {string} data The payload of the extension message.
 * @return {boolean} Return true if the extension message was recognized.
 * @private
 */
remoting.SessionConnectorImpl.prototype.onProtocolExtensionMessage_ =
    function(type, data) {
  if (type == 'test-echo-reply') {
    console.log('Got echo reply: ' + data);
    return true;
  }

  var message = base.jsonParseSafe(data);
  if (typeof message != 'object') {
    console.error('Error parsing extension json data: ' + data);
    return false;
  }

  if (type in this.protocolExtensions_) {
    /** @type {remoting.ProtocolExtension} */
    var extension = this.protocolExtensions_[type];
    var handled = false;
    try {
      handled = extension.onExtensionMessage(type, message);
    } catch (/** @type {*} */ err) {
      console.error('Failed to process protocol extension ' + type +
                    ' message: ' + err);
    }
    if (handled) {
      return true;
    }
  }

  if (remoting.desktopConnectedView) {
    return remoting.desktopConnectedView.handleExtensionMessage(type, message);
  }

  return false;
};

/**
 * Handle a change in the state of the client session prior to successful
 * connection (after connection, this class no longer handles state change
 * events). Errors that occur while connecting either trigger a reconnect
 * or notify the onError handler.
 *
 * @param {remoting.ClientSession.StateEvent=} event
 * @return {void} Nothing.
 * @private
 */
remoting.SessionConnectorImpl.prototype.onStateChange_ = function(event) {
  switch (event.current) {
    case remoting.ClientSession.State.CONNECTED:
      // When the connection succeeds, deregister for state-change callbacks
      // and pass the session to the onConnected callback. It is expected that
      // it will register a new state-change callback to handle disconnect
      // or error conditions.
      this.clientSession_.removeEventListener(
          remoting.ClientSession.Events.stateChanged,
          this.bound_.onStateChange);

      base.dispose(this.reconnector_);
      if (this.connectionMode_ != remoting.DesktopConnectedView.Mode.IT2ME) {
        this.reconnector_ =
            new remoting.SmartReconnector(this, this.clientSession_);
      }
      var connectionInfo = new remoting.ConnectionInfo(
          this.host_, this.credentialsProvider_, this.clientSession_,
          this.plugin_, this.connectionMode_);
      this.onConnected_(connectionInfo);
      // Initialize any protocol extensions that may have been added by the app.
      this.initProtocolExtensions_();
      break;

    case remoting.ClientSession.State.CREATED:
      console.log('Created plugin');
      break;

    case remoting.ClientSession.State.CONNECTING:
      remoting.identity.getEmail().then(
          function(/** string */ email) {
            console.log('Connecting as ' + email);
          });
      break;

    case remoting.ClientSession.State.INITIALIZING:
      console.log('Initializing connection');
      break;

    case remoting.ClientSession.State.CLOSED:
      // This class deregisters for state-change callbacks when the CONNECTED
      // state is reached, so it only sees the CLOSED state in exceptional
      // circumstances. For example, a CONNECTING -> CLOSED transition happens
      // if the host closes the connection without an error message instead of
      // accepting it. Since there's no way of knowing exactly what went wrong,
      // we rely on server-side logs in this case and report a generic error
      // message.
      this.onError_(remoting.Error.unexpected());
      break;

    case remoting.ClientSession.State.FAILED:
      var error = this.clientSession_.getError();
      console.error('Client plugin reported connection failed: ' +
                    error.toString());
      if (error == null) {
        error = remoting.Error.unexpected();
      }
      this.onConnectionFailed_(error);
      break;

    default:
      console.error('Unexpected client plugin state: ' + event.current);
      // This should only happen if the web-app and client plugin get out of
      // sync, and even then the version check should ensure compatibility.
      this.onError_(new remoting.Error(remoting.Error.Tag.MISSING_PLUGIN));
  }
};

/**
 * @constructor
 * @implements {remoting.SessionConnectorFactory}
 */
remoting.DefaultSessionConnectorFactory = function() {};

/**
 * @param {HTMLElement} clientContainer Container element for the client view.
 * @param {function(remoting.ConnectionInfo):void} onConnected Callback on
 *     success.
 * @param {function(!remoting.Error):void} onError Callback on error.
 * @param {function(!remoting.Error):void} onConnectionFailed Callback for when
 *     the connection fails.
 * @param {Array<string>} requiredCapabilities Connector capabilities
 *     required by this application.
 * @param {string} defaultRemapKeys The default set of key mappings to use
 *     in the client session.
 * @return {remoting.SessionConnector}
 */
remoting.DefaultSessionConnectorFactory.prototype.createConnector =
    function(clientContainer, onConnected, onError,
             onConnectionFailed, requiredCapabilities, defaultRemapKeys) {
  return new remoting.SessionConnectorImpl(clientContainer, onConnected,
                                           onError,
                                           onConnectionFailed,
                                           requiredCapabilities,
                                           defaultRemapKeys);
};
