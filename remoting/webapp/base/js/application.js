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
 * @constructor
 */
remoting.Application = function() {
  // Create global factories.
  remoting.ClientPlugin.factory = new remoting.DefaultClientPluginFactory();
  remoting.SessionConnector.factory =
      new remoting.DefaultSessionConnectorFactory();

  /** @protected {remoting.Application.Mode} */
  this.connectionMode_ = remoting.Application.Mode.ME2ME;
};

/**
 * The current application mode (IT2Me, Me2Me or AppRemoting).
 *
 * TODO(kelvinp): Remove this when Me2Me and It2Me are abstracted into its
 * own flow objects.
 *
 * @enum {number}
 */
remoting.Application.Mode = {
  IT2ME: 0,
  ME2ME: 1,
  APP_REMOTING: 2
};

/**
 * Get the connection mode (Me2Me, IT2Me or AppRemoting).
 *
 * @return {remoting.Application.Mode}
 */
remoting.Application.prototype.getConnectionMode = function() {
  return this.connectionMode_;
};

/**
 * Set the connection mode (Me2Me, IT2Me or AppRemoting).
 *
 * @param {remoting.Application.Mode} mode
 */
remoting.Application.prototype.setConnectionMode = function(mode) {
  this.connectionMode_ = mode;
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
  remoting.identity.getToken().then(
    this.startApplication_.bind(this)
  ).catch(function(/** !remoting.Error */ error) {
    if (error.hasTag(remoting.Error.Tag.CANCELLED)) {
      that.exitApplication_();
    } else {
      that.signInFailed_(error);
    }
  });
};

/**
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

/** @type {remoting.Application} */
remoting.app = null;
