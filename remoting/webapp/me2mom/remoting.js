// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var remoting = remoting || {};

(function() {
'use strict';

window.addEventListener('blur', pluginLostFocus_, false);

function pluginLostFocus_() {
  // If the plug loses input focus, release all keys as a precaution against
  // leaving them 'stuck down' on the host.
  if (remoting.session && remoting.session.plugin) {
    remoting.session.plugin.releaseAllKeys();
  }
}

/** @enum {string} */
remoting.AppMode = {
  CLIENT: 'client',
  HOST: 'host',
  IN_SESSION: 'in-session'
};

remoting.EMAIL = 'email';
remoting.HOST_PLUGIN_ID = 'host-plugin-id';

/**
 * Whether or not the plugin should scale itself.
 * @type {boolean}
 */
remoting.scaleToFit = false;

// Constants representing keys used for storing persistent application state.
var KEY_APP_MODE_ = 'remoting-app-mode';
var KEY_EMAIL_ = 'remoting-email';

// Some constants for pretty-printing the access code.
var kSupportIdLen = 7;
var kHostSecretLen = 5;
var kAccessCodeLen = kSupportIdLen + kHostSecretLen;
var kDigitsPerGroup = 4;

function retrieveEmail_(access_token) {
  var headers = {
    'Authorization': 'OAuth ' + remoting.oauth2.getAccessToken()
  };

  var onResponse = function(xhr) {
    if (xhr.status != 200) {
      // TODO(ajwong): Have a better way of showing an error.
      window.alert('Unable to get e-mail');
      return;
    }

    // TODO(ajwong): See if we can't find a JSON endpoint.
    setEmail(xhr.responseText.split('&')[0].split('=')[1]);
  };

  // TODO(ajwong): Update to new v2 API.
  remoting.xhr.get('https://www.googleapis.com/userinfo/email',
                   onResponse, '', headers);
}

function refreshEmail_() {
  if (!getEmail() && remoting.oauth2.isAuthenticated()) {
    remoting.oauth2.callWithToken(retrieveEmail_);
  }
}

// This code moved into this subroutine (instead of being inlined in
// updateAuthStatus_() because of bug in V8.
// http://code.google.com/p/v8/issues/detail?id=1423
function updateControls_(disable) {
  var authStatusControls =
      document.getElementsByClassName('auth-status-control');
  for (var i = 0; i < authStatusControls.length; ++i) {
    authStatusControls[i].disabled = disable;
  }
}

function updateAuthStatus_() {
  var oauthValid = remoting.oauth2.isAuthenticated();
  document.getElementById('oauth2-token-button').hidden = oauthValid;
  document.getElementById('oauth2-clear-button').hidden = !oauthValid;

  var loginName = getEmail();
  if (loginName) {
    document.getElementById('current-email').innerText = loginName;
  }

  var disableControls = !(loginName && oauthValid);
  var controlPanel = document.getElementById('control-panel');
  // TODO(ajwong): Do this via a style, or remove if the new auth flow is
  // implemented.
  if (disableControls) {
    controlPanel.style.backgroundColor = 'rgba(204, 0, 0, 0.15)';
  } else {
    controlPanel.style.backgroundColor = 'rgba(0, 204, 102, 0.15)';
  }
  updateControls_(disableControls);
}

function setEmail(value) {
  window.localStorage.setItem(KEY_EMAIL_, value);
  updateAuthStatus_();
}

/**
 * @return {string} The email address associated with the auth credentials.
 */
function getEmail() {
  return window.localStorage.getItem(KEY_EMAIL_);
}

function exchangedCodeForToken_() {
  if (!remoting.oauth2.isAuthenticated()) {
    alert('Your OAuth2 token was invalid. Please try again.');
  }
  remoting.oauth2.callWithToken(function(token) {
      retrieveEmail_(token);
      updateAuthStatus_();
  });
}

remoting.clearOAuth2 = function() {
  remoting.oauth2.clear();
  updateAuthStatus_();
}

// Show the div with id |mode| and hide those with other ids in |modes|.
function setMode_(mode, modes) {
  for (var i = 0; i < modes.length; ++i) {
    modes[i].hidden = (mode != modes[i].id);
  }
}

remoting.toggleDebugLog = function() {
  var debugLog = document.getElementById('debug-log');
  if (debugLog.hidden) {
    debugLog.hidden = false;
  } else {
    debugLog.hidden = true;
  }
}

remoting.init = function() {
  // Create global objects.
  remoting.oauth2 = new remoting.OAuth2();
  remoting.debug =
      new remoting.DebugLog(document.getElementById('debug-messages'));

  updateAuthStatus_();
  refreshEmail_();
  remoting.setHostMode('unshared');
  remoting.setClientMode('unconnected');
  setGlobalMode(getAppStartupMode());
  document.getElementById('loading-mode').hidden = true;
  document.getElementById('choice-mode').hidden = false;
}

function setGlobalMode(mode) {
  var elementsToShow = [];
  var elementsToHide = [];
  var hostElements = document.getElementsByClassName('host-element');
  hostElements = Array.prototype.slice.apply(hostElements);
  var clientElements = document.getElementsByClassName('client-element');
  clientElements = Array.prototype.slice.apply(clientElements);
  var inSessionElements =
      document.getElementsByClassName('in-session-element');
  inSessionElements = Array.prototype.slice.apply(inSessionElements);
  if (mode == remoting.AppMode.HOST) {
    elementsToShow = elementsToShow.concat(hostElements);
    elementsToHide = elementsToHide.concat(clientElements, inSessionElements);
  } else if (mode == remoting.AppMode.CLIENT) {
    elementsToShow = elementsToShow.concat(clientElements);
    elementsToHide = elementsToHide.concat(hostElements, inSessionElements);
  } else if (mode == remoting.AppMode.IN_SESSION) {
    elementsToShow = elementsToShow.concat(inSessionElements);
    elementsToHide = elementsToHide.concat(hostElements, clientElements);
  }

  // Hide first and then show since an element may be in both lists.
  for (var i = 0; i < elementsToHide.length; ++i) {
    elementsToHide[i].hidden = true;
  }
  for (var i = 0; i < elementsToShow.length; ++i) {
    elementsToShow[i].hidden = false;
  }
  document.getElementById('waiting-footer').hidden = true;
  remoting.currentMode = mode;
}

function setGlobalModePersistent(mode) {
  setGlobalMode(mode);
  // TODO(ajwong): Does it make sense for "in_session" to be a peer to "host"
  // or "client mode"?  I don't think so, but not sure how to restructure UI.
  if (mode != remoting.AppMode.IN_SESSION) {
    remoting.storage.setStartupMode(mode);
  } else {
    remoting.storage.setStartupMode(remoting.AppMode.CLIENT);
  }
}

remoting.setHostMode = function(mode) {
  var section = document.getElementById('host-panel');
  var modes = section.getElementsByClassName('mode');
  remoting.debug.log('Host mode: ' + mode);
  setMode_(mode, modes);
}

remoting.setClientMode = function(mode) {
  var section = document.getElementById('client-panel');
  var modes = section.getElementsByClassName('mode');
  remoting.debug.log('Client mode: ' + mode);
  setMode_(mode, modes);
}

function showWaiting_() {
  document.getElementById('client-footer-text').hidden = true;
  document.getElementById('host-footer-text').hidden = true;
  document.getElementById('waiting-footer').hidden = false;
  document.getElementById('cancel-button').disabled = false;
}

remoting.tryShare = function() {
  remoting.debug.log('Attempting to share...');
  if (remoting.oauth2.needsNewAccessToken()) {
    remoting.debug.log('Refreshing token...');
    remoting.oauth2.refreshAccessToken(function() {
      if (remoting.oauth2.needsNewAccessToken()) {
        // If we still need it, we're going to infinite loop.
        showShareError_('unable-to-get-token');
        throw 'Unable to get access token';
      }
      remoting.tryShare();
    });
    return;
  }

  showWaiting_();

  var div = document.getElementById('host-plugin-container');
  var plugin = document.createElement('embed');
  plugin.type = remoting.PLUGIN_MIMETYPE;
  plugin.id = remoting.HOST_PLUGIN_ID;
  // Hiding the plugin means it doesn't load, so make it size zero instead.
  plugin.width = 0;
  plugin.height = 0;
  div.appendChild(plugin);
  plugin.onStateChanged = onStateChanged_;
  plugin.logDebugInfo = debugInfoCallback_;
  plugin.connect(getEmail(),
                 'oauth2:' + remoting.oauth2.getAccessToken());
}

function onStateChanged_() {
  var plugin = document.getElementById(remoting.HOST_PLUGIN_ID);
  var state = plugin.state;
  if (state == plugin.REQUESTED_ACCESS_CODE) {
    remoting.setHostMode('preparing-to-share');
  } else if (state == plugin.RECEIVED_ACCESS_CODE) {
    var accessCode = plugin.accessCode;
    var accessCodeDisplay = document.getElementById('access-code-display');
    accessCodeDisplay.innerText = '';
    // Display the access code in groups of four digits for readability.
    for (var i = 0; i < accessCode.length; i += kDigitsPerGroup) {
      var nextFourDigits = document.createElement('span');
      nextFourDigits.className = 'access-code-digit-group';
      nextFourDigits.innerText = accessCode.substring(i, i + kDigitsPerGroup);
      accessCodeDisplay.appendChild(nextFourDigits);
    }
    remoting.setHostMode('ready-to-share');
  } else if (state == plugin.CONNECTED) {
    remoting.setHostMode('shared');
  } else if (state == plugin.DISCONNECTED) {
    setGlobalMode(remoting.AppMode.HOST);
    remoting.setHostMode('unshared');
    plugin.parentNode.removeChild(plugin);
  } else {
    remoting.debug.log('Unknown state -> ' + state);
  }
}

/**
* This is that callback that the host plugin invokes to indicate that there
* is additional debug log info to display.
*/
function debugInfoCallback_(msg) {
  remoting.debug.log('plugin: ' + msg);
}

function showShareError_(errorCode) {
  var errorDiv = document.getElementById(errorCode);
  errorDiv.style.display = 'block';
  remoting.debug.log('Sharing error: ' + errorCode);
  remoting.setHostMode('share-failed');
}

remoting.cancelShare = function() {
  remoting.debug.log('Canceling share...');
  var plugin = document.getElementById(remoting.HOST_PLUGIN_ID);
  plugin.disconnect();
}

/**
 * Show a client message that stays on the screeen until the state changes.
 *
 * @param {string} message The message to display.
 * @param {string} opt_host The host to display after the message.
 */
function setClientStateMessage(message, opt_host) {
  document.getElementById('session-status-message').innerText = message;
  opt_host = opt_host || '';
  document.getElementById('connected-to').innerText = opt_host;
}

function updateStatistics() {
  if (remoting.session.state != remoting.ClientSession.State.CONNECTED)
    return;
  var stats = remoting.session.stats();

  var units = '';
  var videoBandwidth = stats['video_bandwidth'];
  if (videoBandwidth < 1024) {
    units = 'Bps';
  } else if (videoBandwidth < 1048576) {
    units = 'KiBps';
    videoBandwidth = videoBandwidth / 1024;
  } else if (videoBandwidth < 1073741824) {
    units = 'MiBps';
    videoBandwidth = videoBandwidth / 1048576;
  } else {
    units = 'GiBps';
    videoBandwidth = videoBandwidth / 1073741824;
  }

  var statistics = document.getElementById('statistics');
  statistics.innerText =
      'Bandwidth: ' + videoBandwidth.toFixed(2) + units +
      ', Capture: ' + stats['capture_latency'].toFixed(2) + 'ms' +
      ', Encode: ' + stats['encode_latency'].toFixed(2) + 'ms' +
      ', Decode: ' + stats['decode_latency'].toFixed(2) + 'ms' +
      ', Render: ' + stats['render_latency'].toFixed(2) + 'ms' +
      ', Latency: ' + stats['roundtrip_latency'].toFixed(2) + 'ms';

  // Update the stats once per second.
  window.setTimeout(updateStatistics, 1000);
}

function onClientStateChange_(state) {
  if (state == remoting.ClientSession.State.UNKNOWN) {
    setClientStateMessage('Unknown');
  } else if (state == remoting.ClientSession.State.CREATED) {
    setClientStateMessage('Created');
  } else if (state == remoting.ClientSession.State.BAD_PLUGIN_VERSION) {
    setClientStateMessage('Incompatible Plugin Version');
  } else if (state == remoting.ClientSession.State.UNKNOWN_PLUGIN_ERROR) {
    setClientStateMessage('Unknown error with plugin.');
  } else if (state == remoting.ClientSession.State.CONNECTING) {
    setClientStateMessage('Connecting as ' + remoting.username);
  } else if (state == remoting.ClientSession.State.INITIALIZING) {
    setClientStateMessage('Initializing connection');
  } else if (state == remoting.ClientSession.State.CONNECTED) {
    var split = remoting.hostJid.split('/');
    var host = null;
    if (split.length == 2) {
      host = split[0];
    }
    setClientStateMessage('Connected to', host);
    updateStatistics();
  } else if (state == remoting.ClientSession.State.CLOSED) {
    setClientStateMessage('Closed');
  } else if (state == remoting.ClientSession.State.CONNECTION_FAILED) {
    setClientStateMessage('Failed');
  } else {
    setClientStateMessage('Bad State: ' + state);
  }
}

function startSession_() {
  remoting.debug.log('Starting session...');
  remoting.username = getEmail();
  setGlobalMode(remoting.AppMode.IN_SESSION);
  remoting.session =
      new remoting.ClientSession(remoting.hostJid, remoting.hostPublicKey,
                                 remoting.accessCode, getEmail(),
                                 onClientStateChange_);
  remoting.oauth2.callWithToken(function(token) {
    remoting.session.createPluginAndConnect(
        document.getElementById('session-mode'),
        token);
  });
}

function showConnectError_(responseCode, responseString) {
  var invalid = document.getElementById('invalid-access-code');
  var other = document.getElementById('other-connect-error');
  if (responseCode == 404) {
    invalid.style.display = 'block';
    other.style.display = 'none';
  } else {
    invalid.style.display = 'none';
    other.style.display = 'block';
    var responseNode = document.getElementById('server-response');
    responseNode.innerText = responseString + ' (' + responseCode + ')';
  }
  remoting.accessCode = '';
  remoting.setClientMode('connect-failed');
}

function parseServerResponse_(xhr) {
  if (xhr.status == 200) {
    var host = JSON.parse(xhr.responseText);
    if (host.data && host.data.jabberId) {
      remoting.hostJid = host.data.jabberId;
      remoting.hostPublicKey = host.data.publicKey;
      startSession_();
      return;
    }
  }
  showConnectError_(xhr.status, xhr.responseText);
}

function normalizeAccessCode_(accessCode) {
  // Trim whitespace.
  // TODO(sergeyu): Do we need to do any other normalization here?
  return accessCode.replace(/\s/g, '');
}

function resolveSupportId(supportId) {
  var headers = {
    'Authorization': 'OAuth ' + remoting.oauth2.getAccessToken()
  };

  remoting.xhr.get(
      'https://www.googleapis.com/chromoting/v1/support-hosts/' +
          encodeURIComponent(supportId),
      parseServerResponse_,
      '',
      headers);
}

remoting.tryConnect = function() {
  if (remoting.oauth2.needsNewAccessToken()) {
    remoting.oauth2.refreshAccessToken(function() {
      if (remoting.oauth2.needsNewAccessToken()) {
        // If we still need it, we're going to infinite loop.
        throw 'Unable to get access token.';
      }
      remoting.tryConnect();
    });
    return;
  }
  var accessCode = document.getElementById('access-code-entry').value;
  remoting.accessCode = normalizeAccessCode_(accessCode);
  // At present, only 12-digit access codes are supported, of which the first
  // 7 characters are the supportId.
  if (remoting.accessCode.length != kAccessCodeLen) {
    showConnectError_(404);
  } else {
    var supportId = remoting.accessCode.substring(0, kSupportIdLen);
    remoting.setClientMode('connecting');
    resolveSupportId(supportId);
  }
}

remoting.cancelPendingOperation = function() {
  document.getElementById('cancel-button').disabled = true;
  if (remoting.currentMode == remoting.AppMode.HOST) {
    remoting.cancelShare();
  }
}

/**
 * Changes the major-mode of the application (Eg., client or host).
 *
 * @param {remoting.AppMode} mode The mode to shift the application into.
 * @return {void} Nothing.
 */
remoting.setAppMode = function(mode) {
  setGlobalMode(mode);
  window.localStorage.setItem(KEY_APP_MODE_, mode);
}

/**
 * Gets the major-mode that this application should start up in.
 *
 * @return {remoting.AppMode} The mode (client or host) to start in.
 */
function getAppStartupMode() {
  var mode = window.localStorage.getItem(KEY_APP_MODE_);
  if (!mode) {
    mode = remoting.AppMode.HOST;
  }
  return mode;
}

remoting.toggleScaleToFit = function() {
  remoting.scaleToFit = !remoting.scaleToFit;
  remoting.session.toggleScaleToFit(remoting.scaleToFit);
}

}());
