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
  if (remoting.currentConnectionType == remoting.ConnectionType.Me2Me) {
    remoting.initDaemonUi();
  } else {
    remoting.setMode(remoting.AppMode.HOME);
    document.getElementById('access-code-entry').value = '';
  }
};

/**
 * Toggle the scale-to-fit feature for the current client session.
 *
 * @return {void} Nothing.
 */
remoting.toggleScaleToFit = function() {
  remoting.clientSession.setScaleToFit(!remoting.clientSession.getScaleToFit());
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
 * Handle changes in the visibility of the window, for example by pausing video.
 *
 * @return {void} Nothing.
 */
remoting.onVisibilityChanged = function() {
  if (remoting.clientSession)
    remoting.clientSession.pauseVideo(document.webkitHidden);
}

/**
 * Disconnect the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.disconnect = function() {
  if (remoting.clientSession) {
    remoting.clientSession.disconnect();
    remoting.clientSession = null;
    console.log('Disconnected.');
    if (remoting.currentConnectionType == remoting.ConnectionType.It2Me) {
      remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_IT2ME);
    } else {
      remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_ME2ME);
    }
  }
};

/**
 * Sends a Ctrl-Alt-Del sequence to the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.sendCtrlAltDel = function() {
  if (remoting.clientSession) {
    console.log('Sending Ctrl-Alt-Del.');
    remoting.clientSession.sendCtrlAltDel();
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
      console.error('Bad access code length');
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

  // Clear the PIN on successful connection, or on error if we're not going to
  // automatically retry.
  var clearPin = false;

  if (newState == remoting.ClientSession.State.CREATED) {
    console.log('Created plugin');

  } else if (newState == remoting.ClientSession.State.BAD_PLUGIN_VERSION) {
    showConnectError_(remoting.Error.BAD_PLUGIN_VERSION);

  } else if (newState == remoting.ClientSession.State.CONNECTING) {
    console.log('Connecting as ' + remoting.oauth2.getCachedEmail());

  } else if (newState == remoting.ClientSession.State.INITIALIZING) {
    console.log('Initializing connection');

  } else if (newState == remoting.ClientSession.State.CONNECTED) {
    if (remoting.clientSession) {
      clearPin = true;
      setConnectionInterruptedButtonsText_();
      remoting.setMode(remoting.AppMode.IN_SESSION);
      remoting.toolbar.center();
      remoting.toolbar.preview();
      remoting.clipboard.startSession();
      updateStatistics_();
    }

  } else if (newState == remoting.ClientSession.State.CLOSED) {
    if (oldState == remoting.ClientSession.State.CONNECTED) {
      remoting.clientSession.removePlugin();
      remoting.clientSession = null;
      console.log('Connection closed by host');
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

  } else if (newState == remoting.ClientSession.State.FAILED) {
    console.error('Client plugin reported connection failed: ' +
                       remoting.clientSession.error);
    clearPin = true;
    if (remoting.clientSession.error ==
        remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE) {
      clearPin = false;
      retryConnectOrReportOffline_();
    } else if (remoting.clientSession.error ==
               remoting.ClientSession.ConnectionError.SESSION_REJECTED) {
      showConnectError_(remoting.Error.INVALID_ACCESS_CODE);
    } else if (remoting.clientSession.error ==
               remoting.ClientSession.ConnectionError.INCOMPATIBLE_PROTOCOL) {
      showConnectError_(remoting.Error.INCOMPATIBLE_PROTOCOL);
    } else if (remoting.clientSession.error ==
               remoting.ClientSession.ConnectionError.NETWORK_FAILURE) {
      showConnectError_(remoting.Error.NETWORK_FAILURE);
    } else if (remoting.clientSession.error ==
               remoting.ClientSession.ConnectionError.HOST_OVERLOAD) {
      showConnectError_(remoting.Error.HOST_OVERLOAD);
    } else {
      showConnectError_(remoting.Error.GENERIC);
    }

    if (clearPin) {
      document.getElementById('pin-entry').value = '';
    }

  } else {
    console.error('Unexpected client plugin state: ' + newState);
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
  if (remoting.clientSession) {
    remoting.clientSession.removePlugin();
    remoting.clientSession = null;
  }
  if (remoting.hostId && remoting.retryIfOffline) {
    console.warn('Connection failed. Retrying.');
    /** @param {boolean} success True if the refresh was successful. */
    var onDone = function(success) {
      if (success) {
        remoting.retryIfOffline = false;
        remoting.connectMe2MeWithPin();
      } else {
        showConnectError_(remoting.Error.HOST_IS_OFFLINE);
      }
    };
    remoting.hostList.refresh(onDone);
  } else {
    console.error('Connection failed. Not retrying.');
    showConnectError_(remoting.Error.HOST_IS_OFFLINE);
  }
}

/**
 * Create the client session object and initiate the connection.
 *
 * @return {void} Nothing.
 */
