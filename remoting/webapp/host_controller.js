// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor */
remoting.HostController = function() {
  /** @return {remoting.HostPlugin} */
  var createNpapiPlugin = function() {
    var plugin = remoting.HostSession.createPlugin();
    /** @type {HTMLElement} @private */
    var container = document.getElementById('daemon-plugin-container');
    container.appendChild(plugin);
    return plugin;
  }

  /** @type {remoting.HostDispatcher} @private */
  this.hostDispatcher_ = new remoting.HostDispatcher(createNpapiPlugin);

  /** @param {string} version */
  var printVersion = function(version) {
    if (version == '') {
      console.log('Host not installed.');
    } else {
      console.log('Host version: ' + version);
    }
  };

  this.hostDispatcher_.getDaemonVersion(printVersion, function() {
    console.log('Host version not available.');
  });
}

// Note that the values in the enums below are copied from
// daemon_controller.h and must be kept in sync.
/** @enum {number} */
remoting.HostController.State = {
  NOT_IMPLEMENTED: -1,
  NOT_INSTALLED: 0,
  INSTALLING: 1,
  STOPPED: 2,
  STARTING: 3,
  STARTED: 4,
  STOPPING: 5,
  UNKNOWN: 6
};

/** @enum {number} */
remoting.HostController.AsyncResult = {
  OK: 0,
  FAILED: 1,
  CANCELLED: 2,
  FAILED_DIRECTORY: 3
};

/**
 * Set of features for which hasFeature() can be used to test.
 *
 * @enum {string}
 */
remoting.HostController.Feature = {
  PAIRING_REGISTRY: 'pairingRegistry'
};

/**
 * @param {remoting.HostController.Feature} feature The feature to test for.
 * @param {function(boolean):void} callback
 * @return {void}
 */
remoting.HostController.prototype.hasFeature = function(feature, callback) {
  // TODO(rmsousa): This could synchronously return a boolean, provided it were
  // only called after the dispatcher is completely initialized.
  this.hostDispatcher_.hasFeature(feature, callback);
};

/**
 * @param {function(boolean, boolean, boolean):void} onDone Callback to be
 *     called when done.
 * @param {function(remoting.Error):void} onError Callback to be called on
 *     error.
 */
remoting.HostController.prototype.getConsent = function(onDone, onError) {
  this.hostDispatcher_.getUsageStatsConsent(onDone, onError);
};

/**
 * Registers and starts the host.
 *
 * @param {string} hostPin Host PIN.
 * @param {boolean} consent The user's consent to crash dump reporting.
 * @param {function():void} onDone Callback to be called when done.
 * @param {function(remoting.Error):void} onError Callback to be called on
 *     error.
 * @return {void} Nothing.
 */
