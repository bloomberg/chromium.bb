// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This class provides an interface between the HostController and either the
 * NativeMessaging Host.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 */
remoting.HostDispatcher = function() {
  /** @type {remoting.HostNativeMessaging} @private */
  this.nativeMessagingHost_ = new remoting.HostNativeMessaging();

  /** @type {remoting.HostDispatcher.State} @private */
  this.state_ = remoting.HostDispatcher.State.UNKNOWN;

  /** @type {Array.<function()>} @private */
  this.pendingRequests_ = [];

  this.tryToInitialize_();
}

/** @enum {number} */
remoting.HostDispatcher.State = {
  UNKNOWN: 0,
  NATIVE_MESSAGING: 1,
  NOT_INSTALLED: 2
};

remoting.HostDispatcher.prototype.tryToInitialize_ = function() {
  /** @type {remoting.HostDispatcher} */
  var that = this;

  if (this.state_ != remoting.HostDispatcher.State.UNKNOWN)
    return;

  function sendPendingRequests() {
    var pendingRequests = that.pendingRequests_;
    that.pendingRequests_ = [];
    for (var i = 0; i < pendingRequests.length; i++) {
      pendingRequests[i]();
    }
  }

  function onNativeMessagingInit() {
    that.state_ = remoting.HostDispatcher.State.NATIVE_MESSAGING;
    sendPendingRequests();
  }

  function onNativeMessagingFailed(error) {
    that.state_ = remoting.HostDispatcher.State.NOT_INSTALLED;
    sendPendingRequests();
  }

  this.nativeMessagingHost_.initialize(onNativeMessagingInit,
                                       onNativeMessagingFailed);
};

/**
 * @param {remoting.HostController.Feature} feature The feature to test for.
 * @param {function(boolean):void} onDone
 * @return {void}
 */
remoting.HostDispatcher.prototype.hasFeature = function(
    feature, onDone) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.hasFeature.bind(this, feature, onDone));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      onDone(this.nativeMessagingHost_.hasFeature(feature));
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onDone(false);
      break;
  }
};

/**
 * @param {function(string):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getHostName = function(onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getHostName.bind(this, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getHostName(onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * @param {string} hostId
 * @param {string} pin
 * @param {function(string):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getPinHash =
    function(hostId, pin, onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getPinHash.bind(this, hostId, pin, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getPinHash(hostId, pin, onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * @param {function(string, string):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.generateKeyPair = function(onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.generateKeyPair.bind(this, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.generateKeyPair(onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * @param {Object} config
 * @param {function(remoting.HostController.AsyncResult):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.updateDaemonConfig =
    function(config, onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.updateDaemonConfig.bind(this, config, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.updateDaemonConfig(config, onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * @param {function(Object):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getDaemonConfig = function(onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getDaemonConfig.bind(this, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getDaemonConfig(onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onDone({});
      break;
  }
};

/**
 * @param {function(string):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getDaemonVersion = function(onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getDaemonVersion.bind(this, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      onDone(this.nativeMessagingHost_.getDaemonVersion());
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * @param {function(boolean, boolean, boolean):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getUsageStatsConsent =
    function(onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getUsageStatsConsent.bind(this, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getUsageStatsConsent(onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * @param {Object} config
 * @param {boolean} consent
 * @param {function(remoting.HostController.AsyncResult):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.startDaemon =
    function(config, consent, onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.startDaemon.bind(this, config, consent, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.startDaemon(config, consent, onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * @param {function(remoting.HostController.AsyncResult):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.stopDaemon = function(onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(this.stopDaemon.bind(this, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.stopDaemon(onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * @param {function(remoting.HostController.State):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getDaemonState = function(onDone, onError) {
  // If the host was in not-initialized state try initializing it again in case
  // it was installed.
  if (this.state_ == remoting.HostDispatcher.State.NOT_INSTALLED) {
    this.state_ = remoting.HostDispatcher.State.UNKNOWN;
    this.tryToInitialize_();
  }

  this.getDaemonStateInternal_(onDone, onError);
}

/**
 * @param {function(remoting.HostController.State):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getDaemonStateInternal_ =
    function(onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getDaemonStateInternal_.bind(this, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getDaemonState(onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onDone(remoting.HostController.State.NOT_INSTALLED);
      break;
  }
};

/**
 * @param {function(Array.<remoting.PairedClient>):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getPairedClients = function(onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getPairedClients.bind(this, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getPairedClients(onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * The pairing API returns a boolean to indicate success or failure, but
 * the JS API is defined in terms of onDone and onError callbacks. This
 * function converts one to the other.
 *
 * @param {function():void} onDone Success callback.
 * @param {function(remoting.Error):void} onError Error callback.
 * @param {boolean} success True if the operation succeeded; false otherwise.
 * @private
 */
remoting.HostDispatcher.runCallback_ = function(onDone, onError, success) {
  if (success) {
    onDone();
  } else {
    onError(remoting.Error.UNEXPECTED);
  }
};

/**
 * @param {function():void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.clearPairedClients =
    function(onDone, onError) {
  var callback =
      remoting.HostDispatcher.runCallback_.bind(null, onDone, onError);
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
        this.clearPairedClients.bind(this, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.clearPairedClients(callback, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * @param {string} client
 * @param {function():void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.deletePairedClient =
    function(client, onDone, onError) {
  var callback =
      remoting.HostDispatcher.runCallback_.bind(null, onDone, onError);
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
        this.deletePairedClient.bind(this, client, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.deletePairedClient(client, callback, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * @param {function(string):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getHostClientId =
    function(onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getHostClientId.bind(this, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getHostClientId(onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};

/**
 * @param {string} authorizationCode
 * @param {function(string, string):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getCredentialsFromAuthCode =
    function(authorizationCode, onDone, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getCredentialsFromAuthCode.bind(
              this, authorizationCode, onDone, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getCredentialsFromAuthCode(
          authorizationCode, onDone, onError);
      break;
    case remoting.HostDispatcher.State.NOT_INSTALLED:
      onError(remoting.Error.MISSING_PLUGIN);
      break;
  }
};
