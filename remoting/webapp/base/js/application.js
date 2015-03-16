// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Interface abstracting the Application functionality.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {Array<string>} appCapabilities Array of application capabilities.
 * @constructor
 */
remoting.Application = function(appCapabilities) {
  /** @private {remoting.Application.Delegate} */
  this.delegate_ = null;

  /** @private {Array<string>} */
  this.appCapabilities_ = [
    remoting.ClientSession.Capability.SEND_INITIAL_RESOLUTION,
    remoting.ClientSession.Capability.RATE_LIMIT_RESIZE_REQUESTS,
    remoting.ClientSession.Capability.VIDEO_RECORDER
  ];
  // Append the app-specific capabilities.
  this.appCapabilities_.push.apply(this.appCapabilities_, appCapabilities);

  /** @private {remoting.SessionConnector} */
  this.sessionConnector_ = null;

  /** @private {base.Disposable} */
  this.sessionConnectedHooks_ = null;
};

/**
 * @param {remoting.Application.Delegate} appDelegate The delegate that
 *    contains the app-specific functionality.
 */
remoting.Application.prototype.setDelegate = function(appDelegate) {
  this.delegate_ = appDelegate;
};

/**
 * @return {string} Application product name to be used in UI.
 */
remoting.Application.prototype.getApplicationName = function() {
  return this.delegate_.getApplicationName();
};

/**
 * @param {remoting.ClientSession.Capability} capability
 * @return {boolean}
 */
remoting.Application.prototype.hasCapability = function(capability) {
  var capabilities = this.appCapabilities_;
  return capabilities.indexOf(capability) != -1;
};

/**
 * Initialize the application and register all event handlers. After this
 * is called, the app is running and waiting for user events.
 *
 * @return {void} Nothing.
 */
remoting.Application.prototype.start = function() {
  // Create global objects.
  remoting.ClientPlugin.factory = new remoting.DefaultClientPluginFactory();
  remoting.SessionConnector.factory =
      new remoting.DefaultSessionConnectorFactory();

  // TODO(garykac): This should be owned properly rather than living in the
  // global 'remoting' namespace.
  remoting.settings = new remoting.Settings();

  remoting.initGlobalObjects();
  remoting.initIdentity();

  this.delegate_.init();

  var that = this;
  remoting.identity.getToken().then(
      this.delegate_.start.bind(this.delegate_, this.getSessionConnector())
  ).catch(remoting.Error.handler(
      function(/** !remoting.Error */ error) {
        if (error.hasTag(remoting.Error.Tag.CANCELLED)) {
          that.exit();
        } else {
          that.delegate_.signInFailed(error);
        }
      }
    )
  );
};

/**
 * Quit the application.
 */
remoting.Application.prototype.exit = function() {
  this.delegate_.handleExit();
  chrome.app.window.current().close();
};

/** Disconnect the remoting client. */
remoting.Application.prototype.disconnect = function() {
  if (remoting.clientSession) {
    remoting.clientSession.disconnect(remoting.Error.none());
    console.log('Disconnected.');
  }
};

/**
 * Called when a new session has been connected.
 *
 * @param {remoting.ClientSession} clientSession
 * @return {void} Nothing.
 */
remoting.Application.prototype.onConnected = function(clientSession) {
  this.sessionConnectedHooks_ = new base.Disposables(
    new base.EventHook(
      clientSession, 'stateChanged', this.onClientStateChange_.bind(this)),
    new base.RepeatingTimer(this.updateStatistics_.bind(this), 1000)
  );
  remoting.clipboard.startSession();

  this.delegate_.handleConnected(clientSession);
};

/**
 * Called when the current session has been disconnected.
 *
 * @return {void} Nothing.
 */
remoting.Application.prototype.onDisconnected = function() {
  this.delegate_.handleDisconnected();
};

/**
 * Called when the current session's connection has failed.
 *
 * @param {!remoting.Error} error
 * @return {void} Nothing.
 */
remoting.Application.prototype.onConnectionFailed = function(error) {
  this.delegate_.handleConnectionFailed(this.sessionConnector_, error);
};

/**
 * Called when the current session has reached the point where the host has
 * started streaming video frames to the client.
 *
 * @return {void} Nothing.
 */
remoting.Application.prototype.onVideoStreamingStarted = function() {
  this.delegate_.handleVideoStreamingStarted();
};

/**
 * Called when an extension message needs to be handled.
 *
 * @param {string} type The type of the extension message.
 * @param {string} data The payload of the extension message.
 * @return {boolean} Return true if the extension message was recognized.
 */
remoting.Application.prototype.onExtensionMessage = function(type, data) {
  var message = /** @type {Object} */ (base.jsonParseSafe(data));
  if (typeof message != 'object') {
    return false;
  }

  // Give the delegate a chance to handle this extension message first.
  if (this.delegate_.handleExtensionMessage(type, message)) {
    return true;
  }

  if (remoting.desktopConnectedView) {
    return remoting.desktopConnectedView.handleExtensionMessage(type, message);
  }

  return false;
};

