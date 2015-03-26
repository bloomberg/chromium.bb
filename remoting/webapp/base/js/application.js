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
 * @implements {remoting.ApplicationInterface}
 */
remoting.Application = function(appCapabilities) {
  // Create global factories.
  remoting.ClientPlugin.factory = new remoting.DefaultClientPluginFactory();
  remoting.SessionConnector.factory =
      new remoting.DefaultSessionConnectorFactory();

  /** @private {Array<string>} */
  this.appCapabilities_ = [
    remoting.ClientSession.Capability.SEND_INITIAL_RESOLUTION,
    remoting.ClientSession.Capability.RATE_LIMIT_RESIZE_REQUESTS,
    remoting.ClientSession.Capability.VIDEO_RECORDER
  ];
  // Append the app-specific capabilities.
  this.appCapabilities_.push.apply(this.appCapabilities_, appCapabilities);

  /** @protected {remoting.SessionConnector} */
  this.sessionConnector_ = remoting.SessionConnector.factory.createConnector(
      document.getElementById('client-container'),
      this.onConnected_.bind(this),
      this.onError_.bind(this),
      this.onConnectionFailed_.bind(this),
      this.appCapabilities_);

  /** @private {base.Disposable} */
  this.sessionConnectedHooks_ = null;
};

/**
 * @return {remoting.SessionConnector} The session connector.
 */
remoting.Application.prototype.getSessionConnector = function() {
  return this.sessionConnector_;
};

/**
 * @param {remoting.ClientSession.Capability} capability
 * @return {boolean}
 */
remoting.Application.prototype.hasCapability = function(capability) {
  var capabilities = this.appCapabilities_;
  return capabilities.indexOf(capability) != -1;
};

/* Disconnect the remoting client. */
remoting.Application.prototype.disconnect = function() {
  if (remoting.clientSession) {
    remoting.clientSession.disconnect(remoting.Error.none());
    console.log('Disconnected.');
  }
};

/* Public method to exit the application. */
remoting.Application.prototype.quit = function() {
  this.exitApplication_();
};

/**
 * Close the main window when quitting the application. This should be called
 * by exitApplication() in the subclass.
 * @protected
 */
remoting.Application.prototype.closeMainWindow_ = function() {
  chrome.app.window.current().close();
};

/**
 * Initialize the application and register all event handlers. After this
 * is called, the app is running and waiting for user events.
 */
remoting.Application.prototype.start = function() {
  // TODO(garykac): This should be owned properly rather than living in the
  // global 'remoting' namespace.
  remoting.settings = new remoting.Settings();

  remoting.initGlobalObjects();
  remoting.initIdentity();

  this.initApplication_();

  var that = this;
  remoting.identity.getToken().
    then(this.startApplication_.bind(this)).
    catch(remoting.Error.handler(
      function(/** !remoting.Error */ error) {
        if (error.hasTag(remoting.Error.Tag.CANCELLED)) {
          that.exitApplication_();
        } else {
          that.signInFailed_(error);
        }
      }
    )
  );
};

/**
 * Called when a new session has been connected.
 *
 * @param {remoting.ConnectionInfo} connectionInfo
 * @return {void} Nothing.
 * @protected
 */
remoting.Application.prototype.initSession_ = function(connectionInfo) {
  this.sessionConnectedHooks_ = new base.Disposables(
    new base.EventHook(connectionInfo.session(), 'stateChanged',
                       this.onSessionFinished_.bind(this)),
    new base.RepeatingTimer(this.updateStatistics_.bind(this), 1000)
  );
  remoting.clipboard.startSession();
};

/**
 * Callback function called when the state of the client plugin changes. The
 * current and previous states are available via the |state| member variable.
 *
 * @param {remoting.ClientSession.StateEvent=} state
 * @private
 */
remoting.Application.prototype.onSessionFinished_ = function(state) {
  switch (state.current) {
    case remoting.ClientSession.State.CLOSED:
      console.log('Connection closed by host');
      this.onDisconnected_();
      break;
    case remoting.ClientSession.State.FAILED:
      var error = remoting.clientSession.getError();
      console.error('Client plugin reported connection failed: ' +
                    error.toString());
      if (error === null) {
        error = remoting.Error.unexpected();
      }
      this.onError_(error);
      break;

    default:
      console.error('Unexpected client plugin state: ' + state.current);
      // This should only happen if the web-app and client plugin get out of
      // sync, so MISSING_PLUGIN is a suitable error.
      this.onError_(new remoting.Error(remoting.Error.Tag.MISSING_PLUGIN));
      break;
  }

  base.dispose(this.sessionConnectedHooks_);
  this.sessionConnectedHooks_= null;
  this.sessionConnector_.closeSession();
};

