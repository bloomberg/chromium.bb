// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Description of this file.
 * Class handling interaction with the cast extension session of the Chromoting
 * host. It receives and sends extension messages from/to the host through
 * the client session. It uses the Google Cast Chrome Sender API library to
 * interact with nearby Cast receivers.
 *
 * Once it establishes connection with a Cast device (upon user choice), it
 * creates a session, loads our registered receiver application and then becomes
 * a message proxy between the host and cast device, helping negotiate
 * their peer connection.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @param {!remoting.ClientSession} clientSession The client session to send
 * cast extension messages to.
 */
remoting.CastExtensionHandler = function(clientSession) {
  /** @private */
  this.clientSession_ = clientSession;

  /** @type {chrome.cast.Session} @private */
  this.session_ = null;

  /** @type {string} @private */
  this.kCastNamespace_ = 'urn:x-cast:com.chromoting.cast.all';

  /** @type {string} @private */
  this.kApplicationId_ = "8A1211E3";

  /** @type {Array.<Object>} @private */
  this.messageQueue_ = [];

  this.start_();
};

/**
 * The id of the script node.
 * @type {string}
 * @private
 */
remoting.CastExtensionHandler.prototype.SCRIPT_NODE_ID_ = 'cast-script-node';

/**
 * Attempts to load the Google Cast Chrome Sender API libary.
 * @private
 */
remoting.CastExtensionHandler.prototype.start_ = function() {
  var node = document.getElementById(this.SCRIPT_NODE_ID_);
  if (node) {
    console.error(
        'Multiple calls to CastExtensionHandler.start_ not expected.');
    return;
  }

  // Create a script node to load the Cast Sender API.
  node = document.createElement('script');
  node.id = this.SCRIPT_NODE_ID_;
  node.src = "https://www.gstatic.com/cv/js/sender/v1/cast_sender.js";
  node.type = 'text/javascript';
  document.body.insertBefore(node, document.body.firstChild);

  /** @type {remoting.CastExtensionHandler} */
  var that = this;
  var onLoad = function() {
    window['__onGCastApiAvailable'] = that.onGCastApiAvailable.bind(that);

  };
  var onLoadError = function(event) {
    console.error("Failed to load Chrome Cast Sender API.");
  }
  node.addEventListener('load', onLoad, false);
  node.addEventListener('error', onLoadError, false);

};

/**
 * Process Cast Extension Messages from the Chromoting host.
 * @param {string} msgString The extension message's data.
 */
remoting.CastExtensionHandler.prototype.onMessage = function(msgString) {
  var message = getJsonObjectFromString(msgString);

  // Save messages to send after a session is established.
  this.messageQueue_.push(message);
  // Trigger the sending of pending messages, followed by the one just
  // received.
  if (this.session_) {
    this.sendPendingMessages_();
  }
};

/**
 * Send cast-extension messages through the client session.
 * @param {Object} response The JSON response to be sent to the host. The
 * response object must contain the appropriate keys.
 * @private
 */
remoting.CastExtensionHandler.prototype.sendMessageToHost_ =
    function(response) {
  this.clientSession_.sendCastExtensionMessage(response);
};

/**
 * Send pending messages from the host to the receiver app.
 * @private
 */
remoting.CastExtensionHandler.prototype.sendPendingMessages_ = function() {
  var len = this.messageQueue_.length;
  for(var i = 0; i<len; i++) {
    this.session_.sendMessage(this.kCastNamespace_,
                              this.messageQueue_[i],
                              this.sendMessageSuccess.bind(this),
                              this.sendMessageFailure.bind(this));
  }
  this.messageQueue_ = [];
};

/**
 * Event handler for '__onGCastApiAvailable' window event. This event is
 * triggered if the Google Cast Chrome Sender API is available. We attempt to
 * load this API in this.start(). If the API loaded successfully, we can proceed
 * to initialize it and configure it to launch our Cast Receiver Application.
 *
 * @param {boolean} loaded True if the API loaded succesfully.
 * @param {Object} errorInfo Info if the API load failed.
 */
remoting.CastExtensionHandler.prototype.onGCastApiAvailable =
    function(loaded, errorInfo) {
  if (loaded) {
    this.initializeCastApi();
  } else {
    console.log(errorInfo);
  }
};

/**
 * Initialize the Cast API.
 * @private
 */
remoting.CastExtensionHandler.prototype.initializeCastApi = function() {
  var sessionRequest = new chrome.cast.SessionRequest(this.kApplicationId_);
  var apiConfig =
      new chrome.cast.ApiConfig(sessionRequest,
                                this.sessionListener.bind(this),
                                this.receiverListener.bind(this),
                                chrome.cast.AutoJoinPolicy.PAGE_SCOPED,
                                chrome.cast.DefaultActionPolicy.CREATE_SESSION);
  chrome.cast.initialize(
      apiConfig, this.onInitSuccess.bind(this), this.onInitError.bind(this));
};

/**
 * Callback for successful initialization of the Cast API.
 */
remoting.CastExtensionHandler.prototype.onInitSuccess = function() {
  console.log("Initialization Successful.");
};

/**
 * Callback for failed initialization of the Cast API.
 */
remoting.CastExtensionHandler.prototype.onInitError = function() {
  console.error("Initialization Failed.");
};

/**
 * Listener invoked when a session is created or connected by the SDK.
 * Note: The requestSession method would not cause this callback to be invoked
 * since it is passed its own listener.
 * @param {chrome.cast.Session} session The resulting session.
 */
