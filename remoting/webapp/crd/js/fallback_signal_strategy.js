// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * A signal strategy encapsulating a primary and a back-up strategy. If the
 * primary fails or times out, then the secondary is used. Information about
 * which strategy was used, and why, is returned via |onProgressCallback|.
 *
 * @param {remoting.SignalStrategy} primary
 * @param {remoting.SignalStrategy} secondary
 *
 * @implements {remoting.SignalStrategy}
 * @constructor
 */
remoting.FallbackSignalStrategy = function(primary,
                                           secondary) {
  /**
   * @type {remoting.SignalStrategy}
   * @private
   */
  this.primary_ = primary;
  this.primary_.setStateChangedCallback(this.onPrimaryStateChanged_.bind(this));

  /**
   * @type {remoting.SignalStrategy}
   * @private
   */
  this.secondary_ = secondary;
  this.secondary_.setStateChangedCallback(
      this.onSecondaryStateChanged_.bind(this));

  /**
   * @type {?function(remoting.SignalStrategy.State)}
   * @private
   */
  this.onStateChangedCallback_ = null;

  /**
   * @type {?function(Element):void}
   * @private
   */
  this.onIncomingStanzaCallback_ = null;

  /**
   * @type {number}
   * @private
   * @const
   */
  this.PRIMARY_CONNECT_TIMEOUT_MS_ = 10 * 1000;

  /**
   * @enum {string}
   * @private
   */
  this.State = {
    NOT_CONNECTED: 'not-connected',
    PRIMARY_PENDING: 'primary-pending',
    PRIMARY_SUCCEEDED: 'primary-succeeded',
    SECONDARY_PENDING: 'secondary-pending',
    SECONDARY_SUCCEEDED: 'secondary-succeeded',
    SECONDARY_FAILED: 'secondary-failed',
    CLOSED: 'closed'
  };

  /**
   * @type {string}
   * @private
   */
  this.state_ = this.State.NOT_CONNECTED;

  /**
   * @type {?remoting.SignalStrategy.State}
   * @private
   */
  this.externalState_ = null;

  /**
   * @type {string}
   * @private
   */
  this.server_ = '';

  /**
   * @type {string}
   * @private
   */
  this.username_ = '';

  /**
   * @type {string}
   * @private
   */
  this.authToken_ = '';

  /**
   * @type {number}
   * @private
   */
  this.primaryConnectTimerId_ = 0;

  /**
   * @type {remoting.LogToServer}
   * @private
   */
  this.logToServer_ = null;

  /**
   * @type {Array<{strategyType: remoting.SignalStrategy.Type,
                    progress: remoting.FallbackSignalStrategy.Progress}>}
   */
  this.connectionSetupResults_ = [];

};

/**
 * @enum {string}
 */
remoting.FallbackSignalStrategy.Progress = {
  SUCCEEDED: 'succeeded',
  FAILED: 'failed',
  TIMED_OUT: 'timed-out',
  SUCCEEDED_LATE: 'succeeded-late',
  FAILED_LATE: 'failed-late',
};

remoting.FallbackSignalStrategy.prototype.dispose = function() {
  this.primary_.dispose();
  this.secondary_.dispose();
};

/**
 * @param {function(remoting.SignalStrategy.State):void} onStateChangedCallback
 *   Callback to call on state change.
 */
remoting.FallbackSignalStrategy.prototype.setStateChangedCallback = function(
    onStateChangedCallback) {
  this.onStateChangedCallback_ = onStateChangedCallback;
};

/**
 * @param {?function(Element):void} onIncomingStanzaCallback Callback to call on
 *     incoming messages.
 */
remoting.FallbackSignalStrategy.prototype.setIncomingStanzaCallback =
    function(onIncomingStanzaCallback) {
  this.onIncomingStanzaCallback_ = onIncomingStanzaCallback;
  if (this.state_ == this.State.PRIMARY_PENDING ||
      this.state_ == this.State.PRIMARY_SUCCEEDED) {
    this.primary_.setIncomingStanzaCallback(onIncomingStanzaCallback);
  } else if (this.state_ == this.State.SECONDARY_PENDING ||
             this.state_ == this.State.SECONDARY_SUCCEEDED) {
    this.secondary_.setIncomingStanzaCallback(onIncomingStanzaCallback);
  }
};