/** @private */
remoting.Application.prototype.updateStatistics_ = function() {
  var perfstats = remoting.clientSession.getPerfStats();
  remoting.stats.update(perfstats);
  remoting.clientSession.logStatistics(perfstats);
};


/*
 * remoting.ApplicationInterface
 * These functions must be overridden in the subclass.
 */

/** @return {string} */
remoting.Application.prototype.getApplicationName = function() {
  base.debug.assert(false, "Subclass must override");
};

/**
 * @param {!remoting.Error} error
 * @protected
 */
remoting.Application.prototype.signInFailed_ = function(error) {
  base.debug.assert(false, "Subclass must override");
};

/** @protected */
remoting.Application.prototype.initApplication_ = function() {
  base.debug.assert(false, "Subclass must override");
};

/**
 * @param {string} token
 * @protected
 */
remoting.Application.prototype.startApplication_ = function(token) {
  base.debug.assert(false, "Subclass must override");
};

/** @protected */
remoting.Application.prototype.exitApplication_ = function() {
  base.debug.assert(false, "Subclass must override");
};

/**
 * @param {remoting.ConnectionInfo} connectionInfo
 * @protected
 */
remoting.Application.prototype.onConnected_ = function(connectionInfo) {
  base.debug.assert(false, "Subclass must override");
};

/** @protected */
remoting.Application.prototype.onDisconnected_ = function() {
  base.debug.assert(false, "Subclass must override");
};

/**
 * @param {!remoting.Error} error
 * @protected
 */
remoting.Application.prototype.onConnectionFailed_ = function(error) {
  base.debug.assert(false, "Subclass must override");
};

/**
 * @param {!remoting.Error} error The error to be localized and displayed.
 * @protected
 */
remoting.Application.prototype.onError_ = function(error) {
  base.debug.assert(false, "Subclass must override");
};


/**
 * The interface specifies the methods that a subclass of remoting.Application
 * is required implement to override the default behavior.
 *
 * @interface
 */
remoting.ApplicationInterface = function() {};

/**
 * @return {string} Application product name to be used in UI.
 */
remoting.ApplicationInterface.prototype.getApplicationName = function() {};

/**
 * Report an authentication error to the user. This is called in lieu of
 * startApplication() if the user cannot be authenticated or if they decline
 * the app permissions.
 *
 * @param {!remoting.Error} error The failure reason.
 */
remoting.ApplicationInterface.prototype.signInFailed_ = function(error) {};

/**
 * Initialize the application. This is called before an OAuth token is requested
 * and should be used for tasks such as initializing the DOM, registering event
 * handlers, etc. After this is called, the app is running and waiting for
 * user events.
 */
remoting.ApplicationInterface.prototype.initApplication_ = function() {};

/**
 * Start the application. Once startApplication() is called, we can assume that
 * the user has consented to all permissions specified in the manifest.
 *
 * @param {string} token An OAuth access token. The app should not cache
 *     this token, but can assume that it will remain valid during application
 *     start-up.
 */
remoting.ApplicationInterface.prototype.startApplication_ = function(token) {};

/**
 * Close down the application before exiting.
 */
remoting.ApplicationInterface.prototype.exitApplication_ = function() {};

/**
 * Called when a new session has been connected.
 *
 * @param {remoting.ConnectionInfo} connectionInfo
 */
remoting.ApplicationInterface.prototype.onConnected_ =
    function(connectionInfo) {};

/**
 * Called when the current session has been disconnected.
 */
remoting.ApplicationInterface.prototype.onDisconnected_ = function() {};

/**
 * Called when the current session's connection has failed.
 *
 * @param {!remoting.Error} error
 */
remoting.ApplicationInterface.prototype.onConnectionFailed_ =
    function(error) {};

/**
 * Called when an error needs to be displayed to the user.
 *
 * @param {!remoting.Error} errorTag The error to be localized and displayed.
 */
remoting.ApplicationInterface.prototype.onError_ = function(errorTag) {};


/** @type {remoting.Application} */
remoting.app = null;