remoting.HostController.prototype.start = function(hostPin, consent, onDone,
                                                   onError) {
  /** @type {remoting.HostController} */
  var that = this;

  /** @return {string} */
  function generateUuid() {
    var random = new Uint16Array(8);
    window.crypto.getRandomValues(random);
    /** @type {Array.<string>} */
    var e = new Array();
    for (var i = 0; i < 8; i++) {
      e[i] = (/** @type {number} */random[i] + 0x10000).
          toString(16).substring(1);
    }
    return e[0] + e[1] + '-' + e[2] + '-' + e[3] + '-' +
        e[4] + '-' + e[5] + e[6] + e[7];
  };

  var newHostId = generateUuid();

  /** @param {remoting.Error} error */
  function onStartError(error) {
    // Unregister the host if we failed to start it.
    remoting.HostList.unregisterHostById(newHostId);
    onError(error);
  }

  /**
   * @param {string} hostName
   * @param {string} publicKey
   * @param {remoting.HostController.AsyncResult} result
   */
  function onStarted(hostName, publicKey, result) {
    if (result == remoting.HostController.AsyncResult.OK) {
      remoting.hostList.onLocalHostStarted(hostName, newHostId, publicKey);
      onDone();
    } else if (result == remoting.HostController.AsyncResult.CANCELLED) {
      onStartError(remoting.Error.CANCELLED);
    } else {
      onStartError(remoting.Error.UNEXPECTED);
    }
  }

  /**
   * @param {string} hostName
   * @param {string} publicKey
   * @param {string} privateKey
   * @param {XMLHttpRequest} xhr
   * @param {string} hostSecretHash
   */
  function startHostWithHash(hostName, publicKey, privateKey, xhr,
                             hostSecretHash) {
    var hostConfig = {
        xmpp_login: remoting.identity.getCachedEmail(),
        oauth_refresh_token: remoting.oauth2.exportRefreshToken(),
        host_id: newHostId,
        host_name: hostName,
        host_secret_hash: hostSecretHash,
        private_key: privateKey
    };
    that.hostDispatcher_.startDaemon(hostConfig, consent,
                                     onStarted.bind(null, hostName, publicKey),
                                     onStartError);
  }

  /**
   * @param {string} hostName
   * @param {string} publicKey
   * @param {string} privateKey
   * @param {XMLHttpRequest} xhr
   */
  function onRegistered(hostName, publicKey, privateKey, xhr) {
    var success = (xhr.status == 200);

    if (success) {
      that.hostDispatcher_.getPinHash(newHostId, hostPin,
          startHostWithHash.bind(null, hostName, publicKey, privateKey, xhr),
          onError);
    } else {
      console.log('Failed to register the host. Status: ' + xhr.status +
                  ' response: ' + xhr.responseText);
      onError(remoting.Error.REGISTRATION_FAILED);
    }
  };

  /**
   * @param {string} hostName
   * @param {string} privateKey
   * @param {string} publicKey
   * @param {string} oauthToken
   */
  function doRegisterHost(hostName, privateKey, publicKey, oauthToken) {
    var headers = {
      'Authorization': 'OAuth ' + oauthToken,
      'Content-type' : 'application/json; charset=UTF-8'
    };

    var newHostDetails = { data: {
       hostId: newHostId,
       hostName: hostName,
       publicKey: publicKey
    } };
    remoting.xhr.post(
        remoting.settings.DIRECTORY_API_BASE_URL + '/@me/hosts/',
        onRegistered.bind(null, hostName, publicKey, privateKey),
        JSON.stringify(newHostDetails),
        headers);
  };

  /**
   * @param {string} hostName
   * @param {string} privateKey
   * @param {string} publicKey
   */
  function onKeyGenerated(hostName, privateKey, publicKey) {
    remoting.identity.callWithToken(
        doRegisterHost.bind(null, hostName, privateKey, publicKey),
        onError);
  };

  /**
   * @param {string} hostName
   * @return {void} Nothing.
   */
  function startWithHostname(hostName) {
    that.hostDispatcher_.generateKeyPair(onKeyGenerated.bind(null, hostName),
                                         onError);
  }

  this.hostDispatcher_.getHostName(startWithHostname, onError);
};

/**
 * Stop the daemon process.
 * @param {function():void} onDone Callback to be called when done.
 * @param {function(remoting.Error):void} onError Callback to be called on
 *     error.
 * @return {void} Nothing.
 */
remoting.HostController.prototype.stop = function(onDone, onError) {
  /** @type {remoting.HostController} */
  var that = this;

  /** @param {string?} hostId The host id of the local host. */
  function unregisterHost(hostId) {
    if (hostId) {
      remoting.HostList.unregisterHostById(hostId);
    }
    onDone();
  };

  /** @param {remoting.HostController.AsyncResult} result */
  function onStopped(result) {
    if (result == remoting.HostController.AsyncResult.OK) {
      that.getLocalHostId(unregisterHost);
    } else if (result == remoting.HostController.AsyncResult.CANCELLED) {
      onError(remoting.Error.CANCELLED);
    } else {
      onError(remoting.Error.UNEXPECTED);
    }
  }

  this.hostDispatcher_.stopDaemon(onStopped, onError);
};