/**
 * Called when an error needs to be displayed to the user.
 *
 * @param {!remoting.Error} errorTag The error to be localized and displayed.
 * @return {void} Nothing.
 */
remoting.Application.prototype.onError = function(errorTag) {
  this.delegate_.handleError(errorTag);
};

/**
 * @return {remoting.SessionConnector} A session connector, creating a new one
 *     if necessary.
 */
remoting.Application.prototype.getSessionConnector = function() {
  // TODO(garykac): Check if this can be initialized in the ctor.
  if (!this.sessionConnector_) {
    this.sessionConnector_ = remoting.SessionConnector.factory.createConnector(
        document.getElementById('client-container'),
        this.onConnected.bind(this),
        this.onError.bind(this),
        this.onExtensionMessage.bind(this),
        this.onConnectionFailed.bind(this),
        this.appCapabilities_,
        this.delegate_.getDefaultRemapKeys());
  }
  return this.sessionConnector_;
};

/**
 * Callback function called when the state of the client plugin changes. The
 * current and previous states are available via the |state| member variable.
 *
 * @param {remoting.ClientSession.StateEvent=} state
 * @private
 */
remoting.Application.prototype.onClientStateChange_ = function(state) {
  switch (state.current) {
    case remoting.ClientSession.State.CLOSED:
      console.log('Connection closed by host');
      this.onDisconnected();
      break;
    case remoting.ClientSession.State.FAILED:
      var error = remoting.clientSession.getError();
      console.error('Client plugin reported connection failed: ' +
                    error.toString());
      if (error === null) {
        error = remoting.Error.unexpected();
      }
      this.onError(error);
      break;

    default:
      console.error('Unexpected client plugin state: ' + state.current);
      // This should only happen if the web-app and client plugin get out of
      // sync, so MISSING_PLUGIN is a suitable error.
      this.onError(new remoting.Error(remoting.Error.Tag.MISSING_PLUGIN));
      break;
  }

  base.dispose(this.sessionConnectedHooks_);
  this.sessionConnectedHooks_= null;
  remoting.clientSession.dispose();
  remoting.clientSession = null;
};

/** @private */
remoting.Application.prototype.updateStatistics_ = function() {
  var perfstats = remoting.clientSession.getPerfStats();
  remoting.stats.update(perfstats);
  remoting.clientSession.logStatistics(perfstats);
};


/**
 * @interface
 */
remoting.Application.Delegate = function() {};

/**
 * Initialize the application. This is called before an OAuth token is requested
 * and should be used for tasks such as initializing the DOM, registering event
 * handlers, etc.
 */
remoting.Application.Delegate.prototype.init = function() {};

/**
 * Start the application. Once start() is called, the delegate can assume that
 * the user has consented to all permissions specified in the manifest.
 *
 * @param {remoting.SessionConnector} connector
 * @param {string} token An OAuth access token. The delegate should not cache
 *     this token, but can assume that it will remain valid during application
 *     start-up.
 */
remoting.Application.Delegate.prototype.start = function(connector, token) {};

/**
 * Report an authentication error to the user. This is called in lieu of start()
 * if the user cannot be authenticated.
 *
 * @param {!remoting.Error} error The failure reason.
 */
remoting.Application.Delegate.prototype.signInFailed = function(error) {};

/**
 * @return {string} Application product name to be used in UI.
 */
remoting.Application.Delegate.prototype.getApplicationName = function() {};

/**
 * @return {string} The default remap keys for the current platform.
 */
remoting.Application.Delegate.prototype.getDefaultRemapKeys = function() {};

/**
 * Called when a new session has been connected.
 *
 * @param {remoting.ClientSession} clientSession
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.handleConnected = function(
    clientSession) {};

/**
 * Called when the current session has been disconnected.
 *
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.handleDisconnected = function() {};

/**
 * Called when the current session's connection has failed.
 *
 * @param {remoting.SessionConnector} connector
 * @param {!remoting.Error} error
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.handleConnectionFailed =
    function(connector, error) {};

/**
 * Called when the current session has reached the point where the host has
 * started streaming video frames to the client.
 *
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.handleVideoStreamingStarted =
    function() {};

/**
 * Called when an extension message needs to be handled.
 *
 * @param {string} type The type of the extension message.
 * @param {Object} message The parsed extension message data.
 * @return {boolean} Return true if the extension message was recognized.
 */
remoting.Application.Delegate.prototype.handleExtensionMessage =
    function(type, message) {};

/**
 * Called when an error needs to be displayed to the user.
 *
 * @param {!remoting.Error} errorTag The error to be localized and displayed.
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.handleError = function(errorTag) {};

/**
 * Perform any application-specific cleanup before exiting. This is called in
 * lieu of start() if the user declines the app permissions, and will usually
 * be called immediately prior to exiting, although delegates should not rely
 * on this.
 */
remoting.Application.Delegate.prototype.handleExit = function() {};


/** @type {remoting.Application} */
remoting.app = null;
