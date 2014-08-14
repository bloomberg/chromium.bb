// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * It2MeHelperChannel relays messages between Hangouts and Chrome Remote Desktop
 * (webapp) for the helper (the Hangouts participant who is giving remote
 * assistance).
 *
 * It runs in the background page and contains two chrome.runtime.Port objects,
 * respresenting connections to the webapp and hangout, respectively.
 *
 * Connection is always initiated from Hangouts.
 *
 *   Hangout                     It2MeHelperChannel        Chrome Remote Desktop
 *      |-----runtime.connect() ------>|                                |
 *      |------connect message-------->|                                |
 *      |                              |-------appLauncher.launch()---->|
 *      |                              |<------runtime.connect()------- |
 *      |                              |<-----sessionStateChanged------ |
 *      |<----sessionStateChanged------|                                |
 *
 * Disconnection can be initiated from either side:
 * 1. In the normal flow initiated from hangout
 *    Hangout                    It2MeHelperChannel        Chrome Remote Desktop
 *       |-----disconnect message------>|                               |
 *       |<-sessionStateChanged(CLOSED)-|                               |
 *       |                              |-----appLauncher.close()------>|
 *
 * 2. In the normal flow initiated from webapp
 *    Hangout                    It2MeHelperChannel        Chrome Remote Desktop
 *       |                              |<-sessionStateChanged(CLOSED)--|
 *       |                              |<--------port.disconnect()-----|
 *       |<--------port.disconnect()----|                               |
 *
 * 2. If hangout crashes
 *    Hangout                    It2MeHelperChannel        Chrome Remote Desktop
 *       |---------port.disconnect()--->|                               |
 *       |                              |--------port.disconnect()----->|
 *       |                              |------appLauncher.close()----->|
 *
 * 3. If webapp crashes
 *    Hangout                    It2MeHelperChannel        Chrome Remote Desktop
 *       |                              |<-------port.disconnect()------|
 *       |<-sessionStateChanged(FAILED)-|                               |
 *       |<--------port.disconnect()----|                               |
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {remoting.AppLauncher} appLauncher
 * @param {chrome.runtime.Port} hangoutPort Represents an active connection to
 *     Hangouts.
 * @param {function(remoting.It2MeHelperChannel)} onDisconnectCallback Callback
 *     to notify when the connection is torn down.  IT2MeService uses this
 *     callback to dispose of the channel object.
 * @constructor
 */
remoting.It2MeHelperChannel =
    function(appLauncher, hangoutPort, onDisconnectCallback) {

  /**
   * @type {remoting.AppLauncher}
   * @private
   */
  this.appLauncher_ = appLauncher;

  /**
   * @type {chrome.runtime.Port}
   * @private
   */
  this.hangoutPort_ = hangoutPort;

  /**
   * @type {chrome.runtime.Port}
   * @private
   */
  this.webappPort_ = null;

  /**
   * @type {string}
   * @private
   */
  this.instanceId_ = '';

  /**
   * @type {remoting.ClientSession.State}
   * @private
   */
  this.sessionState_ = remoting.ClientSession.State.CONNECTING;

  /**
   * @type {?function(remoting.It2MeHelperChannel)}
   * @private
   */
  this.onDisconnectCallback_ = onDisconnectCallback;

  this.onWebappMessageRef_ = this.onWebappMessage_.bind(this);
  this.onWebappDisconnectRef_ = this.onWebappDisconnect_.bind(this);
  this.onHangoutMessageRef_ = this.onHangoutMessage_.bind(this);
  this.onHangoutDisconnectRef_ = this.onHangoutDisconnect_.bind(this);
};

/** @enum {string} */
remoting.It2MeHelperChannel.HangoutMessageTypes = {
  CONNECT: 'connect',
  DISCONNECT: 'disconnect'
};

/** @enum {string} */
remoting.It2MeHelperChannel.WebappMessageTypes = {
  SESSION_STATE_CHANGED: 'sessionStateChanged'
};

remoting.It2MeHelperChannel.prototype.init = function() {
  this.hangoutPort_.onMessage.addListener(this.onHangoutMessageRef_);
  this.hangoutPort_.onDisconnect.addListener(this.onHangoutDisconnectRef_);
};

/** @return {string} */
remoting.It2MeHelperChannel.prototype.instanceId = function() {
  return this.instanceId_;
};

/**
 * @param {{method:string, data:Object.<string,*>}} message
 * @return {boolean} whether the message is handled or not.
 * @private
 */
