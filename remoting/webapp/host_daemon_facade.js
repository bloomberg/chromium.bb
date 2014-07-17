// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class to communicate with the host daemon via Native Messaging.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 */
remoting.HostDaemonFacade = function() {
  /**
   * @type {number}
   * @private
   */
  this.nextId_ = 0;

  /**
   * @type {Object.<number, remoting.HostDaemonFacade.PendingReply>}
   * @private
   */
  this.pendingReplies_ = {};

  /** @type {?chrome.extension.Port} @private */
  this.port_ = null;

  /** @type {string} @private */
  this.version_ = '';

  /** @type {Array.<remoting.HostController.Feature>} @private */
  this.supportedFeatures_ = [];

  /** @type {Array.<function(boolean):void>} @private */
  this.afterInitializationTasks_ = [];

  /** @private */
  this.initializationFinished_ = false;

  /** @type {remoting.Error} @private */
  this.error_ = remoting.Error.NONE;

  try {
    this.port_ = chrome.runtime.connectNative(
        'com.google.chrome.remote_desktop');
    this.port_.onMessage.addListener(this.onIncomingMessage_.bind(this));
    this.port_.onDisconnect.addListener(this.onDisconnect_.bind(this));
    this.postMessage_({type: 'hello'},
                      this.onInitialized_.bind(this, true),
                      this.onInitialized_.bind(this, false));
  } catch (err) {
    console.log('Native Messaging initialization failed: ',
                /** @type {*} */ (err));
    this.onInitialized_(false);
  }
};

/**
 * Type used for entries of |pendingReplies_| list.
 *
 * @param {string} type Type of the originating request.
 * @param {function(...):void} onDone Response callback. Parameters depend on
 *     the request type.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @constructor
 */
remoting.HostDaemonFacade.PendingReply = function(type, onDone, onError) {
  this.type = type;
  this.onDone = onDone;
  this.onError = onError;
};

/**
 * @param {boolean} success
 * @return {void} Nothing.
 * @private
 */
remoting.HostDaemonFacade.prototype.onInitialized_ = function(success) {
  this.initializationFinished_ = true;
  var afterInitializationTasks = this.afterInitializationTasks_;
  this.afterInitializationTasks_ = [];
  for (var id in afterInitializationTasks) {
    afterInitializationTasks[/** @type {number} */(id)](success);
  }
};

/**
 * @param {remoting.HostController.Feature} feature The feature to test for.
 * @param {function(boolean):void} onDone Callback to return result.
 * @return {boolean} True if the implementation supports the named feature.
 */
remoting.HostDaemonFacade.prototype.hasFeature = function(feature, onDone) {
  if (!this.port_) {
    onDone(false);
  } else if (this.initializationFinished_) {
    onDone(this.supportedFeatures_.indexOf(feature) >= 0);
  } else {
    /** @type remoting.HostDaemonFacade */
    var that = this;
    this.afterInitializationTasks_.push(
        /** @param {boolean} success */
        function(success) {
          onDone(that.supportedFeatures_.indexOf(feature) >= 0);
        });
  }
};

/**
 * Attaches a new ID to the supplied message, and posts it to the Native
 * Messaging port, adding |onDone| to the list of pending replies.
 * |message| should have its 'type' field set, and any other fields set
 * depending on the message type.
 *
 * @param {{type: string}} message The message to post.
 * @param {function(...):void} onDone The callback, if any, to be triggered
 *     on response.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 * @private
 */
remoting.HostDaemonFacade.prototype.postMessage_ =
    function(message, onDone, onError) {
  if (!this.port_) {
    onError(this.error_);
    return;
  }
  var id = this.nextId_++;
  message['id'] = id;
  this.pendingReplies_[id] = new remoting.HostDaemonFacade.PendingReply(
    message.type + 'Response', onDone, onError);
  this.port_.postMessage(message);
};

/**
 * Handler for incoming Native Messages.
 *
 * @param {Object} message The received message.
 * @return {void} Nothing.
 * @private
 */
remoting.HostDaemonFacade.prototype.onIncomingMessage_ = function(message) {
  /** @type {number} */
  var id = message['id'];
  if (typeof(id) != 'number') {
    console.error('NativeMessaging: missing or non-numeric id');
    return;
  }
  var reply = this.pendingReplies_[id];
  if (!reply) {
    console.error('NativeMessaging: unexpected id: ', id);
    return;
  }
  delete this.pendingReplies_[id];

  try {
    var type = getStringAttr(message, 'type');
    if (type != reply.type) {
      throw 'Expected reply type: ' + reply.type + ', got: ' + type;
    }

    this.handleIncomingMessage_(message, reply.onDone);
  } catch (e) {
    console.error('Error while processing native message' +
                  /** @type {*} */ (e));
    reply.onError(remoting.Error.UNEXPECTED);
  }
}

/**
 * Handler for incoming Native Messages.
 *
 * @param {Object} message The received message.
 * @param {function(...):void} onDone Function to call when we're done
 *     processing the message.
 * @return {void} Nothing.
 * @private
 */
