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
    CLIENT_UNCONNECTED: 'client.unconnected',
    CLIENT_CONNECTING: 'client.connecting',
    CLIENT_CONNECT_FAILED: 'client.connect-failed',
  HOST: 'host',
    HOST_UNSHARED: 'host.unshared',
    HOST_WAITING_FOR_CODE: 'host.waiting-for-code',
    HOST_WAITING_FOR_CONNECTION: 'host.waiting-for-connection',
    HOST_SHARED: 'host.shared',
    HOST_SHARE_FAILED: 'host.share-failed',
  IN_SESSION: 'in-session'
};

remoting.EMAIL = 'email';
remoting.HOST_PLUGIN_ID = 'host-plugin-id';

// TODO(jamiewalch): Replace this with a proper l10n strategy.
/** @enum {string} */
remoting.ClientError = {
  NO_RESPONSE: 'Failed to get response from server.',
  INVALID_ACCESS_CODE: 'Invalid access code.',
  MISSING_PLUGIN: 'The viewer plugin is missing or out-of-date. Please ' +
      'upgrade to a more recent version of Chrome.',
  OAUTH_FETCH_FAILED: 'Unable to fetch OAuth2 token. Try revoking the ' +
      'token and authenticating again.',
  OTHER_ERROR: 'An error occurred.'
};

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

function hasClass(classes, cls) {
  return classes.match(new RegExp('(\\s|^)' + cls + '(\\s|$)'));
}

function addClass(element, cls) {
  if (!hasClass(element.className, cls))
    element.className = element.className + ' ' + cls;
}

function removeClass(element, cls) {
  element.className =
      element.className.replace(new RegExp('\\b' + cls + '\\b', 'g'), '');
}

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
  document.getElementById('current-email').innerText = loginName || '';

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
  window.localStorage.removeItem(KEY_EMAIL_);
  updateAuthStatus_();
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
  remoting.setMode(getAppStartupMode());
  if (isHostModeSupported()) {
    var unsupported = document.getElementById('client-footer-text-cros');
    unsupported.parentNode.removeChild(unsupported);
  } else {
    var footer = document.getElementById('client-footer-text');
    footer.parentNode.removeChild(footer);
    document.getElementById('client-footer-text-cros').id =
        'client-footer-text';
  }
}

/**
 * Change the app's modal state to |mode|, which is considered to be a dotted
 * hierachy of modes. For example, setMode('host.shared') will show any modal
 * elements with an data-ui-mode attribute of 'host' or 'host.shared' and hide
 * all others.
 *
 * @param {string} mode The new modal state, expressed as a dotted hiearchy.
 */
