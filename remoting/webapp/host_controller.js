// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor */
remoting.HostController = function() {
  this.hostDaemonFacade_ = this.createDaemonFacade_();
};

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

/**
 * @param {string} state The host controller state name.
 * @return {remoting.HostController.State} The state enum value.
 */
remoting.HostController.State.fromString = function(state) {
  if (!remoting.HostController.State.hasOwnProperty(state)) {
    throw "Invalid HostController.State: " + state;
  }
  return remoting.HostController.State[state];
}

/** @enum {number} */
remoting.HostController.AsyncResult = {
  OK: 0,
  FAILED: 1,
  CANCELLED: 2,
  FAILED_DIRECTORY: 3
};

/**
 * @param {string} result The async result name.
 * @return {remoting.HostController.AsyncResult} The result enum value.
 */
remoting.HostController.AsyncResult.fromString = function(result) {
  if (!remoting.HostController.AsyncResult.hasOwnProperty(result)) {
    throw "Invalid HostController.AsyncResult: " + result;
  }
  return remoting.HostController.AsyncResult[result];
}

/**
 * @return {remoting.HostDaemonFacade}
 * @private
 */
remoting.HostController.prototype.createDaemonFacade_ = function() {
  /** @type {remoting.HostDaemonFacade} @private */
  var hostDaemonFacade = new remoting.HostDaemonFacade();

  /** @param {string} version */
  var printVersion = function(version) {
    if (version == '') {
      console.log('Host not installed.');
    } else {
      console.log('Host version: ' + version);
    }
  };

  hostDaemonFacade.getDaemonVersion(printVersion, function() {
    console.log('Host version not available.');
  });

  return hostDaemonFacade;
};

/**
 * Set of features for which hasFeature() can be used to test.
 *
 * @enum {string}
 */
remoting.HostController.Feature = {
  PAIRING_REGISTRY: 'pairingRegistry',
  OAUTH_CLIENT: 'oauthClient'
};

/**
 * @param {remoting.HostController.Feature} feature The feature to test for.
 * @param {function(boolean):void} callback
 * @return {void}
 */
remoting.HostController.prototype.hasFeature = function(feature, callback) {
  // TODO(rmsousa): This could synchronously return a boolean, provided it were
  // only called after native messaging is completely initialized.
  this.hostDaemonFacade_.hasFeature(feature, callback);
};

/**
 * @param {function(boolean, boolean, boolean):void} onDone Callback to be
 *     called when done.
 * @param {function(remoting.Error):void} onError Callback to be called on
 *     error.
 */