/**
 * @param {string} server
 * @param {string} username
 * @param {string} authToken
 */
remoting.FallbackSignalStrategy.prototype.connect =
    function(server, username, authToken) {
  base.debug.assert(this.state_ == this.State.NOT_CONNECTED);
  base.debug.assert(this.onStateChangedCallback_ != null);
  this.server_ = server;
  this.username_ = username;
  this.authToken_ = authToken;
  this.state_ = this.State.PRIMARY_PENDING;
  this.primary_.setIncomingStanzaCallback(this.onIncomingStanzaCallback_);
  this.primary_.connect(server, username, authToken);
  this.primaryConnectTimerId_ =
      window.setTimeout(this.onPrimaryTimeout_.bind(this),
                        this.PRIMARY_CONNECT_TIMEOUT_MS_);
};

/**
 * Sends a message. Can be called only in CONNECTED state.
 * @param {string} message
 */
remoting.FallbackSignalStrategy.prototype.sendMessage = function(message) {
  this.getConnectedSignalStrategy_().sendMessage(message);
};

/**
 * Send any messages accumulated during connection set-up.
 *
 * @param {remoting.LogToServer} logToServer The LogToServer instance for the
 *     connection.
 */
remoting.FallbackSignalStrategy.prototype.sendConnectionSetupResults =
    function(logToServer) {
  this.logToServer_ = logToServer;
  this.sendConnectionSetupResultsInternal_();
}

remoting.FallbackSignalStrategy.prototype.sendConnectionSetupResultsInternal_ =
    function() {
  for (var i = 0; i < this.connectionSetupResults_.length; ++i) {
    var result = this.connectionSetupResults_[i];
    this.logToServer_.logSignalStrategyProgress(result.strategyType,
                                                result.progress);
  }
  this.connectionSetupResults_ = [];
};

/** @return {remoting.SignalStrategy.State} Current state */
remoting.FallbackSignalStrategy.prototype.getState = function() {
  return (this.externalState_ === null)
      ? remoting.SignalStrategy.State.NOT_CONNECTED
      : this.externalState_;
};

/** @return {remoting.Error} Error when in FAILED state. */
remoting.FallbackSignalStrategy.prototype.getError = function() {
  base.debug.assert(this.state_ == this.State.SECONDARY_FAILED);
  base.debug.assert(
      this.secondary_.getState() == remoting.SignalStrategy.State.FAILED);
  return this.secondary_.getError();
};

/** @return {string} Current JID when in CONNECTED state. */
remoting.FallbackSignalStrategy.prototype.getJid = function() {
  return this.getConnectedSignalStrategy_().getJid();
};

/** @return {remoting.SignalStrategy.Type} The signal strategy type. */
remoting.FallbackSignalStrategy.prototype.getType = function() {
  return this.getConnectedSignalStrategy_().getType();
};

/**
 * @return {remoting.SignalStrategy} The active signal strategy, if the
 *     connection has succeeded.
 * @private
 */
remoting.FallbackSignalStrategy.prototype.getConnectedSignalStrategy_ =
    function() {
  if (this.state_ == this.State.PRIMARY_SUCCEEDED) {
    base.debug.assert(
        this.primary_.getState() == remoting.SignalStrategy.State.CONNECTED);
    return this.primary_;
  } else if (this.state_ == this.State.SECONDARY_SUCCEEDED) {
    base.debug.assert(
        this.secondary_.getState() == remoting.SignalStrategy.State.CONNECTED);
    return this.secondary_;
  } else {
    base.debug.assert(
        false,
        'getConnectedSignalStrategy called in unconnected state');
    return null;
  }
};

/**
 * @param {remoting.SignalStrategy.State} state
 * @private
 */