remoting.setMode = function(mode) {
  var modes = mode.split('.');
  for (var i = 1; i < modes.length; ++i)
    modes[i] = modes[i - 1] + '.' + modes[i];
  var elements = document.querySelectorAll('[data-ui-mode]');
  for (var i = 0; i < elements.length; ++i) {
    var element = elements[i];
    var hidden = true;
    for (var m = 0; m < modes.length; ++m) {
      if (hasClass(element.getAttribute('data-ui-mode'), modes[m])) {
        hidden = false;
        break;
      }
    }
    element.hidden = hidden;
  }
  remoting.debug.log('App mode: ' + mode);
  remoting.currentMode = modes[0];
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

  remoting.setMode(remoting.AppMode.HOST_WAITING_FOR_CODE);
  document.getElementById('cancel-button').disabled = false;
  disableTimeoutCountdown_();

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

function disableTimeoutCountdown_() {
  if (remoting.timerRunning) {
    clearInterval(remoting.accessCodeTimerId);
    remoting.timerRunning = false;
    updateTimeoutStyles_();
  }
}

var ACCESS_CODE_TIMER_DISPLAY_THRESHOLD = 30;
var ACCESS_CODE_RED_THRESHOLD = 10;

/**
 * Show/hide or restyle various elements, depending on the remaining countdown
 * and timer state.
 *
 * @return {bool} True if the timeout is in progress, false if it has expired.
 */
function updateTimeoutStyles_() {
  if (remoting.timerRunning) {
    if (remoting.accessCodeExpiresIn <= 0) {
      remoting.cancelShare();
      return false;
    }
    if (remoting.accessCodeExpiresIn <= ACCESS_CODE_RED_THRESHOLD) {
      addClass(document.getElementById('access-code-display'), 'expiring');
    } else {
      removeClass(document.getElementById('access-code-display'), 'expiring');
    }
  }
  document.getElementById('access-code-countdown').hidden =
      (remoting.accessCodeExpiresIn > ACCESS_CODE_TIMER_DISPLAY_THRESHOLD) ||
      !remoting.timerRunning;
  return true;
}

remoting.decrementAccessCodeTimeout_ = function() {
  --remoting.accessCodeExpiresIn;
  remoting.updateAccessCodeTimeoutElement_();
}

remoting.updateAccessCodeTimeoutElement_ = function() {
  var pad = (remoting.accessCodeExpiresIn < 10) ? '0' : '';
  document.getElementById('seconds-remaining').innerText =
      pad + remoting.accessCodeExpiresIn;
  if (!updateTimeoutStyles_()) {
    disableTimeoutCountdown_();
  }
}

function onStateChanged_() {
  var plugin = document.getElementById(remoting.HOST_PLUGIN_ID);
  var state = plugin.state;
  if (state == plugin.REQUESTED_ACCESS_CODE) {
    // Nothing to do here.
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
    remoting.accessCodeExpiresIn = plugin.accessCodeLifetime;
    if (remoting.accessCodeExpiresIn > 0) {  // Check it hasn't expired.
      remoting.accessCodeTimerId = setInterval(
          'remoting.decrementAccessCodeTimeout_()', 1000);
      remoting.timerRunning = true;
      remoting.updateAccessCodeTimeoutElement_();
      updateTimeoutStyles_();
      remoting.setMode(remoting.AppMode.HOST_WAITING_FOR_CONNECTION);
    } else {
      // This can only happen if the access code takes more than 5m to get from
      // the cloud to the web-app, so we don't care how clean this UX is.
      remoting.debug.log('Access code already invalid on receipt!');
      remoting.cancelShare();
    }
  } else if (state == plugin.CONNECTED) {
    remoting.setMode(remoting.AppMode.HOST_SHARED);
    disableTimeoutCountdown_();
  } else if (state == plugin.DISCONNECTED) {
    remoting.setMode(remoting.AppMode.HOST_UNSHARED);
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
  remoting.setMode(remoting.AppMode.HOST_SHARE_FAILED);
}

remoting.cancelShare = function() {
  remoting.debug.log('Canceling share...');
  var plugin = document.getElementById(remoting.HOST_PLUGIN_ID);
  plugin.disconnect();
  disableTimeoutCountdown_();
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

function onClientStateChange_(oldState) {
  var state = remoting.session.state;
  if (state == remoting.ClientSession.State.CREATED) {
    remoting.debug.log('Created plugin');
  } else if (state == remoting.ClientSession.State.BAD_PLUGIN_VERSION) {
    showConnectError_(remoting.ClientError.MISSING_PLUGIN);
  } else if (state == remoting.ClientSession.State.CONNECTING) {
    remoting.debug.log('Connecting as ' + remoting.username);
  } else if (state == remoting.ClientSession.State.INITIALIZING) {
    remoting.debug.log('Initializing connection');
  } else if (state == remoting.ClientSession.State.CONNECTED) {
    var split = remoting.hostJid.split('/');
    var host = null;
    if (split.length == 2) {
      host = split[0];
    }
    setClientStateMessage('Connected to', host);
    remoting.setMode(remoting.AppMode.IN_SESSION);
    updateStatistics();
  } else if (state == remoting.ClientSession.State.CLOSED) {
    if (oldState != remoting.ClientSession.State.CONNECTED) {
      // TODO(jamiewalch): This is not quite correct, as it will report
      // "Invalid access code", regardless of what actually went wrong.
      // Fix this up by having the host send a suitable error code.
      showConnectError_(remoting.ClientError.INVALID_ACCESS_CODE);
    } else {
      remoting.debug.log('Connection closed');
      remoting.setClientMode('unconnected');
    }
  } else if (state == remoting.ClientSession.State.CONNECTION_FAILED) {
    remoting.debug.log('Client plugin reported connection failed');
    showConnectError_(remoting.ClientError.OTHER_ERROR);
  } else {
    remoting.debug.log('Unexpected client plugin state: ' + state);
    showConnectError_(remoting.ClientError.OTHER_ERROR);
  }
}

function startSession_() {
  remoting.debug.log('Starting session...');
  remoting.username = getEmail();
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

function showConnectError_(errorMsg) {
  remoting.debug.log('Connection failed: ' + errorMsg);
  var errorNode = document.getElementById('connect-error-message');
  errorNode.innerText = errorMsg;
  remoting.accessCode = '';
  if (remoting.session) {
    remoting.session.disconnect();
    remoting.session = null;
  }
  remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED);
}

function parseServerResponse_(xhr) {
  remoting.debug.log('parseServerResponse: status = ' + xhr.status);
  if (xhr.status == 200) {
    var host = JSON.parse(xhr.responseText);
    if (host.data && host.data.jabberId) {
      remoting.hostJid = host.data.jabberId;
      remoting.hostPublicKey = host.data.publicKey;
      startSession_();
      return;
    }
  }
  var errorMsg = remoting.ClientError.OTHER_ERROR;
  if (xhr.status == 404) {
    errorMsg = remoting.ClientError.INVALID_ACCESS_CODE;
  } else if (xhr.status == 0) {
    errorMsg = remoting.ClientError.NO_RESPONSE;
  } else {
    remoting.debug.log('The server responded: ' + xhr.responseText);
  }
  showConnectError_(errorMsg);
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

remoting.doTryConnect = function() {
  var accessCode = document.getElementById('access-code-entry').value;
  remoting.accessCode = normalizeAccessCode_(accessCode);
  // At present, only 12-digit access codes are supported, of which the first
  // 7 characters are the supportId.
  if (remoting.accessCode.length != kAccessCodeLen) {
    remoting.debug.log('Bad access code length');
    showConnectError_(remoting.ClientError.INVALID_ACCESS_CODE);
  } else {
    var supportId = remoting.accessCode.substring(0, kSupportIdLen);
    remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
    resolveSupportId(supportId);
  }
}

remoting.tryConnect = function() {
  if (remoting.oauth2.needsNewAccessToken()) {
    remoting.oauth2.refreshAccessToken(function(xhr) {
      if (remoting.oauth2.needsNewAccessToken()) {
        // Failed to get access token
        remoting.debug.log('tryConnect: OAuth2 token fetch failed');
        showConnectError_(remoting.ClientError.OAUTH_FETCH_FAILED);
        return;
      }
      remoting.doTryConnect();
    });
  } else {
    remoting.doTryConnect();
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
  window.localStorage.setItem(KEY_APP_MODE_, mode);
  remoting.setMode(getAppStartupMode());
}

/**
 * Gets the major-mode that this application should start up in.
 *
 * @return {remoting.AppMode} The mode (client or host) to start in.
 */
function getAppStartupMode() {
  if (isHostModeSupported()) {
    var mode = window.localStorage.getItem(KEY_APP_MODE_);
    if (mode == remoting.AppMode.CLIENT) {
      return remoting.AppMode.CLIENT_UNCONNECTED;
    }
    return remoting.AppMode.HOST_UNSHARED;
  } else {
    return remoting.AppMode.CLIENT_UNCONNECTED;
  }
}

/**
 * Returns whether Host mode is supported on this platform.
 *
 * @return {boolean} True if Host mode is supported.
 */
function isHostModeSupported() {
  // Currently, ChromeOS is not supported.
  return !navigator.userAgent.match(/\bCrOS\b/);
}

/**
 * Enable or disable scale-to-fit.
 *
 * @return {void} Nothing.
 */
remoting.toggleScaleToFit = function() {
  remoting.scaleToFit = !remoting.scaleToFit;
  remoting.session.toggleScaleToFit(remoting.scaleToFit);
}

/**
 * Disconnect the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.disconnect = function() {
  if (remoting.session) {
    remoting.session.disconnect();
    remoting.session = null;
    remoting.debug.log('Disconnected.');
    remoting.setMode(remoting.AppMode.CLIENT_UNCONNECTED);
    var accessCode = document.getElementById('access-code-entry');
    accessCode.value = '';
  }
}

}());
