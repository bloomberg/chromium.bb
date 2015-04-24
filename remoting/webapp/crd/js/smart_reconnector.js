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

/** @suppress {duplicate} */
var remoting = remoting || {};

(function () {

'use strict';

/**
 * @constructor
 * @param {remoting.ConnectingDialog} connectingDialog
 * @param {function()} reconnectCallback
 * @param {function()} disconnectCallback
 * @param {remoting.ClientSession} clientSession This represents the current
 *    remote desktop connection.  It is used to monitor the changes in
 *    connection state.
 * @implements {base.Disposable}
 */
remoting.SmartReconnector = function(connectingDialog, reconnectCallback,
                                     disconnectCallback, clientSession) {
  /** @private */
  this.reconnectCallback_ = reconnectCallback;

  /** @private */
  this.disconnectCallback_ = disconnectCallback;

  /** @private */
  this.clientSession_ = clientSession;

  /**
   * Placeholder of any pending reconnect operations, e.g. Waiting for online,
   * or a timeout to reconnect.
   *
   * @private {base.Disposable}
   */
  this.pending_ = null;

  /** @private */
  this.connectingDialog_ = connectingDialog;

  var Events = remoting.ClientSession.Events;
  /** @private */
  this.eventHook_ =
      new base.EventHook(clientSession, Events.videoChannelStateChanged,
                         this.videoChannelStateChanged_.bind(this));
};

// The online event only means the network adapter is enabled, but
// it doesn't necessarily mean that we have a working internet connection.
// Therefore, delay the connection by |RECONNECT_DELAY_MS| to allow for the
// network to connect.
var RECONNECT_DELAY_MS = 2000;

// If the video channel is inactive for 10 seconds reconnect the session.
var CONNECTION_TIMEOUT_MS = 10000;

remoting.SmartReconnector.prototype.reconnect_ = function() {
  this.cancelPending_();
  this.disconnectCallback_();
  this.reconnectCallback_();
};

remoting.SmartReconnector.prototype.reconnectAsync_ = function() {
  this.cancelPending_();
  this.connectingDialog_.show();
  this.pending_ =
      new base.OneShotTimer(this.reconnect_.bind(this), RECONNECT_DELAY_MS);
};

/**
 * @param {!remoting.Error} reason
 */
remoting.SmartReconnector.prototype.onConnectionDropped = function(reason) {
  this.cancelPending_();
  if (navigator.onLine) {
    this.reconnect_();
  } else {
    this.pending_ = new base.DomEventHook(
        window, 'online', this.reconnectAsync_.bind(this), false);
  }
};

/**
 * @param {boolean=} active  True if the video channel is active.
 */
remoting.SmartReconnector.prototype.videoChannelStateChanged_ =
    function (active) {
  this.cancelPending_();
  if (!active) {
    this.pending_ = new base.DomEventHook(
        window, 'online', this.startReconnectTimeout_.bind(this), false);
  }
};

remoting.SmartReconnector.prototype.startReconnectTimeout_ = function() {
  this.cancelPending_();
  this.pending_ =
      new base.OneShotTimer(this.reconnect_.bind(this), CONNECTION_TIMEOUT_MS);
};

/** @private */
remoting.SmartReconnector.prototype.cancelPending_ = function() {
  base.dispose(this.pending_);
  this.pending_ = null;
};

remoting.SmartReconnector.prototype.dispose = function() {
  this.cancelPending_();
  base.dispose(this.eventHook_);
  this.eventHook_ = null;
};

})();