remoting.FallbackSignalStrategy.prototype.onPrimaryStateChanged_ =
    function(state) {
  switch (state) {
    case remoting.SignalStrategy.State.CONNECTED:
      if (this.state_ == this.State.PRIMARY_PENDING) {
        window.clearTimeout(this.primaryConnectTimerId_);
        this.updateProgress_(
            this.primary_,
            remoting.FallbackSignalStrategy.Progress.SUCCEEDED);
        this.state_ = this.State.PRIMARY_SUCCEEDED;
      } else {
        this.updateProgress_(
            this.primary_,
            remoting.FallbackSignalStrategy.Progress.SUCCEEDED_LATE);
      }
      break;

    case remoting.SignalStrategy.State.FAILED:
      if (this.state_ == this.State.PRIMARY_PENDING) {
        window.clearTimeout(this.primaryConnectTimerId_);
        this.updateProgress_(
            this.primary_,
            remoting.FallbackSignalStrategy.Progress.FAILED);
        this.connectSecondary_();
      } else {
        this.updateProgress_(
            this.primary_,
            remoting.FallbackSignalStrategy.Progress.FAILED_LATE);
      }
      return;  // Don't notify the external callback

    case remoting.SignalStrategy.State.CLOSED:
      this.state_ = this.State.CLOSED;
      break;
  }

  this.notifyExternalCallback_(state);
};

/**
 * @param {remoting.SignalStrategy.State} state
 * @private
 */
remoting.FallbackSignalStrategy.prototype.onSecondaryStateChanged_ =
    function(state) {
  switch (state) {
    case remoting.SignalStrategy.State.CONNECTED:
      this.updateProgress_(
          this.secondary_,
          remoting.FallbackSignalStrategy.Progress.SUCCEEDED);
      this.state_ = this.State.SECONDARY_SUCCEEDED;
      break;

    case remoting.SignalStrategy.State.FAILED:
      this.updateProgress_(
          this.secondary_,
          remoting.FallbackSignalStrategy.Progress.FAILED);
      this.state_ = this.State.SECONDARY_FAILED;
      break;

    case remoting.SignalStrategy.State.CLOSED:
      this.state_ = this.State.CLOSED;
      break;
  }

  this.notifyExternalCallback_(state);
};

/**
 * Notify the external callback of a change in state if it's consistent with
 * the allowed state transitions (ie, if it represents a later stage in the
 * connection process). Suppress state transitions that would violate this,
 * for example a CONNECTING -> NOT_CONNECTED transition when we switch from
 * the primary to the secondary signal strategy.
 *
 * @param {remoting.SignalStrategy.State} state
 * @private
 */
remoting.FallbackSignalStrategy.prototype.notifyExternalCallback_ =
    function(state) {
  if (this.externalState_ === null || state > this.externalState_) {
    this.externalState_ = state;
    this.onStateChangedCallback_(state);
  }
};

/**
 * @private
 */
remoting.FallbackSignalStrategy.prototype.connectSecondary_ = function() {
  base.debug.assert(this.state_ == this.State.PRIMARY_PENDING);
  base.debug.assert(this.server_ != '');
  base.debug.assert(this.username_ != '');
  base.debug.assert(this.authToken_ != '');

  this.state_ = this.State.SECONDARY_PENDING;
  this.primary_.setIncomingStanzaCallback(null);
  this.secondary_.setIncomingStanzaCallback(this.onIncomingStanzaCallback_);
  this.secondary_.connect(this.server_, this.username_, this.authToken_);
};

/**
 * @private
 */
remoting.FallbackSignalStrategy.prototype.onPrimaryTimeout_ = function() {
  this.updateProgress_(
      this.primary_,
      remoting.FallbackSignalStrategy.Progress.TIMED_OUT);
  this.connectSecondary_();
};

/**
 * @param {remoting.SignalStrategy} strategy
 * @param {remoting.FallbackSignalStrategy.Progress} progress
 * @private
 */
remoting.FallbackSignalStrategy.prototype.updateProgress_ = function(
    strategy, progress) {
  console.log('FallbackSignalStrategy progress: ' + strategy.getType() + ' ' +
      progress);
  this.connectionSetupResults_.push({
    'strategyType': strategy.getType(),
    'progress': progress
  });
  if (this.logToServer_) {
    this.sendConnectionSetupResultsInternal_();
  }
};