remoting.CastExtensionHandler.prototype.sessionListener = function(session) {
  console.log('New Session:' + /** @type {string} */ (session.sessionId));
  this.session_ = session;
  if (this.session_.media.length != 0) {

    // There should be no media associated with the session, since we never
    // directly load media from the Sender application.
    this.onMediaDiscovered('sessionListener', this.session_.media[0]);
  }
  this.session_.addMediaListener(
      this.onMediaDiscovered.bind(this, 'addMediaListener'));
  this.session_.addUpdateListener(this.sessionUpdateListener.bind(this));
  this.session_.addMessageListener(this.kCastNamespace_,
                                   this.chromotingMessageListener.bind(this));
  this.session_.sendMessage(this.kCastNamespace_,
      {subject : 'test', chromoting_data : 'Hello, Cast.'},
      this.sendMessageSuccess.bind(this),
      this.sendMessageFailure.bind(this));
  this.sendPendingMessages_();
};

/**
 * Listener invoked when a media session is created by another sender.
 * @param {string} how How this callback was triggered.
 * @param {chrome.cast.media.Media} media The media item discovered.
 * @private
 */
remoting.CastExtensionHandler.prototype.onMediaDiscovered =
    function(how, media) {
      console.error("Unexpected media session discovered.");
};

/**
 * Listener invoked when a cast extension message was sent to the cast device
 * successfully.
 * @private
 */
remoting.CastExtensionHandler.prototype.sendMessageSuccess = function() {
};

/**
 * Listener invoked when a cast extension message failed to be sent to the cast
 * device.
 * @param {Object} error The error.
 * @private
 */
remoting.CastExtensionHandler.prototype.sendMessageFailure = function(error) {
  console.error('Failed to Send Message.', error);
};

/**
 * Listener invoked when a cast extension message is received from the Cast
 * device.
 * @param {string} ns The namespace of the message received.
 * @param {string} message The stringified JSON message received.
 */
remoting.CastExtensionHandler.prototype.chromotingMessageListener =
    function(ns, message) {
  if (ns === this.kCastNamespace_) {
    try {
        var messageObj = getJsonObjectFromString(message);
        this.sendMessageToHost_(messageObj);
    } catch (err) {
      console.error('Failed to process message from Cast device.');
    }
  } else {
    console.error("Unexpected message from Cast device.");
  }
};

/**
 * Listener invoked when there updates to the current session.
 *
 * @param {boolean} isAlive True if the session is still alive.
 */
remoting.CastExtensionHandler.prototype.sessionUpdateListener =
    function(isAlive) {
  var message = isAlive ? 'Session Updated' : 'Session Removed';
  message += ': ' + this.session_.sessionId +'.';
  console.log(message);
};

/**
 * Listener invoked when the availability of a Cast receiver that supports
 * the application in sessionRequest is known or changes.
 *
 * @param {chrome.cast.ReceiverAvailability} availability Receiver availability.
 */
remoting.CastExtensionHandler.prototype.receiverListener =
    function(availability) {
  if (availability === chrome.cast.ReceiverAvailability.AVAILABLE) {
    console.log("Receiver(s) Found.");
  } else {
    console.error("No Receivers Available.");
  }
};

/**
 * Launches the associated receiver application by requesting that it be created
 * on the Cast device. It uses the SessionRequest passed during initialization
 * to determine what application to launch on the Cast device.
 *
 * Note: This method is intended to be used as a click listener for a custom
 * cast button on the webpage. We currently use the default cast button in
 * Chrome, so this method is unused.
 */
remoting.CastExtensionHandler.prototype.launchApp = function() {
  chrome.cast.requestSession(this.onRequestSessionSuccess.bind(this),
                             this.onLaunchError.bind(this));
};

/**
 * Listener invoked when chrome.cast.requestSession completes successfully.
 *
 * @param {chrome.cast.Session} session The requested session.
 */
remoting.CastExtensionHandler.prototype.onRequestSessionSuccess =
    function (session) {
  this.session_ = session;
  this.session_.addUpdateListener(this.sessionUpdateListener.bind(this));
  if (this.session_.media.length != 0) {
    this.onMediaDiscovered('onRequestSession', this.session_.media[0]);
  }
  this.session_.addMediaListener(
      this.onMediaDiscovered.bind(this, 'addMediaListener'));
  this.session_.addMessageListener(this.kCastNamespace_,
                                   this.chromotingMessageListener.bind(this));
};

/**
 * Listener invoked when chrome.cast.requestSession fails.
 * @param {chrome.cast.Error} error The error code.
 */
remoting.CastExtensionHandler.prototype.onLaunchError = function(error) {
  console.error("Error Casting to Receiver.", error);
};

/**
 * Stops the running receiver application associated with the session.
 * TODO(aiguha): When the user disconnects using the blue drop down bar,
 * the client session should notify the CastExtensionHandler, which should
 * call this method to close the session with the Cast device.
 */
remoting.CastExtensionHandler.prototype.stopApp = function() {
  this.session_.stop(this.onStopAppSuccess.bind(this),
                     this.onStopAppError.bind(this));
};

/**
 * Listener invoked when the receiver application is stopped successfully.
 */
remoting.CastExtensionHandler.prototype.onStopAppSuccess = function() {
};

/**
 * Listener invoked when we fail to stop the receiver application.
 *
 * @param {chrome.cast.Error} error The error code.
 */
remoting.CastExtensionHandler.prototype.onStopAppError = function(error) {
  console.error('Error Stopping App: ', error);
};
