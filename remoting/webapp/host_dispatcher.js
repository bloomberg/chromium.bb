// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This class provides an interface between the HostController and either the
 * NativeMessaging Host or the Host NPAPI plugin, depending on whether or not
 * NativeMessaging is supported. Since the test for NativeMessaging support is
 * asynchronous, this class stores any requests on a queue, pending the result
 * of the test.
 * Once the test is complete, the pending requests are performed on either the
 * NativeMessaging host, or the NPAPI plugin.
 *
 * If necessary, the HostController is instructed (via a callback) to
 * instantiate the NPAPI plugin, and return a reference to it here.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @param {function():remoting.HostPlugin} createPluginCallback Callback to
 *     instantiate the NPAPI plugin when NativeMessaging is determined to be
 *     unsupported.
 */
remoting.HostDispatcher = function(createPluginCallback) {
  /** @type {remoting.HostDispatcher} */
  var that = this;

  /** @type {remoting.HostNativeMessaging} @private */
  this.nativeMessagingHost_ = new remoting.HostNativeMessaging();

  /** @type {remoting.HostPlugin} @private */
  this.npapiHost_ = null;

  /** @type {remoting.HostDispatcher.State} */
  this.state_ = remoting.HostDispatcher.State.UNKNOWN;

  /** @type {Array.<function()>} */
  this.pendingRequests_ = [];

  function sendPendingRequests() {
    for (var i = 0; i < that.pendingRequests_.length; i++) {
      that.pendingRequests_[i]();
    }
    that.pendingRequests_ = null;
  }

  function onNativeMessagingInit() {
    console.log('Native Messaging supported.');
    that.state_ = remoting.HostDispatcher.State.NATIVE_MESSAGING;
    sendPendingRequests();
  }

  function onNativeMessagingFailed(error) {
    console.log('Native Messaging unsupported, falling back to NPAPI.');
    that.npapiHost_ = createPluginCallback();
    that.state_ = remoting.HostDispatcher.State.NPAPI;
    sendPendingRequests();
  }

  this.nativeMessagingHost_.initialize(onNativeMessagingInit,
                                       onNativeMessagingFailed);
};

/** @enum {number} */
remoting.HostDispatcher.State = {
  UNKNOWN: 0,
  NATIVE_MESSAGING: 1,
  NPAPI: 2
};

/**
 * @param {function(string):void} callback
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getHostName = function(callback, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getHostName.bind(this, callback, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getHostName(callback, onError);
      break;
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.getHostName(callback);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
      break;
  }
};

/**
 * @param {string} hostId
 * @param {string} pin
 * @param {function(string):void} callback
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getPinHash = function(hostId, pin, callback,
                                                        onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getPinHash.bind(this, hostId, pin, callback, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getPinHash(hostId, pin, callback, onError);
      break;
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.getPinHash(hostId, pin, callback);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
      break;
  }
};

/**
 * @param {function(string, string):void} callback
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.generateKeyPair = function(callback,
                                                             onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.generateKeyPair.bind(this, callback, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.generateKeyPair(callback, onError);
      break;
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.generateKeyPair(callback);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
      break;
  }
};

/**
 * @param {Object} config
 * @param {function(remoting.HostController.AsyncResult):void} callback
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.updateDaemonConfig = function(config,
                                                                callback,
                                                                onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.updateDaemonConfig.bind(this, config, callback, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.updateDaemonConfig(config, callback, onError);
      break;
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.updateDaemonConfig(JSON.stringify(config), callback);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
      break;
  }
};

/**
 * @param {function(Object):void} callback
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getDaemonConfig = function(callback,
                                                             onError) {
  /**
   * Converts the config string from the NPAPI plugin to an Object, to pass to
   * |callback|.
   * @param {string} configStr
   * @return {void}
   */
  function callbackForNpapi(configStr) {
    var config = jsonParseSafe(configStr);
    if (typeof(config) != 'object') {
      onError(remoting.Error.UNEXPECTED);
    } else {
      callback(/** @type {Object} */ (config));
    }
  }

  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getDaemonConfig.bind(this, callback, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getDaemonConfig(callback, onError);
      break;
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.getDaemonConfig(callbackForNpapi);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
      break;
  }
};

/**
 * @param {function(string):void} callback
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getDaemonVersion = function(callback,
                                                              onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getDaemonVersion.bind(this, callback, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      callback(this.nativeMessagingHost_.getDaemonVersion());
      break;
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.getDaemonVersion(callback);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
      break;
  }
};

/**
 * @param {function(boolean, boolean, boolean):void} callback
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getUsageStatsConsent = function(callback,
                                                                  onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getUsageStatsConsent.bind(this, callback, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getUsageStatsConsent(callback, onError);
      break;
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.getUsageStatsConsent(callback);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
      break;
  }
};

/**
 * @param {Object} config
 * @param {boolean} consent
 * @param {function(remoting.HostController.AsyncResult):void} callback
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.startDaemon = function(config, consent,
                                                         callback, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.startDaemon.bind(this, config, consent, callback, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.startDaemon(config, consent, callback, onError);
      break;
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.startDaemon(JSON.stringify(config), consent, callback);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
      break;
  }
};

/**
 * @param {function(remoting.HostController.AsyncResult):void} callback
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.stopDaemon = function(callback, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(this.stopDaemon.bind(this, callback, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.stopDaemon(callback, onError);
      break;
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.stopDaemon(callback);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
      break;
  }
};

/**
 * @param {function(remoting.HostController.State):void} callback
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getDaemonState = function(callback, onError) {
  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getDaemonState.bind(this, callback, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getDaemonState(callback, onError);
      break;
    case remoting.HostDispatcher.State.NPAPI:
      // Call the callback directly, since NPAPI exposes the state directly as
      // a field member, rather than an asynchronous method.
      var state = this.npapiHost_.daemonState;
      if (state === undefined) {
        onError(remoting.Error.MISSING_PLUGIN);
      } else {
        callback(state);
      }
      break;
  }
};

/**
 * @param {function(Array.<remoting.PairedClient>):void} callback
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostDispatcher.prototype.getPairedClients = function(callback,
                                                              onError) {
  /**
   * Converts the JSON string from the NPAPI plugin to Array.<PairedClient>, to
   * pass to |callback|.
   * @param {string} pairedClientsJson
   * @return {void}
   */
  function callbackForNpapi(pairedClientsJson) {
    var pairedClients = remoting.PairedClient.convertToPairedClientArray(
        jsonParseSafe(pairedClientsJson));
    if (pairedClients != null) {
      callback(pairedClients);
    } else {
      onError(remoting.Error.UNEXPECTED);
    }
  }

  switch (this.state_) {
    case remoting.HostDispatcher.State.UNKNOWN:
      this.pendingRequests_.push(
          this.getPairedClients.bind(this, callback, onError));
      break;
    case remoting.HostDispatcher.State.NATIVE_MESSAGING:
      this.nativeMessagingHost_.getPairedClients(callback, onError);
      break;
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.getPairedClients(callbackForNpapi);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
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
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.clearPairedClients(callback);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
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
    case remoting.HostDispatcher.State.NPAPI:
      try {
        this.npapiHost_.deletePairedClient(client, callback);
      } catch (err) {
        onError(remoting.Error.MISSING_PLUGIN);
      }
      break;
  }
};
