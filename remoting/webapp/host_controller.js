// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor */
remoting.HostController = function() {
  /** @type {remoting.HostPlugin} @private */
  this.plugin_ = remoting.HostSession.createPlugin();
  /** @type {HTMLElement} @private */
  this.container_ = document.getElementById('daemon-plugin-container');
  this.container_.appendChild(this.plugin_);
  /** @type {remoting.Host?} */
  this.localHost = null;
  /** @param {string} version */
  var printVersion = function(version) {
    if (version == '') {
      console.log('Host not installed.');
    } else {
      console.log('Host version:', version);
    }
  };
  this.plugin_.getDaemonVersion(printVersion);
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

/** @enum {number} */
remoting.HostController.AsyncResult = {
  OK: 0,
  FAILED: 1,
  CANCELLED: 2,
  FAILED_DIRECTORY: 3
};

/** @return {remoting.HostController.State} The current state of the daemon. */
remoting.HostController.prototype.state = function() {
  var result = this.plugin_.daemonState;
  if (typeof(result) == 'undefined') {
    // If the plug-in can't be instantiated, for example on ChromeOS, then
    // return something sensible.
    return remoting.HostController.State.NOT_IMPLEMENTED;
  } else {
    return result;
  }
};

/**
 * Show or hide daemon-specific parts of the UI.
 * @return {void} Nothing.
 */
remoting.HostController.prototype.updateDom = function() {
  var match = '';
  var state = this.state();
  switch (state) {
    case remoting.HostController.State.STARTED:
      remoting.updateModalUi('enabled', 'data-daemon-state');
      break;
    case remoting.HostController.State.NOT_IMPLEMENTED:
      document.getElementById('start-daemon').disabled = true;
      document.getElementById('start-daemon-message').innerText =
          chrome.i18n.getMessage(
              /*i18n-content*/'HOME_DAEMON_DISABLED_MESSAGE');
      // No break;
    case remoting.HostController.State.STOPPED:
    case remoting.HostController.State.NOT_INSTALLED:
      remoting.updateModalUi('disabled', 'data-daemon-state');
      break;
  }
};

/**
 * Set tool-tips for the 'connect' action. We can't just set this on the
 * parent element because the button has no tool-tip, and therefore would
 * inherit connectStr.
 *
 * return {void} Nothing.
 */
remoting.HostController.prototype.setTooltips = function() {
  var connectStr = '';
  if (this.localHost) {
    chrome.i18n.getMessage(/*i18n-content*/'TOOLTIP_CONNECT',
                           this.localHost.hostName);
  }
  document.getElementById('this-host-name').title = connectStr;
  document.getElementById('this-host-icon').title = connectStr;
};

/**
 * Registers and starts the host.
 * @param {string} hostPin Host PIN.
 * @param {function(remoting.HostController.AsyncResult):void} callback
 * callback Callback to be called when done.
 * @return {void} Nothing.
 */
remoting.HostController.prototype.start = function(hostPin, callback) {
  /** @type {remoting.HostController} */
  var that = this;
  var hostName = that.plugin_.getHostName();

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
    return e[0] + e[1] + '-' + e[2] + "-" + e[3] + '-' +
        e[4] + '-' + e[5] + e[6] + e[7];
  };

  var newHostId = generateUuid();

  /** @param {function(remoting.HostController.AsyncResult):void} callback
   *  @param {remoting.HostController.AsyncResult} result
   *  @param {string} hostName */
  function onStarted(callback, result, hostName) {
    if (result == remoting.HostController.AsyncResult.OK) {
      // Create a dummy remoting.Host instance to represent the local host.
      // Refreshing the list is no good in general, because the directory
      // information won't be in sync for several seconds. We don't know the
      // host JID or public key, but they can be missing from the cache with
      // no ill effects (it will be refreshed--we hope that the directory
      // will have been updated by that point).
      var localHost = new remoting.Host();
      localHost.hostName = hostName;
      localHost.hostId = newHostId;
      localHost.status = 'ONLINE';
      that.setHost(localHost);
      remoting.hostList.addHost(localHost);
    } else {
      // Unregister the host if we failed to start it.
      remoting.HostList.unregisterHostById(newHostId);
    }
    callback(result);
  };

  /** @param {string} privateKey
   *  @param {XMLHttpRequest} xhr */
  function onRegistered(privateKey, xhr) {
    var success = (xhr.status == 200);

    if (success) {
      var hostSecretHash =
          that.plugin_.getPinHash(newHostId, hostPin);
      var hostConfig = JSON.stringify({
          xmpp_login: remoting.oauth2.getCachedEmail(),
          oauth_refresh_token: remoting.oauth2.exportRefreshToken(),
          oauth_use_official_client_id:
              remoting.oauth2.USE_OFFICIAL_CLIENT_ID,
          host_id: newHostId,
          host_name: hostName,
          host_secret_hash: hostSecretHash,
          private_key: privateKey
      });
      /** @param {remoting.HostController.AsyncResult} result */
      var onStartDaemon = function(result) {
        onStarted(callback, result, hostName);
      };
      that.plugin_.startDaemon(hostConfig, onStartDaemon);
    } else {
      console.log('Failed to register the host. Status: ' + xhr.status +
                  ' response: ' + xhr.responseText);
      callback(remoting.HostController.AsyncResult.FAILED_DIRECTORY);
    }
  };

  /**
   * @param {string} privateKey
   * @param {string} publicKey
   * @param {string} oauthToken
   */
  function doRegisterHost(privateKey, publicKey, oauthToken) {
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
        'https://www.googleapis.com/chromoting/v1/@me/hosts/',
        /** @param {XMLHttpRequest} xhr */
        function (xhr) { onRegistered(privateKey, xhr); },
        JSON.stringify(newHostDetails),
        headers);
  };

  /** @param {string} privateKey
   *  @param {string} publicKey */
  function onKeyGenerated(privateKey, publicKey) {
    remoting.oauth2.callWithToken(
        /** @param {string?} oauthToken */
        function(oauthToken) {
          if (oauthToken) {
            doRegisterHost(privateKey, publicKey, oauthToken);
          } else {
            // TODO(jamiewalch): Have a more specific error code here?
            callback(remoting.HostController.AsyncResult.FAILED);
          }
        });
  };

  this.plugin_.generateKeyPair(onKeyGenerated);
};