remoting.HostDaemonFacade.prototype.handleIncomingMessage_ =
    function(message, onDone) {
  var type = getStringAttr(message, 'type');

  switch (type) {
    case 'helloResponse':
      this.version_ = getStringAttr(message, 'version');
      // Old versions of the native messaging host do not return this list.
      // Those versions default to the empty list of supported features.
      this.supportedFeatures_ = getArrayAttr(message, 'supportedFeatures', []);
      onDone();
      break;

    case 'getHostNameResponse':
      onDone(getStringAttr(message, 'hostname'));
      break;

    case 'getPinHashResponse':
      onDone(getStringAttr(message, 'hash'));
      break;

    case 'generateKeyPairResponse':
      var privateKey = getStringAttr(message, 'privateKey');
      var publicKey = getStringAttr(message, 'publicKey');
      onDone(privateKey, publicKey);
      break;

    case 'updateDaemonConfigResponse':
      var result = remoting.HostController.AsyncResult.fromString(
          getStringAttr(message, 'result'));
      onDone(result);
      break;

    case 'getDaemonConfigResponse':
      onDone(getObjectAttr(message, 'config'));
      break;

    case 'getUsageStatsConsentResponse':
      var supported = getBooleanAttr(message, 'supported');
      var allowed = getBooleanAttr(message, 'allowed');
      var setByPolicy = getBooleanAttr(message, 'setByPolicy');
      onDone(supported, allowed, setByPolicy);
      break;

    case 'startDaemonResponse':
    case 'stopDaemonResponse':
      var result = remoting.HostController.AsyncResult.fromString(
          getStringAttr(message, 'result'));
      onDone(result);
      break;

    case 'getDaemonStateResponse':
      var state = remoting.HostController.State.fromString(
        getStringAttr(message, 'state'));
      onDone(state);
      break;

    case 'getPairedClientsResponse':
      var pairedClients = remoting.PairedClient.convertToPairedClientArray(
          message['pairedClients']);
      if (pairedClients != null) {
        onDone(pairedClients);
      } else {
        throw 'No paired clients!';
      }
      break;

    case 'clearPairedClientsResponse':
    case 'deletePairedClientResponse':
      onDone(getBooleanAttr(message, 'result'));
      break;

    case 'getHostClientIdResponse':
      onDone(getStringAttr(message, 'clientId'));
      break;

    case 'getCredentialsFromAuthCodeResponse':
      var userEmail = getStringAttr(message, 'userEmail');
      var refreshToken = getStringAttr(message, 'refreshToken');
      if (userEmail && refreshToken) {
        onDone(userEmail, refreshToken);
      } else {
        throw 'Missing userEmail or refreshToken';
      }
      break;

    default:
      throw 'Unexpected native message: ' + message;
  }
};

/**
 * @return {void} Nothing.
 * @private
 */
remoting.HostDaemonFacade.prototype.onDisconnect_ = function() {
  console.error('Native Message port disconnected');

  this.port_ = null;

  // If initialization hasn't finished then assume that the port was
  // disconnected because Native Messaging host is not installed.
  this.error_ = this.initializationFinished_ ? remoting.Error.UNEXPECTED :
                                               remoting.Error.MISSING_PLUGIN;

  // Notify the error-handlers of any requests that are still outstanding.
  var pendingReplies = this.pendingReplies_;
  this.pendingReplies_ = {};
  for (var id in pendingReplies) {
    pendingReplies[/** @type {number} */(id)].onError(this.error_);
  }
}

/**
 * Gets local hostname.
 *
 * @param {function(string):void} onDone Callback to return result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 */
remoting.HostDaemonFacade.prototype.getHostName =
    function(onDone, onError) {
  this.postMessage_({type: 'getHostName'}, onDone, onError);
};

/**
 * Calculates PIN hash value to be stored in the config, passing the resulting
 * hash value base64-encoded to the callback.
 *
 * @param {string} hostId The host ID.
 * @param {string} pin The PIN.
 * @param {function(string):void} onDone Callback to return result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 */
remoting.HostDaemonFacade.prototype.getPinHash =
    function(hostId, pin, onDone, onError) {
  this.postMessage_({
      type: 'getPinHash',
      hostId: hostId,
      pin: pin
  }, onDone, onError);
};

/**
 * Generates new key pair to use for the host. The specified callback is called
 * when the key is generated. The key is returned in format understood by the
 * host (PublicKeyInfo structure encoded with ASN.1 DER, and then BASE64).
 *
 * @param {function(string, string):void} onDone Callback to return result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 */
remoting.HostDaemonFacade.prototype.generateKeyPair =
    function(onDone, onError) {
  this.postMessage_({type: 'generateKeyPair'}, onDone, onError);
};

