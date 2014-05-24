// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class handling reconnecting the session when it is disconnected due to
 * network failure.
 *
 * The SmartReconnector listens for changes in connection state of
 * |clientSession| to determine if a reconnection is needed.  It then calls into
 * |connector| to reconnect the session.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @param {remoting.SessionConnector} connector This is used to reconnect the
 *    the session when necessary
 * @param {remoting.ClientSession} clientSession This represents the current
 *    remote desktop connection.  It is used to monitor the changes in
 *    connection state.
 * @implements {base.Disposable}
 */
remoting.SmartReconnector = function(connector, clientSession) {
  /** @private */
  this.connector_ = connector;

  /** @private */
  this.clientSession_ = clientSession;

  /** @private */
  this.reconnectTimerId_ = null;

  /** @private */
  this.connectionTimeoutTimerId_ = null;

  /** @private */
  this.bound_ = {
    reconnect: this.reconnect_.bind(this),
    reconnectAsync: this.reconnectAsync_.bind(this),
    startReconnectTimeout: this.startReconnectTimeout_.bind(this),
    stateChanged: this.stateChanged_.bind(this),
    videoChannelStateChanged: this.videoChannelStateChanged_.bind(this)
  };

  clientSession.addEventListener(
      remoting.ClientSession.Events.stateChanged,
      this.bound_.stateChanged);
  clientSession.addEventListener(
      remoting.ClientSession.Events.videoChannelStateChanged,
      this.bound_.videoChannelStateChanged);
};

// The online event only means the network adapter is enabled, but
// it doesn't necessarily mean that we have a working internet connection.
// Therefore, delay the connection by |kReconnectDelay| to allow for the network
// to connect.
remoting.SmartReconnector.kReconnectDelay = 2000;

// If the video channel is inactive for 10 seconds reconnect the session.
remoting.SmartReconnector.kConnectionTimeout = 10000;

remoting.SmartReconnector.prototype = {
  reconnect_: function() {
    this.cancelPending_();
    remoting.disconnect();
    remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
    this.connector_.reconnect();
  },

  reconnectAsync_: function() {
    this.cancelPending_();
    remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
    this.reconnectTimerId_ = window.setTimeout(
        this.bound_.reconnect, remoting.SmartReconnector.kReconnectDelay);
  },

  /**
   * @param {remoting.ClientSession.StateEvent} event
   */
  stateChanged_: function(event) {
    var State = remoting.ClientSession.State;
    if (event.previous === State.CONNECTED && event.current === State.FAILED) {
      this.cancelPending_();
      if (navigator.onLine) {
        this.reconnect_();
      } else {
        window.addEventListener('online', this.bound_.reconnectAsync, false);
      }
    }
  },

  /**
   * @param {boolean} active  True if the video channel is active.
   */
  videoChannelStateChanged_: function (active) {
    this.cancelPending_();
    if (!active) {
      window.addEventListener(
          'online', this.bound_.startReconnectTimeout, false);
    }
  },

  startReconnectTimeout_: function () {
    this.cancelPending_();
    this.connectionTimeoutTimerId_ = window.setTimeout(
          this.bound_.reconnect, remoting.SmartReconnector.kConnectionTimeout);
  },

  cancelPending_: function() {
    window.removeEventListener(
        'online', this.bound_.startReconnectTimeout, false);
    window.removeEventListener('online', this.bound_.reconnectAsync, false);
    window.clearTimeout(this.reconnectTimerId_);
    window.clearTimeout(this.connectionTimeoutTimerId_);
    this.reconnectTimerId_ = null;
    this.connectionTimeoutTimerId_ = null;
  },

  dispose: function() {
    this.clientSession_.removeEventListener(
        remoting.ClientSession.Events.stateChanged,
        this.bound_.stateChanged);
    this.clientSession_.removeEventListener(
        remoting.ClientSession.Events.videoChannelStateChanged,
        this.bound_.videoChannelStateChanged);
  }
};
