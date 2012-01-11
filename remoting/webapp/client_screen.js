// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Functions related to the 'client screen' for Chromoting.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @type {boolean} Whether or not the plugin should scale itself.
 */
remoting.scaleToFit = false;

/**
 * @type {remoting.ClientSession} The client session object, set once the
 *     access code has been successfully verified.
 */
remoting.clientSession = null;

/**
 * @type {string} The normalized access code.
 */
remoting.accessCode = '';

/**
 * @type {string} The host's JID, returned by the server.
 */
remoting.hostJid = '';

/**
 * @type {string} For Me2Me connections, the id of the current host.
 */
remoting.hostId = '';

/**
 * @type {boolean} For Me2Me connections. Set to true if connection
 * must be retried on failure.
 */
remoting.retryIfOffline = false;

/**
 * @type {string} The host's public key, returned by the server.
 */
remoting.hostPublicKey = '';

/**
 * @type {XMLHttpRequest} The XHR object corresponding to the current
 *     support-hosts request, if there is one outstanding.
 * @private
 */
remoting.supportHostsXhr_ = null;

/**
 * @enum {string}
 */
remoting.ConnectionType = {
  It2Me: 'It2Me',
  Me2Me: 'Me2Me'
};

/**
 * @type {remoting.ConnectionType?}
 */
remoting.currentConnectionType = null;

/**
 * Entry point for the 'connect' functionality. This function defers to the
 * WCS loader to call it back with an access token.
 */
remoting.connectIt2Me = function() {
  remoting.currentConnectionType = remoting.ConnectionType.It2Me;
  document.getElementById('cancel-button').disabled = false;
  remoting.WcsLoader.load(connectIt2MeWithAccessToken_);
};

/**
 * Cancel an incomplete connect operation.
 *
 * @return {void} Nothing.
 */
remoting.cancelConnect = function() {
  if (remoting.supportHostsXhr_) {
    remoting.supportHostsXhr_.abort();
    remoting.supportHostsXhr_ = null;
  }
  if (remoting.clientSession) {
    remoting.clientSession.removePlugin();
    remoting.clientSession = null;
  }
  remoting.setMode(remoting.AppMode.HOME);
};

/**
 * Enable or disable scale-to-fit.
 *
 * @param {Event} event The click event. The style of the target is updated to
 *     reflect the new scaling state.
 * @return {void} Nothing.
 */
remoting.toggleScaleToFit = function(event) {
  var button = /** @type Element */(event.target);
  remoting.scaleToFit = !remoting.scaleToFit;
  if (remoting.scaleToFit) {
    addClass(button, 'toggle-button-active');
  } else {
    removeClass(button, 'toggle-button-active');
  }
  remoting.clientSession.updateDimensions();
};

/**
 * Update the remoting client layout in response to a resize event.
 *
 * @return {void} Nothing.
 */
remoting.onResize = function() {
  if (remoting.clientSession)
    remoting.clientSession.onResize();
};

/**
 * Disconnect the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.disconnect = function() {
  if (remoting.clientSession) {
    remoting.clientSession.disconnect();
    remoting.clientSession = null;
    remoting.debug.log('Disconnected.');
    if (remoting.currentConnectionType == remoting.ConnectionType.It2Me) {
      remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_IT2ME);
    } else {
      remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_ME2ME);
    }
  }
};

/**
 * If WCS was successfully loaded, proceed with the connection, otherwise
 * report an error.
 *
 * @param {string?} token The OAuth2 access token, or null if an error occurred.
 * @return {void} Nothing.
 */
function connectIt2MeWithAccessToken_(token) {
  if (token) {
    var accessCode = document.getElementById('access-code-entry').value;
    remoting.accessCode = normalizeAccessCode_(accessCode);
    // At present, only 12-digit access codes are supported, of which the first
    // 7 characters are the supportId.
    var kSupportIdLen = 7;
    var kHostSecretLen = 5;
    var kAccessCodeLen = kSupportIdLen + kHostSecretLen;
    if (remoting.accessCode.length != kAccessCodeLen) {
      remoting.debug.log('Bad access code length');
      showConnectError_(remoting.Error.INVALID_ACCESS_CODE);
    } else {
      var supportId = remoting.accessCode.substring(0, kSupportIdLen);
      remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
      resolveSupportId(supportId);
    }
  } else {
    showConnectError_(remoting.Error.AUTHENTICATION_FAILED);
  }
}

/**
 * Callback function called when the state of the client plugin changes. The
 * current state is available via the |state| member variable.
 *
 * @param {number} oldState The previous state of the plugin.
 * @param {number} newState The current state of the plugin.
 */