function startSession_() {
  console.log('Starting session...');
  var accessCode = document.getElementById('access-code-entry');
  accessCode.value = '';  // The code has been validated and won't work again.
  remoting.clientSession =
      new remoting.ClientSession(
          remoting.hostJid, remoting.hostPublicKey,
          remoting.accessCode, 'v1_token', '',
          /** @type {string} */ (remoting.oauth2.getCachedEmail()),
          remoting.ClientSession.Mode.IT2ME,
          onClientStateChange_);
  /** @param {string?} token The auth token. */
  var createPluginAndConnect = function(token) {
    if (token) {
      remoting.clientSession.createPluginAndConnect(
          document.getElementById('session-mode'),
          token);
    } else {
      showConnectError_(remoting.Error.AUTHENTICATION_FAILED);
    }
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
  console.error('Connection failed: ' + errorTag);
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
 * Set the text on the buttons shown under the error message so that they are
 * easy to understand in the case where a successful connection failed, as
 * opposed to the case where a connection never succeeded.
 */
function setConnectionInterruptedButtonsText_() {
  var button1 = document.getElementById('client-reconnect-button');
  l10n.localizeElementFromTag(button1, /*i18n-content*/'RECONNECT');
  button1.removeAttribute('autofocus');
  var button2 = document.getElementById('client-finished-me2me-button');
  l10n.localizeElementFromTag(button2, /*i18n-content*/'OK');
  button2.setAttribute('autofocus', 'autofocus');
}

/**
 * Parse the response from the server to a request to resolve a support id.
 *
 * @param {XMLHttpRequest} xhr The XMLHttpRequest object.
 * @return {void} Nothing.
 */
function parseServerResponse_(xhr) {
  remoting.supportHostsXhr_ = null;
  console.log('parseServerResponse: xhr = ' + xhr);
  if (xhr.status == 200) {
    var host = /** @type {{data: {jabberId: string, publicKey: string}}} */
        jsonParseSafe(xhr.responseText);
    if (host && host.data && host.data.jabberId && host.data.publicKey) {
      remoting.hostJid = host.data.jabberId;
      remoting.hostPublicKey = host.data.publicKey;
      var split = remoting.hostJid.split('/');
      document.getElementById('connected-to').innerText = split[0];
      startSession_();
      return;
    } else {
      console.error('Invalid "support-hosts" response from server.');
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
    console.error('The server responded: ' + xhr.responseText);
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
  var perfstats = remoting.clientSession.getPerfStats();
  remoting.stats.update(perfstats);
  remoting.clientSession.logStatistics(perfstats);
  // Update the stats once per second.
  window.setTimeout(updateStatistics_, 1000);
}

/**
 * @return {boolean} true if the client plugin supports PIN auth.
 */
remoting.isPinAuthSupported = function () {
  var plugin = /** @type {remoting.ViewerPlugin} */
      document.createElement('embed');
  plugin.src = 'about://none';
  plugin.type = 'pepper-application/x-chromoting';
  plugin.width = 0;
  plugin.height = 0;
  document.body.appendChild(plugin);
  var version = plugin.apiVersion;
  document.body.removeChild(plugin);
  // Future version of the plugin will not have apiVersion. We assume
  // that they support PINs.
  return !version || version >= 4;
};

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

  if (!remoting.isPinAuthSupported()) {
    // Skip PIN prompt if it is not supported.
    remoting.connectMe2MeWithPin();
  } else {
    var host = remoting.hostList.getHostForId(remoting.hostId);
    // If we're re-loading a tab for a host that has since been unregistered
    // then the hostId may no longer resolve.
    if (!host) {
      showConnectError_(remoting.Error.HOST_IS_OFFLINE);
      return;
    }
    var message = document.getElementById('pin-message');
    l10n.localizeElement(message, host.hostName);
    remoting.setMode(remoting.AppMode.CLIENT_PIN_PROMPT);
  }
};

/**
 * Start a connection to the specified host, using the cached details
 * and the PIN entered by the user.
 *
 * @return {void} Nothing.
 */
remoting.connectMe2MeWithPin = function() {
  console.log('Connecting to host...');
  remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);

  var host = remoting.hostList.getHostForId(remoting.hostId);
  // If the user clicked on a cached host that has since been removed then we
  // won't find the hostId. If the user clicked on the entry for the local host
  // immediately after having enabled it then we won't know it's JID or public
  // key until the host heartbeats and we pull a fresh host list.
  if (!host || !host.jabberId || !host.publicKey) {
    retryConnectOrReportOffline_();
    return;
  }
  remoting.hostJid = host.jabberId;
  remoting.hostPublicKey = host.publicKey;
  document.getElementById('connected-to').innerText = host.hostName;
  document.title = chrome.i18n.getMessage('PRODUCT_NAME') + ': ' +
      host.hostName;

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

    remoting.clientSession =
        new remoting.ClientSession(
            remoting.hostJid, remoting.hostPublicKey,
            pin, 'spake2_hmac,spake2_plain', remoting.hostId,
            /** @type {string} */ (remoting.oauth2.getCachedEmail()),
            remoting.ClientSession.Mode.ME2ME, onClientStateChange_);
    remoting.clientSession.createPluginAndConnect(
        document.getElementById('session-mode'),
        token);
  } else {
    showConnectError_(remoting.Error.AUTHENTICATION_FAILED);
  }
}
