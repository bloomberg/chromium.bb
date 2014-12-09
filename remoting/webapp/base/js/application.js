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
  /**
   * @type {remoting.Application.Delegate}
   * @private
   */
  this.delegate_ = null;

  /**
   * @type {remoting.SessionConnector}
   * @private
   */
  this.session_connector_ = null;
};

/**
 * @param {remoting.Application.Delegate} appDelegate The delegate that
 *    contains the app-specific functionality.
 */
remoting.Application.prototype.setDelegate = function(appDelegate) {
  this.delegate_ = appDelegate;
};

/**
 * Initialize the application and register all event handlers. After this
 * is called, the app is running and waiting for user events.
 *
 * @return {void} Nothing.
 */
remoting.Application.prototype.init = function() {
  this.delegate_.init();
};

/**
 * Called when a new session has been connected.
 *
 * @param {remoting.ClientSession} clientSession
 * @return {void} Nothing.
 */
remoting.Application.prototype.onConnected = function(clientSession) {
  // TODO(garykac): Make clientSession a member var of Application.
  remoting.clientSession = clientSession;
  remoting.clientSession.addEventListener('stateChanged', onClientStateChange_);

  remoting.clipboard.startSession();
  updateStatistics_();
  remoting.hangoutSessionEvents.raiseEvent(
      remoting.hangoutSessionEvents.sessionStateChanged,
      remoting.ClientSession.State.CONNECTED
  );

  this.delegate_.onConnected(clientSession);
};

/**
 * Called when the current session has been disconnected.
 *
 * @return {void} Nothing.
 */
remoting.Application.prototype.onDisconnected = function() {
  this.delegate_.onDisconnected();
};

/**
 * Called when the current session has reached the point where the host has
 * started streaming video frames to the client.
 *
 * @return {void} Nothing.
 */
remoting.Application.prototype.onVideoStreamingStarted = function() {
  this.delegate_.onVideoStreamingStarted();
};

/**
 * Called when an extension message needs to be handled.
 *
 * @param {string} type The type of the extension message.
 * @param {string} data The payload of the extension message.
 * @return {boolean} Return true if the extension message was recognized.
 */
remoting.Application.prototype.onExtensionMessage = function(type, data) {
  // Give the delegate a chance to handle this extension message first.
  if (this.delegate_.onExtensionMessage(type, data)) {
    return true;
  }

  if (remoting.clientSession) {
    return remoting.clientSession.handleExtensionMessage(type, data);
  }
  return false;
};

/**
 * Called when an error needs to be displayed to the user.
 *
 * @param {remoting.Error} errorTag The error to be localized and displayed.
 * @return {void} Nothing.
 */
remoting.Application.prototype.onError = function(errorTag) {
  this.delegate_.onError(errorTag);
};

/**
 * @return {remoting.SessionConnector} A session connector, creating a new one
 *     if necessary.
 */
remoting.Application.prototype.getSessionConnector = function() {
  // TODO(garykac): Check if this can be initialized in the ctor.
  if (!this.session_connector_) {
    this.session_connector_ = remoting.SessionConnector.factory.createConnector(
        document.getElementById('video-container'),
        this.onConnected.bind(this),
        this.onError.bind(this),
        this.onExtensionMessage.bind(this),
        this.delegate_.getRequiredCapabilities(),
        this.delegate_.getDefaultRemapKeys());
  }
  return this.session_connector_;
};


/**
 * @interface
 */
remoting.Application.Delegate = function() {};

/**
 * Initialize the application and register all event handlers. After this
 * is called, the app is running and waiting for user events.
 *
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.init = function() {};

/**
 * @return {string} The default remap keys for the current platform.
 */
remoting.Application.Delegate.prototype.getDefaultRemapKeys = function() {};

/**
 * @return {Array.<string>} A list of |ClientSession.Capability|s required
 *     by this application.
 */
remoting.Application.Delegate.prototype.getRequiredCapabilities = function() {};

/**
 * Called when a new session has been connected.
 *
 * @param {remoting.ClientSession} clientSession
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.onConnected = function(clientSession) {
};

/**
 * Called when the current session has been disconnected.
 *
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.onDisconnected = function() {};

/**
 * Called when the current session has reached the point where the host has
 * started streaming video frames to the client.
 *
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.onVideoStreamingStarted = function() {};

/**
 * Called when an extension message needs to be handled.
 *
 * @param {string} type The type of the extension message.
 * @param {string} data The payload of the extension message.
 * @return {boolean} Return true if the extension message was recognized.
 */
remoting.Application.Delegate.prototype.onExtensionMessage = function(
    type, data) {};

/**
 * Called when an error needs to be displayed to the user.
 *
 * @param {remoting.Error} errorTag The error to be localized and displayed.
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.onError = function(errorTag) {};


/** @type {remoting.Application} */
remoting.app = null;