// TODO(jamiewalch): Make this pass both the current and old states to avoid
// race conditions.
function onClientStateChange_(oldState, newState) {
  if (!remoting.clientSession) {
    // If the connection has been cancelled, then we no longer have a reference
    // to the session object and should ignore any state changes.
    return;
  }
  if (newState == remoting.ClientSession.State.CREATED) {
    remoting.debug.log('Created plugin');

  } else if (newState == remoting.ClientSession.State.BAD_PLUGIN_VERSION) {
    showConnectError_(remoting.Error.BAD_PLUGIN_VERSION);

  } else if (newState == remoting.ClientSession.State.CONNECTING) {
    remoting.debug.log('Connecting as ' + remoting.oauth2.getCachedEmail());

  } else if (newState == remoting.ClientSession.State.INITIALIZING) {
    remoting.debug.log('Initializing connection');

  } else if (newState == remoting.ClientSession.State.CONNECTED) {
    if (remoting.clientSession) {
      remoting.setMode(remoting.AppMode.IN_SESSION);
      remoting.toolbar.center();
      remoting.toolbar.preview();
      updateStatistics_();
    }

  } else if (newState == remoting.ClientSession.State.CLOSED) {
    if (oldState == remoting.ClientSession.State.CONNECTED) {
      remoting.clientSession.removePlugin();
      remoting.clientSession = null;
      remoting.debug.log('Connection closed by host');
      if (remoting.currentConnectionType == remoting.ConnectionType.It2Me) {
        remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_IT2ME);
      } else {
        remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_ME2ME);
      }
    } else {
      // The transition from CONNECTING to CLOSED state may happen
      // only with older client plugins. Current version should go the
      // FAILED state when connection fails.
      showConnectError_(remoting.Error.INVALID_ACCESS_CODE);
    }

  } else if (newState == remoting.ClientSession.State.CONNECTION_FAILED) {
    remoting.debug.log('Client plugin reported connection failed: ' +
                       remoting.clientSession.error);
    if (remoting.clientSession.error ==
        remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE) {
      retryConnectOrReportOffline_();
    } else if (remoting.clientSession.error ==
               remoting.ClientSession.ConnectionError.SESSION_REJECTED) {
      showConnectError_(remoting.Error.INVALID_ACCESS_CODE);
    } else if (remoting.clientSession.error ==
               remoting.ClientSession.ConnectionError.INCOMPATIBLE_PROTOCOL) {
      showConnectError_(remoting.Error.INCOMPATIBLE_PROTOCOL);
    } else if (remoting.clientSession.error ==
               remoting.ClientSession.ConnectionError.NETWORK_FAILURE) {
      showConnectError_(remoting.Error.GENERIC);
    } else {
      showConnectError_(remoting.Error.GENERIC);
    }

  } else {
    remoting.debug.log('Unexpected client plugin state: ' + newState);
    // This should only happen if the web-app and client plugin get out of
    // sync, and even then the version check should allow compatibility.
    showConnectError_(remoting.Error.MISSING_PLUGIN);
  }
}

/**
 * If we have a hostId to retry, try refreshing it and connecting again. If not,
 * then show the 'host offline' error message.
 *
 * @return {void} Nothing.
 */
function retryConnectOrReportOffline_() {
  if (remoting.hostId && remoting.retryIfOffline) {
    console.log('Connection failed. Retrying.');
    /** @param {boolean} success True if the refresh was successful. */
    var onDone = function(success) {
      if (success) {
        remoting.connectMe2Me(remoting.hostId, false);
      } else {
        showConnectError_(remoting.Error.HOST_IS_OFFLINE);
      }
    };
    remoting.hostList.refresh(onDone);
  } else {
    console.log('Connection failed. Not retrying.');
    showConnectError_(remoting.Error.HOST_IS_OFFLINE);
  }
}

/**
 * Create the client session object and initiate the connection.
 *
 * @return {void} Nothing.
 */
function startSession_() {
  remoting.debug.log('Starting session...');
  var accessCode = document.getElementById('access-code-entry');
  accessCode.value = '';  // The code has been validated and won't work again.
  remoting.clientSession =
      new remoting.ClientSession(
          remoting.hostJid, remoting.hostPublicKey,
          remoting.accessCode,
          /** @type {string} */ (remoting.oauth2.getCachedEmail()),
          onClientStateChange_);
  /** @param {string} token The auth token. */
  var createPluginAndConnect = function(token) {
    remoting.clientSession.createPluginAndConnect(
        document.getElementById('session-mode'),
        token);
  };
  remoting.oauth2.callWithToken(createPluginAndConnect);
}

/**
 * Show a client-side error message.
 *
 * @param {remoting.Error} errorTag The error to be localized and
 *     displayed.
 * @return {void} Nothing.
 */
function showConnectError_(errorTag) {
  remoting.debug.log('Connection failed: ' + errorTag);
  var errorDiv = document.getElementById('connect-error-message');
  l10n.localizeElementFromTag(errorDiv, /** @type {string} */ (errorTag));
  remoting.accessCode = '';
  if (remoting.clientSession) {
    remoting.clientSession.disconnect();
    remoting.clientSession = null;
  }
  if (remoting.currentConnectionType == remoting.ConnectionType.It2Me) {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME);
  } else {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_ME2ME);
  }
}