/**
 * Check the host configuration is valid (non-null, and contains both host_id
 * and xmpp_login keys).
 * @param {Object} config The host configuration.
 * @return {boolean} True if it is valid.
 */
function isHostConfigValid_(config) {
  return !!config && typeof config['host_id'] == 'string' &&
      typeof config['xmpp_login'] == 'string';
}

/**
 * @param {string} newPin The new PIN to set
 * @param {function():void} onDone Callback to be called when done.
 * @param {function(remoting.Error):void} onError Callback to be called on
 *     error.
 * @return {void} Nothing.
 */
remoting.HostController.prototype.updatePin = function(newPin, onDone,
                                                       onError) {
  /** @type {remoting.HostController} */
  var that = this;

  /** @param {remoting.HostController.AsyncResult} result */
  function onConfigUpdated(result) {
    if (result == remoting.HostController.AsyncResult.OK) {
      onDone();
    } else if (result == remoting.HostController.AsyncResult.CANCELLED) {
      onError(remoting.Error.CANCELLED);
    } else {
      onError(remoting.Error.UNEXPECTED);
    }
  }

  /** @param {string} pinHash */
  function updateDaemonConfigWithHash(pinHash) {
    var newConfig = {
      host_secret_hash: pinHash
    };
    that.hostDispatcher_.updateDaemonConfig(newConfig, onConfigUpdated,
                                            onError);
  }

  /** @param {Object} config */
  function onConfig(config) {
    if (!isHostConfigValid_(config)) {
      onError(remoting.Error.UNEXPECTED);
      return;
    }
    /** @type {string} */
    var hostId = config['host_id'];
    that.hostDispatcher_.getPinHash(hostId, newPin, updateDaemonConfigWithHash,
                                    onError);
  }

  // TODO(sergeyu): When crbug.com/121518 is fixed: replace this call
  // with an unprivileged version if that is necessary.
  this.hostDispatcher_.getDaemonConfig(onConfig, onError);
};

/**
 * Get the state of the local host.
 *
 * @param {function(remoting.HostController.State):void} onDone Completion
 *     callback.
 */
remoting.HostController.prototype.getLocalHostState = function(onDone) {
  this.hostDispatcher_.getDaemonState(onDone, function() {
    onDone(remoting.HostController.State.NOT_IMPLEMENTED);
  });
};

/**
 * Get the id of the local host, or null if it is not registered.
 *
 * @param {function(string?):void} onDone Completion callback.
 */
remoting.HostController.prototype.getLocalHostId = function(onDone) {
  /** @type {remoting.HostController} */
  var that = this;
  /** @param {Object} config */
  function onConfig(config) {
    var hostId = null;
    if (isHostConfigValid_(config)) {
      hostId = /** @type {string} */ config['host_id'];
    }
    onDone(hostId);
  };

  this.hostDispatcher_.getDaemonConfig(onConfig, function(error) {
    onDone(null);
  });
};

/**
 * Fetch the list of paired clients for this host.
 *
 * @param {function(Array.<remoting.PairedClient>):void} onDone
 * @param {function(remoting.Error):void} onError
 * @return {void}
 */
remoting.HostController.prototype.getPairedClients = function(onDone,
                                                              onError) {
  this.hostDispatcher_.getPairedClients(onDone, onError);
};

/**
 * Delete a single paired client.
 *
 * @param {string} client The client id of the pairing to delete.
 * @param {function():void} onDone Completion callback.
 * @param {function(remoting.Error):void} onError Error callback.
 * @return {void}
 */
remoting.HostController.prototype.deletePairedClient = function(
    client, onDone, onError) {
  this.hostDispatcher_.deletePairedClient(client, onDone, onError);
};

/**
 * Delete all paired clients.
 *
 * @param {function():void} onDone Completion callback.
 * @param {function(remoting.Error):void} onError Error callback.
 * @return {void}
 */
remoting.HostController.prototype.clearPairedClients = function(
    onDone, onError) {
  this.hostDispatcher_.clearPairedClients(onDone, onError);
};

/** @type {remoting.HostController} */
remoting.hostController = null;
