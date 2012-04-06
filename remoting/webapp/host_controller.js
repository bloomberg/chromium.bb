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
  try {
    return this.plugin_.daemonState;
  } catch (err) {
    // If the plug-in can't be instantiated, for example on ChromeOS, then
    // return something sensible.
    return remoting.HostController.State.NOT_IMPLEMENTED;
  }
};

/**
 * Show or hide daemon-specific parts of the UI.
 * @return {void} Nothing.
 */
remoting.HostController.prototype.updateDom = function() {
  // TODO(sergeyu): This code updates UI state. Does it belong here,
  // or should it moved somewhere else?
  var match = '';
  var row = document.getElementById('this-host-connect');
  var state = this.state();
  switch (state) {
    case remoting.HostController.State.STARTED:
      remoting.updateModalUi('enabled', 'data-daemon-state');
      row.classList.remove('host-offline');
      row.classList.add('clickable');
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
      row.classList.add('host-offline');
      row.classList.remove('clickable');
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
  document.getElementById('this-host-spacer').title = connectStr;
};

/**
 * Registers and starts the host.
 * @param {string} hostPin Host PIN.
 * @param {function(remoting.HostController.AsyncResult):void}
 * callback Callback to be called when done.
 * @return {void} Nothing.
 */
remoting.HostController.prototype.start = function(hostPin, callback) {
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
    return e[0] + e[1] + '-' + e[2] + "-" + e[3] + '-' +
        e[4] + '-' + e[5] + e[6] + e[7];
  };

  var newHostId = generateUuid();
  // TODO(jamiewalch): Create an unprivileged API to get the host id from the
  // plugin instead of storing it locally (crbug.com/121518).
  window.localStorage.setItem('me2me-host-id', newHostId);

  /** @param {string} privateKey
   *  @param {XMLHttpRequest} xhr */
  function onRegistered(privateKey, xhr) {
    var success = (xhr.status == 200);

    if (success) {
      // TODO(sergeyu): Calculate HMAC of the PIN instead of storing it
      // in plaintext.
      var hostConfig = JSON.stringify({
          xmpp_login: remoting.oauth2.getCachedEmail(),
          oauth_refresh_token: remoting.oauth2.getRefreshToken(),
          host_id: newHostId,
          host_name: that.plugin_.getHostName(),
          host_secret_hash: 'plain:' + window.btoa(hostPin),
          private_key: privateKey
        });
      that.plugin_.startDaemon(hostConfig, callback);
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
       hostName: that.plugin_.getHostName(),
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
        /** @param {string} oauthToken */
        function(oauthToken) {
          doRegisterHost(privateKey, publicKey, oauthToken);
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
    window.localStorage.removeItem('me2me-host-id');
    callback(result);
  };
  this.plugin_.stopDaemon(onStopped);
};

/**
 * @param {string} newPin The new PIN to set
 * @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback to be called when finished.
 * @return {void} Nothing.
 */
remoting.HostController.prototype.updatePin = function(newPin, callback) {
      // TODO(sergeyu): Calculate HMAC of the PIN instead of storing it
      // in plaintext.
  var newConfig = JSON.stringify({
          host_secret_hash: 'plain:' + window.btoa(newPin)
      });
  this.plugin_.updateDaemonConfig(newConfig, callback);
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
    /** @type {remoting.HostTableEntry} @private */
    this.hostTableEntry_ = new remoting.HostTableEntry();
    this.hostTableEntry_.init(host,
                              document.getElementById('this-host-connect'),
                              document.getElementById('this-host-name'),
                              document.getElementById('this-host-rename'),
                              renameHost);
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
 * @param {function():void} onDone Completion callback. TODO(jamiewalch): For
 *     now, this is synchronous and reads the host id from local storage. In
 *     the future, it will asynchronously read the host id from the plugin
 *     (crbug.com/121518).
 */
remoting.HostController.prototype.onHostListRefresh =
    function(hostList, onDone) {
  var hostId = window.localStorage.getItem('me2me-host-id');
  if (hostId && typeof(hostId) == 'string') {
    this.setHost(hostList.getHostForId(/** @type{string} */(hostId)));
  } else {
    this.setHost(null);
  }
  onDone();
};

/** @type {remoting.HostController} */
remoting.hostController = null;