/**
 * Parse the response from the server to a request to resolve a support id.
 *
 * @param {XMLHttpRequest} xhr The XMLHttpRequest object.
 * @return {void} Nothing.
 */
function parseServerResponse_(xhr) {
  remoting.supportHostsXhr_ = null;
  remoting.debug.log('parseServerResponse: status = ' + xhr.status);
  if (xhr.status == 200) {
    var host = /** @type {{data: {jabberId: string, publicKey: string}}} */
        JSON.parse(xhr.responseText);
    if (host.data && host.data.jabberId && host.data.publicKey) {
      remoting.hostJid = host.data.jabberId;
      remoting.hostPublicKey = host.data.publicKey;
      var split = remoting.hostJid.split('/');
      document.getElementById('connected-to').innerText = split[0];
      startSession_();
      return;
    }
  }
  var errorMsg = remoting.Error.GENERIC;
  if (xhr.status == 404) {
    errorMsg = remoting.Error.INVALID_ACCESS_CODE;
  } else if (xhr.status == 0) {
    errorMsg = remoting.Error.NO_RESPONSE;
  } else if (xhr.status == 503) {
    errorMsg = remoting.Error.SERVICE_UNAVAILABLE;
  } else {
    remoting.debug.log('The server responded: ' + xhr.responseText);
  }
  showConnectError_(errorMsg);
}

/**
 * Normalize the access code entered by the user.
 *
 * @param {string} accessCode The access code, as entered by the user.
 * @return {string} The normalized form of the code (whitespace removed).
 */
function normalizeAccessCode_(accessCode) {
  // Trim whitespace.
  // TODO(sergeyu): Do we need to do any other normalization here?
  return accessCode.replace(/\s/g, '');
}

/**
 * Initiate a request to the server to resolve a support ID.
 *
 * @param {string} supportId The canonicalized support ID.
 */
function resolveSupportId(supportId) {
  var headers = {
    'Authorization': 'OAuth ' + remoting.oauth2.getAccessToken()
  };

  remoting.supportHostsXhr_ = remoting.xhr.get(
      'https://www.googleapis.com/chromoting/v1/support-hosts/' +
          encodeURIComponent(supportId),
      parseServerResponse_,
      '',
      headers);
}

/**
 * Timer callback to update the statistics panel.
 */
function updateStatistics_() {
  if (!remoting.clientSession ||
      remoting.clientSession.state != remoting.ClientSession.State.CONNECTED) {
    return;
  }
  var stats = remoting.clientSession.stats();
  remoting.debug.updateStatistics(stats);
  remoting.clientSession.logStatistics(stats);
  // Update the stats once per second.
  window.setTimeout(updateStatistics_, 1000);
}


/**
 * Shows PIN entry screen.
 *
 * @param {string} hostId The unique id of the host.
 * @param {boolean} retryIfOffline If true and the host can't be contacted,
 *     refresh the host list and try again. This allows bookmarked hosts to
 *     work even if they reregister with Talk and get a different Jid.
 * @return {void} Nothing.
 */
remoting.connectMe2Me = function(hostId, retryIfOffline) {
  remoting.currentConnectionType = remoting.ConnectionType.Me2Me;
  remoting.hostId = hostId;
  remoting.retryIfOffline = retryIfOffline;

  // TODO(jamiewalch): Reinstate the PIN screen once it's supported.
  // remoting.setMode(remoting.AppMode.CLIENT_PIN_PROMPT);
  remoting.connectMe2MeWithPin();
}

/**
 * Start a connection to the specified host, using the cached details
 * and the PIN entered by the user.
 *
 * @return {void} Nothing.
 */
remoting.connectMe2MeWithPin = function() {
  remoting.debug.log('Connecting to host...');
  remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);

  var host = remoting.hostList.getHostForId(remoting.hostId);
  if (!host) {
    retryConnectOrReportOffline_();
    return;
  }
  remoting.hostJid = host.jabberId;
  remoting.hostPublicKey = host.publicKey;
  document.getElementById('connected-to').innerText = host.hostName;
  document.title = document.title + ': ' + host.hostName;

  remoting.WcsLoader.load(connectMe2MeWithAccessToken_);
};

/**
 * Continue making the connection to a host, once WCS has initialized.
 *
 * @param {string?} token The OAuth2 access token, or null if an error occurred.
 * @return {void} Nothing.
 */
function connectMe2MeWithAccessToken_(token) {
  if (token) {
    /** @type {string} */
    var pin = document.getElementById('pin-entry').value;
    document.getElementById('pin-entry').value = '';

    remoting.clientSession =
        new remoting.ClientSession(
            remoting.hostJid, remoting.hostPublicKey,
            pin, /** @type {string} */ (remoting.oauth2.getCachedEmail()),
            onClientStateChange_);
    remoting.clientSession.createPluginAndConnect(
        document.getElementById('session-mode'),
        token);
  } else {
    showConnectError_(remoting.Error.AUTHENTICATION_FAILED);
  }
}
