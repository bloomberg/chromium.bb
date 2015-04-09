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
remoting.clientSession;

/**
 * @param {HTMLElement} clientContainer Container element for the client view.
 * @param {function(remoting.ConnectionInfo):void} onConnected Callback on
 *     success.
 * @param {function(!remoting.Error):void} onError Callback on error.
 * @param {function(!remoting.Error):void} onConnectionFailed Callback for when
 *     the connection fails.
 * @param {Array<string>} requiredCapabilities Connector capabilities
 *     required by this application.
 * @constructor
 * @implements {remoting.SessionConnector}
 */
remoting.SessionConnectorImpl = function(clientContainer, onConnected, onError,
                                         onConnectionFailed,
                                         requiredCapabilities) {
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

  /** @private */
  this.bound_ = {
    onStateChange : this.onStateChange_.bind(this)
  };

  // Initialize/declare per-connection state.
  this.closeSession();
};

/**
 * Reset the per-connection state so that the object can be re-used for a
 * second connection. Note the none of the shared WCS state is reset.
 * @private
 */
remoting.SessionConnectorImpl.prototype.closeSession = function() {
  // It's OK to initialize these member variables here because the
  // constructor calls this method.

  /** @private {remoting.Host} */
  this.host_ = null;

  /** @private {boolean} */
  this.logHostOfflineErrors_ = false;

  base.dispose(this.clientSession_);
  /** @private {remoting.ClientSession} */
  this.clientSession_ = null;
  remoting.clientSession = null;

  base.dispose(this.plugin_);
  /** @private {remoting.ClientPlugin} */
  this.plugin_ = null;

  /** @private {remoting.CredentialsProvider} */
  this.credentialsProvider_ = null;

  base.dispose(this.signalStrategy_);
  /** @private {remoting.SignalStrategy} */
  this.signalStrategy_ = null;
};

/**
 * Initiates a connection.
 *
 * @param {remoting.Application.Mode} mode
 * @param {remoting.Host} host the Host to connect to.
 * @param {remoting.CredentialsProvider} credentialsProvider
 * @param {boolean=} opt_suppressOfflineError
 * @return {void} Nothing.
 * @private
 */
remoting.SessionConnectorImpl.prototype.connect =
    function(mode, host, credentialsProvider, opt_suppressOfflineError) {
  // In some circumstances, the WCS <iframe> can get reloaded, which results
  // in a new clientJid and a new callback. In this case, cancel any existing
  // connect operation and remove the old client plugin before instantiating a
  // new one.
  this.closeSession();
  remoting.app.setConnectionMode(mode);
  this.host_ = host;
  this.credentialsProvider_ = credentialsProvider;
  this.logHostOfflineErrors_ = !Boolean(opt_suppressOfflineError);
  this.connectSignaling_();
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
      this.plugin_, this.host_, this.signalStrategy_);
  remoting.clientSession = this.clientSession_;

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
  this.closeSession();
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

      var connectionInfo = new remoting.ConnectionInfo(
          this.host_, this.credentialsProvider_, this.clientSession_,
          this.plugin_);
      this.onConnected_(connectionInfo);
      break;

    case remoting.ClientSession.State.CONNECTING:
      remoting.identity.getEmail().then(
          function(/** string */ email) {
            console.log('Connecting as ' + email);
          });
      break;

    case remoting.ClientSession.State.AUTHENTICATED:
      console.log('Connection authenticated');
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
 * @return {remoting.SessionConnector}
 */
remoting.DefaultSessionConnectorFactory.prototype.createConnector =
    function(clientContainer, onConnected, onError,
             onConnectionFailed, requiredCapabilities) {
  return new remoting.SessionConnectorImpl(clientContainer, onConnected,
                                           onError,
                                           onConnectionFailed,
                                           requiredCapabilities);
};
