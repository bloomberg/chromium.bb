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
 * @param {HTMLElement} clientContainer Container element for the client view.
 * @param {Array<string>} requiredCapabilities Connector capabilities
 *     required by this application.
 * @param {remoting.ClientSession.EventHandler} handler
 * @constructor
 * @implements {remoting.SessionConnector}
 */
remoting.SessionConnectorImpl =
    function(clientContainer, requiredCapabilities, handler) {
  /** @private {HTMLElement} */
  this.clientContainer_ = clientContainer;

  /** @private */
  this.onError_ = handler.onError.bind(handler);

  /** @private */
  this.handler_ = handler;

  /** @private {Array<string>} */
  this.requiredCapabilities_ = [
    remoting.ClientSession.Capability.SEND_INITIAL_RESOLUTION,
    remoting.ClientSession.Capability.RATE_LIMIT_RESIZE_REQUESTS,
    remoting.ClientSession.Capability.VIDEO_RECORDER
  ];

  // Append the app-specific capabilities.
  this.requiredCapabilities_.push.apply(this.requiredCapabilities_,
                                        requiredCapabilities);

  // Initialize/declare per-connection state.
  this.closeSession_();
};

/**
 * Reset the per-connection state so that the object can be re-used for a
 * second connection. Note the none of the shared WCS state is reset.
 * @private
 */
remoting.SessionConnectorImpl.prototype.closeSession_ = function() {
  // It's OK to initialize these member variables here because the
  // constructor calls this method.

  base.dispose(this.eventHook_);
  /** @private {base.Disposable} */
  this.eventHook_ = null;

  /** @private {remoting.Host} */
  this.host_ = null;

  /** @private {boolean} */
  this.logHostOfflineErrors_ = false;

  base.dispose(this.clientSession_);
  /** @private {remoting.ClientSession} */
  this.clientSession_ = null;

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
  this.closeSession_();
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

  this.clientSession_.logHostOfflineErrors(this.logHostOfflineErrors_);
  this.eventHook_ = new base.EventHook(
      this.clientSession_, 'stateChanged', this.onStateChange_.bind(this));
  this.plugin_.connect(
      this.host_, this.signalStrategy_.getJid(), this.credentialsProvider_);
};

/**
 * @param {!remoting.Error} error
 * @private
 */
remoting.SessionConnectorImpl.prototype.pluginError_ = function(error) {
  this.signalStrategy_.setIncomingStanzaCallback(null);
  this.closeSession_();
};

/**
 * Handle a change in the state of the client session.
 *
 * @param {remoting.ClientSession.StateEvent=} event
 * @return {void} Nothing.
 * @private
 */
remoting.SessionConnectorImpl.prototype.onStateChange_ = function(event) {
  switch (event.current) {
    case remoting.ClientSession.State.CONNECTED:
      var connectionInfo = new remoting.ConnectionInfo(
          this.host_, this.credentialsProvider_, this.clientSession_,
          this.plugin_);
      this.handler_.onConnected(connectionInfo);
      break;

    case remoting.ClientSession.State.CONNECTING:
      remoting.identity.getEmail().then(function(/** string */ email) {
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
      this.handler_.onDisconnected();
      break;

    case remoting.ClientSession.State.FAILED:
      var error = this.clientSession_.getError();
      console.error('Client plugin reported connection failed: ' +
                    error.toString());
      this.handler_.onConnectionFailed(error || remoting.Error.unexpected());
      break;

    default:
      console.error('Unexpected client plugin state: ' + event.current);
      // This should only happen if the web-app and client plugin get out of
      // sync, and even then the version check should ensure compatibility.
      this.onError_(new remoting.Error(remoting.Error.Tag.MISSING_PLUGIN));
  }

  if (this.clientSession_.isFinished()) {
    this.closeSession_();
  }
};

/**
 * @constructor
 * @implements {remoting.SessionConnectorFactory}
 */
remoting.DefaultSessionConnectorFactory = function() {};

/**
 * @param {HTMLElement} clientContainer Container element for the client view.
 * @param {Array<string>} requiredCapabilities Connector capabilities
 *     required by this application.
 * @param {remoting.ClientSession.EventHandler} handler
 * @return {remoting.SessionConnector}
 */
remoting.DefaultSessionConnectorFactory.prototype.createConnector =
    function(clientContainer, requiredCapabilities, handler) {
  return new remoting.SessionConnectorImpl(clientContainer,
                                           requiredCapabilities, handler);
};