remoting.It2MeHelperChannel.prototype.onHangoutMessage_ = function(message) {
  try {
    var MessageTypes = remoting.It2MeHelperChannel.HangoutMessageTypes;
    switch (message.method) {
      case MessageTypes.CONNECT:
        this.launchWebapp_(message);
        return true;
      case MessageTypes.DISCONNECT:
        this.closeWebapp_(message);
        return true;
    }
  } catch(e) {
    var error = /** @type {Error} */ e;
    console.error(error);
    this.hangoutPort_.postMessage({
      method: message.method + 'Response',
      error: error.message
    });
  }
  return false;
};

/**
 * Disconnect the existing connection to the helpee.
 *
 * @param {{method:string, data:Object.<string,*>}} message
 * @private
 */
remoting.It2MeHelperChannel.prototype.closeWebapp_ =
    function(message) {
  // TODO(kelvinp): Closing the v2 app currently doesn't disconnect the IT2me
  // session (crbug.com/402137), so send an explicit notification to Hangouts.
  this.sessionState_ = remoting.ClientSession.State.CLOSED;
  this.hangoutPort_.postMessage({
    method: 'sessionStateChanged',
    state: this.sessionState_
  });
  this.appLauncher_.close(this.instanceId_);
};

/**
 * Launches the web app.
 *
 * @param {{method:string, data:Object.<string,*>}} message
 * @private
 */
remoting.It2MeHelperChannel.prototype.launchWebapp_ =
    function(message) {
  var accessCode = getStringAttr(message, 'accessCode');
  if (!accessCode) {
    throw new Error('Access code is missing');
  }

  // Launch the webapp.
  this.appLauncher_.launch({
    mode: 'hangout',
    accessCode: accessCode
  }).then(
    /**
     * @this {remoting.It2MeHelperChannel}
     * @param {string} instanceId
     */
    function(instanceId){
      this.instanceId_ = instanceId;
    }.bind(this));
};

/**
 * @private
 */
remoting.It2MeHelperChannel.prototype.onHangoutDisconnect_ = function() {
  this.appLauncher_.close(this.instanceId_);
  this.unhookPorts_();
};

/**
 * @param {chrome.runtime.Port} port The port represents a connection to the
 *     webapp.
 * @param {string} id The id of the tab or window that is hosting the webapp.
 */
remoting.It2MeHelperChannel.prototype.onWebappConnect = function(port, id) {
  base.debug.assert(id === this.instanceId_);
  base.debug.assert(this.hangoutPort_ !== null);

  // Hook listeners.
  port.onMessage.addListener(this.onWebappMessageRef_);
  port.onDisconnect.addListener(this.onWebappDisconnectRef_);
  this.webappPort_ = port;
};

/** @param {chrome.runtime.Port} port The webapp port. */
remoting.It2MeHelperChannel.prototype.onWebappDisconnect_ = function(port) {
  // If the webapp port got disconnected while the session is still connected,
  // treat it as an error.
  var States = remoting.ClientSession.State;
  if (this.sessionState_ === States.CONNECTING ||
      this.sessionState_ === States.CONNECTED) {
    this.sessionState_ = States.FAILED;
    this.hangoutPort_.postMessage({
      method: 'sessionStateChanged',
      state: this.sessionState_
    });
  }
  this.unhookPorts_();
};

/**
 * @param {{method:string, data:Object.<string,*>}} message
 * @private
 */
remoting.It2MeHelperChannel.prototype.onWebappMessage_ = function(message) {
  try {
    console.log('It2MeHelperChannel id=' + this.instanceId_ +
                ' incoming message method=' + message.method);
    var MessageTypes = remoting.It2MeHelperChannel.WebappMessageTypes;
    switch (message.method) {
      case MessageTypes.SESSION_STATE_CHANGED:
        var state = getNumberAttr(message, 'state');
        this.sessionState_ =
            /** @type {remoting.ClientSession.State} */ state;
        this.hangoutPort_.postMessage(message);
        return true;
    }
    throw new Error('Unknown message method=' + message.method);
  } catch(e) {
    var error = /** @type {Error} */ e;
    console.error(error);
    this.webappPort_.postMessage({
      method: message.method + 'Response',
      error: error.message
    });
  }
  return false;
};

remoting.It2MeHelperChannel.prototype.unhookPorts_ = function() {
  if (this.webappPort_) {
    this.webappPort_.onMessage.removeListener(this.onWebappMessageRef_);
    this.webappPort_.onDisconnect.removeListener(this.onWebappDisconnectRef_);
    this.webappPort_.disconnect();
    this.webappPort_ = null;
  }

  if (this.hangoutPort_) {
    this.hangoutPort_.onMessage.removeListener(this.onHangoutMessageRef_);
    this.hangoutPort_.onDisconnect.removeListener(this.onHangoutDisconnectRef_);
    this.hangoutPort_.disconnect();
    this.hangoutPort_ = null;
  }

  if (this.onDisconnectCallback_) {
    this.onDisconnectCallback_(this);
    this.onDisconnectCallback_  = null;
  }
};