remoting.HostController.prototype.getConsent = function(onDone, onError) {
  this.hostDaemonFacade_.getUsageStatsConsent(onDone, onError);
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
   * @param {string} xmppLogin
   * @param {string} refreshToken
   * @param {string} clientBaseJid
   * @param {string} hostSecretHash
   */
  function startHostWithHash(hostName, publicKey, privateKey, xmppLogin,
                             refreshToken, clientBaseJid, hostSecretHash) {
    var hostConfig = {
      xmpp_login: xmppLogin,
      oauth_refresh_token: refreshToken,
      host_id: newHostId,
      host_name: hostName,
      host_secret_hash: hostSecretHash,
      private_key: privateKey
    };
    var hostOwner = clientBaseJid;
    var hostOwnerEmail = remoting.identity.getCachedEmail();
    if (hostOwner != xmppLogin) {
      hostConfig['host_owner'] = hostOwner;
      if (hostOwnerEmail != hostOwner) {
        hostConfig['host_owner_email'] = hostOwnerEmail;
      }
    }
    that.hostDaemonFacade_.startDaemon(
        hostConfig, consent, onStarted.bind(null, hostName, publicKey),
        onStartError);
  }

  /**
   * @param {string} hostName
   * @param {string} publicKey
   * @param {string} privateKey
   * @param {string} email
   * @param {string} refreshToken
   * @param {string} clientBaseJid
   */
  function onClientBaseJid(
      hostName, publicKey, privateKey, email, refreshToken, clientBaseJid) {
    that.hostDaemonFacade_.getPinHash(
        newHostId, hostPin,
        startHostWithHash.bind(null, hostName, publicKey, privateKey,
                               email, refreshToken, clientBaseJid),
        onError);
  }

  /**
   * @param {string} hostName
   * @param {string} publicKey
   * @param {string} privateKey
   * @param {string} email
   * @param {string} refreshToken
   */
  function onServiceAccountCredentials(
      hostName, publicKey, privateKey, email, refreshToken) {
    that.getClientBaseJid_(
        onClientBaseJid.bind(
            null, hostName, publicKey, privateKey, email, refreshToken),
        onStartError);
  }

  /**
   * @param {string} hostName
   * @param {string} publicKey
   * @param {string} privateKey
   * @param {XMLHttpRequest} xhr
   */
  function onRegistered(
      hostName, publicKey, privateKey, xhr) {
    var success = (xhr.status == 200);

    if (success) {
      var result = jsonParseSafe(xhr.responseText);
      if ('data' in result && 'authorizationCode' in result['data']) {
        that.hostDaemonFacade_.getCredentialsFromAuthCode(
            result['data']['authorizationCode'],
            onServiceAccountCredentials.bind(
                null, hostName, publicKey, privateKey),
            onError);
      } else {
        // No authorization code returned, use regular user credential flow.
        that.hostDaemonFacade_.getPinHash(
            newHostId, hostPin, startHostWithHash.bind(
                null, hostName, publicKey, privateKey,
                remoting.identity.getCachedEmail(),
                remoting.oauth2.getRefreshToken()),
          onError);
      }
    } else {
      console.log('Failed to register the host. Status: ' + xhr.status +
                  ' response: ' + xhr.responseText);
      onError(remoting.Error.REGISTRATION_FAILED);
    }
  }

  /**
   * @param {string} hostName
   * @param {string} privateKey
   * @param {string} publicKey
   * @param {string} hostClientId
   * @param {string} oauthToken
   */
  function doRegisterHost(
      hostName, privateKey, publicKey, hostClientId, oauthToken) {
    var headers = {
      'Authorization': 'OAuth ' + oauthToken,
      'Content-type' : 'application/json; charset=UTF-8'
    };

    var newHostDetails = { data: {
       hostId: newHostId,
       hostName: hostName,
       publicKey: publicKey
    } };

    var registerHostUrl =
        remoting.settings.DIRECTORY_API_BASE_URL + '/@me/hosts';

    if (hostClientId) {
      registerHostUrl += '?' + remoting.xhr.urlencodeParamHash(
          { hostClientId: hostClientId });
    }

    remoting.xhr.post(
        registerHostUrl,
        onRegistered.bind(null, hostName, publicKey, privateKey),
        JSON.stringify(newHostDetails),
        headers);
  }

  /**
   * @param {string} hostName
   * @param {string} privateKey
   * @param {string} publicKey
   * @param {string} hostClientId
   */
  function onHostClientId(
      hostName, privateKey, publicKey, hostClientId) {
    remoting.identity.callWithToken(
        doRegisterHost.bind(
            null, hostName, privateKey, publicKey, hostClientId), onError);
  }

  /**
   * @param {string} hostName
   * @param {string} privateKey
   * @param {string} publicKey
   * @param {boolean} hasFeature
   */
  function onHasFeatureOAuthClient(
      hostName, privateKey, publicKey, hasFeature) {
    if (hasFeature) {
      that.hostDaemonFacade_.getHostClientId(
          onHostClientId.bind(null, hostName, privateKey, publicKey), onError);
    } else {
      remoting.identity.callWithToken(
          doRegisterHost.bind(
              null, hostName, privateKey, publicKey, null), onError);
    }
  }

  /**
   * @param {string} hostName
   * @param {string} privateKey
   * @param {string} publicKey
   */
  function onKeyGenerated(hostName, privateKey, publicKey) {
    that.hasFeature(
        remoting.HostController.Feature.OAUTH_CLIENT,
        onHasFeatureOAuthClient.bind(null, hostName, privateKey, publicKey));
  }

  /**
   * @param {string} hostName
   * @return {void} Nothing.
   */
  function startWithHostname(hostName) {
    that.hostDaemonFacade_.generateKeyPair(onKeyGenerated.bind(null, hostName),
                                         onError);
  }

  this.hostDaemonFacade_.getHostName(startWithHostname, onError);
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
  }

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

  this.hostDaemonFacade_.stopDaemon(onStopped, onError);
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
    that.hostDaemonFacade_.updateDaemonConfig(newConfig, onConfigUpdated,
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
    that.hostDaemonFacade_.getPinHash(
        hostId, newPin, updateDaemonConfigWithHash, onError);
  }

  // TODO(sergeyu): When crbug.com/121518 is fixed: replace this call
  // with an unprivileged version if that is necessary.
  this.hostDaemonFacade_.getDaemonConfig(onConfig, onError);
};

/**
 * Get the state of the local host.
 *
 * @param {function(remoting.HostController.State):void} onDone Completion
 *     callback.
 */
remoting.HostController.prototype.getLocalHostState = function(onDone) {
  /** @param {remoting.Error} error */
  function onError(error) {
    onDone((error == remoting.Error.MISSING_PLUGIN) ?
               remoting.HostController.State.NOT_INSTALLED :
               remoting.HostController.State.UNKNOWN);
  }
  this.hostDaemonFacade_.getDaemonState(onDone, onError);
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

  this.hostDaemonFacade_.getDaemonConfig(onConfig, function(error) {
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
  this.hostDaemonFacade_.getPairedClients(onDone, onError);
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
  this.hostDaemonFacade_.deletePairedClient(client, onDone, onError);
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
  this.hostDaemonFacade_.clearPairedClients(onDone, onError);
};

/**
 * Gets the host owner's base JID, used by the host for client authorization.
 * In most cases this is the same as the owner's email address, but for
 * non-Gmail accounts, it may be different.
 *
 * @private
 * @param {function(string): void} onSuccess
 * @param {function(remoting.Error): void} onError
 */
remoting.HostController.prototype.getClientBaseJid_ = function(
    onSuccess, onError) {
  var signalStrategy = null;

  var onState = function(state) {
    switch (state) {
      case remoting.SignalStrategy.State.CONNECTED:
        var jid = signalStrategy.getJid().split('/')[0].toLowerCase();
        base.dispose(signalStrategy);
        signalStrategy = null;
        onSuccess(jid);
        break;

      case remoting.SignalStrategy.State.FAILED:
        var error = signalStrategy.getError();
        base.dispose(signalStrategy);
        signalStrategy = null;
        onError(error);
        break;
    }
  };

  signalStrategy = remoting.SignalStrategy.create(onState);

  /** @param {string} token */
  function connectSignalingWithToken(token) {
    remoting.identity.getEmail(
        connectSignalingWithTokenAndEmail.bind(null, token), onError);
  }

  /**
   * @param {string} token
   * @param {string} email
   */
  function connectSignalingWithTokenAndEmail(token, email) {
    signalStrategy.connect(
        remoting.settings.XMPP_SERVER_ADDRESS, email, token);
  }

  remoting.identity.callWithToken(connectSignalingWithToken, onError);
};

/** @type {remoting.HostController} */
remoting.hostController = null;