/**
 * Stop the daemon process.
 * @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback to be called when finished.
 * @return {void} Nothing.
 */
remoting.HostController.prototype.stop = function(callback) {
  /** @type {remoting.HostController} */
  var that = this;

  /** @param {remoting.HostController.AsyncResult} result */
  function onStopped(result) {
    if (that.localHost && that.localHost.hostId)
      remoting.HostList.unregisterHostById(that.localHost.hostId);
    callback(result);
  };
  this.plugin_.stopDaemon(onStopped);
};

/**
 * Parse a stringified host configuration and return it as a dictionary if it
 * is well-formed and contains both host_id and xmpp_login keys.  null is
 * returned if either key is missing, or if the configuration is corrupt.
 * @param {string} configStr The host configuration, JSON encoded to a string.
 * @return {Object.<string,string>|null} The host configuration.
 */
function parseHostConfig_(configStr) {
  var config = /** @type {Object.<string,string>} */ jsonParseSafe(configStr);
  if (config &&
      typeof config['host_id'] == 'string' &&
      typeof config['xmpp_login'] == 'string') {
    return config;
  } else {
    console.error('Invalid getDaemonConfig response.');
  }
  return null;
}

/**
 * @param {string} newPin The new PIN to set
 * @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback to be called when finished.
 * @return {void} Nothing.
 */
remoting.HostController.prototype.updatePin = function(newPin, callback) {
  /** @type {remoting.HostController} */
  var that = this;

  /** @param {string} configStr */
  function onConfig(configStr) {
    var config = parseHostConfig_(configStr);
    if (!config) {
      callback(remoting.HostController.AsyncResult.FAILED);
      return;
    }
    var hostId = config['host_id'];
    var newConfig = JSON.stringify({
        host_secret_hash: that.plugin_.getPinHash(hostId, newPin)
      });
    that.plugin_.updateDaemonConfig(newConfig, callback);
  };

  // TODO(sergeyu): When crbug.com/121518 is fixed: replace this call
  // with an upriveleged version if that is necessary.
  this.plugin_.getDaemonConfig(onConfig);
};

/**
 * Set the remoting.Host instance (retrieved from the Chromoting service)
 * that corresponds to the local computer, if any.
 *
 * @param {remoting.Host?} host The host, or null if not registered.
 * @return {void} Nothing.
 */
remoting.HostController.prototype.setHost = function(host) {
  this.localHost = host;
  this.setTooltips();
  /** @type {remoting.HostController} */
  var that = this;
  if (host) {
    /** @param {remoting.HostTableEntry} host */
    var renameHost = function(host) {
      remoting.hostList.renameHost(host);
      that.setTooltips();
    };
    if (!this.hostTableEntry_) {
      /** @type {remoting.HostTableEntry} @private */
      this.hostTableEntry_ = new remoting.HostTableEntry();
      this.hostTableEntry_.init(host,
                                document.getElementById('this-host-connect'),
                                document.getElementById('this-host-name'),
                                document.getElementById('this-host-rename'),
                                renameHost);
    } else {
      // TODO(jamiewalch): This is hack to prevent multiple click handlers being
      // registered for the same DOM elements if this method is called more than
      // once. A better solution would be to let HostTable create the daemon row
      // like it creates the rows for non-local hosts.
      this.hostTableEntry_.host = host;
    }
  } else {
    this.hostTableEntry_ = null;
  }
};

/**
 * Update the internal state so that the local host can be correctly filtered
 * out of the host list.
 *
 * @param {remoting.HostList} hostList The new host list, returned by the
 *     Chromoting service.
 * @param {function():void} onDone Completion callback.
 */
remoting.HostController.prototype.onHostListRefresh =
    function(hostList, onDone) {
  /** @type {remoting.HostController} */
  var that = this;
  /** @param {string} configStr */
  function onConfig(configStr) {
    var config = parseHostConfig_(configStr);
    if (config) {
      var hostId = config['host_id'];
      that.setHost(hostList.getHostForId(hostId));
    } else {
      that.setHost(null);
    }
    onDone();
  };
  try {
    this.plugin_.getDaemonConfig(onConfig);
  } catch (err) {
    this.setHost(null);
    onDone();
  }
};

/** @type {remoting.HostController} */
remoting.hostController = null;