/**
 * Updates host config with the values specified in |config|. All
 * fields that are not specified in |config| remain
 * unchanged. Following parameters cannot be changed using this
 * function: host_id, xmpp_login. Error is returned if |config|
 * includes these parameters. Changes take effect before the callback
 * is called.
 *
 * @param {Object} config The new config parameters.
 * @param {function(remoting.HostController.AsyncResult):void} onDone
 *     Callback to be called when finished.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 */
remoting.HostDaemonFacade.prototype.updateDaemonConfig =
    function(config, onDone, onError) {
  this.postMessage_({
      type: 'updateDaemonConfig',
      config: config
  }, onDone, onError);
};

/**
 * Loads daemon config. The config is passed as a JSON formatted string to the
 * callback.
 *
 * @param {function(Object):void} onDone Callback to return result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 */
remoting.HostDaemonFacade.prototype.getDaemonConfig =
    function(onDone, onError) {
  this.postMessage_({type: 'getDaemonConfig'}, onDone, onError);
};

/**
 * Retrieves daemon version. The version is passed to onDone as a dotted decimal
 * string of the form major.minor.build.patch.
 * @param {function(string):void} onDone Callback to be called to return result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void}
 */
remoting.HostDaemonFacade.prototype.getDaemonVersion =
    function(onDone, onError) {
  if (!this.port_) {
    onError(remoting.Error.UNEXPECTED);
  } else if (this.initializationFinished_) {
    onDone(this.version_);
  } else {
    /** @type remoting.HostDaemonFacade */
    var that = this;
    this.afterInitializationTasks_.push(
        /** @param {boolean} success */
        function(success) {
          if (success) {
            onDone(that.version_);
          } else {
            onError(that.error_);
          }
        });
  }
};

/**
 * Get the user's consent to crash reporting. The consent flags are passed to
 * the callback as booleans: supported, allowed, set-by-policy.
 *
 * @param {function(boolean, boolean, boolean):void} onDone Callback to return
 *     result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 */
remoting.HostDaemonFacade.prototype.getUsageStatsConsent =
    function(onDone, onError) {
  this.postMessage_({type: 'getUsageStatsConsent'}, onDone, onError);
};

/**
 * Starts the daemon process with the specified configuration.
 *
 * @param {Object} config Host configuration.
 * @param {boolean} consent Consent to report crash dumps.
 * @param {function(remoting.HostController.AsyncResult):void} onDone
 *     Callback to return result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 */
remoting.HostDaemonFacade.prototype.startDaemon =
    function(config, consent, onDone, onError) {
  this.postMessage_({
      type: 'startDaemon',
      config: config,
      consent: consent
  }, onDone, onError);
};

/**
 * Stops the daemon process.
 *
 * @param {function(remoting.HostController.AsyncResult):void} onDone
 *     Callback to return result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 */
remoting.HostDaemonFacade.prototype.stopDaemon =
    function(onDone, onError) {
  this.postMessage_({type: 'stopDaemon'}, onDone, onError);
};

/**
 * Gets the installed/running state of the Host process.
 *
 * @param {function(remoting.HostController.State):void} onDone Callback to
*      return result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 */
remoting.HostDaemonFacade.prototype.getDaemonState =
    function(onDone, onError) {
  this.postMessage_({type: 'getDaemonState'}, onDone, onError);
}

/**
 * Retrieves the list of paired clients.
 *
 * @param {function(Array.<remoting.PairedClient>):void} onDone Callback to
 *     return result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 */
remoting.HostDaemonFacade.prototype.getPairedClients =
    function(onDone, onError) {
  this.postMessage_({type: 'getPairedClients'}, onDone, onError);
}

/**
 * Clears all paired clients from the registry.
 *
 * @param {function(boolean):void} onDone Callback to be called when finished.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 */
remoting.HostDaemonFacade.prototype.clearPairedClients =
    function(onDone, onError) {
  this.postMessage_({type: 'clearPairedClients'}, onDone, onError);
}

/**
 * Deletes a paired client referenced by client id.
 *
 * @param {string} client Client to delete.
 * @param {function(boolean):void} onDone Callback to be called when finished.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 */
remoting.HostDaemonFacade.prototype.deletePairedClient =
    function(client, onDone, onError) {
  this.postMessage_({
    type: 'deletePairedClient',
    clientId: client
  }, onDone, onError);
}

/**
 * Gets the API keys to obtain/use service account credentials.
 *
 * @param {function(string):void} onDone Callback to return result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 */
remoting.HostDaemonFacade.prototype.getHostClientId =
    function(onDone, onError) {
  this.postMessage_({type: 'getHostClientId'}, onDone, onError);
};

/**
 *
 * @param {string} authorizationCode OAuth authorization code.
 * @param {function(string, string):void} onDone Callback to return result.
 * @param {function(remoting.Error):void} onError Callback to call on error.
 * @return {void} Nothing.
 */
remoting.HostDaemonFacade.prototype.getCredentialsFromAuthCode =
    function(authorizationCode, onDone, onError) {
  this.postMessage_({
    type: 'getCredentialsFromAuthCode',
    authorizationCode: authorizationCode
  }, onDone, onError);
};
