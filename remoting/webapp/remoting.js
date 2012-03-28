// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @type {remoting.HostSession} */ remoting.hostSession = null;

/**
 * @enum {string} All error messages from messages.json
 */
remoting.Error = {
  NO_RESPONSE: /*i18n-content*/'ERROR_NO_RESPONSE',
  INVALID_ACCESS_CODE: /*i18n-content*/'ERROR_INVALID_ACCESS_CODE',
  MISSING_PLUGIN: /*i18n-content*/'ERROR_MISSING_PLUGIN',
  AUTHENTICATION_FAILED: /*i18n-content*/'ERROR_AUTHENTICATION_FAILED',
  HOST_IS_OFFLINE: /*i18n-content*/'ERROR_HOST_IS_OFFLINE',
  INCOMPATIBLE_PROTOCOL: /*i18n-content*/'ERROR_INCOMPATIBLE_PROTOCOL',
  BAD_PLUGIN_VERSION: /*i18n-content*/'ERROR_BAD_PLUGIN_VERSION',
  NETWORK_FAILURE: /*i18n-content*/'ERROR_NETWORK_FAILURE',
  HOST_OVERLOAD: /*i18n-content*/'ERROR_HOST_OVERLOAD',
  GENERIC: /*i18n-content*/'ERROR_GENERIC',
  UNEXPECTED: /*i18n-content*/'ERROR_UNEXPECTED',
  SERVICE_UNAVAILABLE: /*i18n-content*/'ERROR_SERVICE_UNAVAILABLE'
};

/**
 * Entry point for app initialization.
 */
remoting.init = function() {
  remoting.logExtensionInfoAsync_();
  l10n.localize();
  var button = document.getElementById('toggle-scaling');
  button.title = chrome.i18n.getMessage(/*i18n-content*/'TOOLTIP_SCALING');
  // Create global objects.
  remoting.oauth2 = new remoting.OAuth2();
  remoting.stats = new remoting.ConnectionStats(
      document.getElementById('statistics'));
  remoting.formatIq = new remoting.FormatIq();
  remoting.hostList = new remoting.HostList(
      document.getElementById('host-list'),
      document.getElementById('host-list-error'));
  remoting.toolbar = new remoting.Toolbar(
      document.getElementById('session-toolbar'));
  remoting.clipboard = new remoting.Clipboard();

  refreshEmail_();
  var email = remoting.oauth2.getCachedEmail();
  if (email) {
    document.getElementById('current-email').innerText = email;
  }

  // The plugin's onFocus handler sends a paste command to |window|, because
  // it can't send one to the plugin element itself.
  window.addEventListener('paste', pluginGotPaste_, false);

  if (isHostModeSupported_()) {
    var noShare = document.getElementById('chrome-os-no-share');
    noShare.parentNode.removeChild(noShare);
  } else {
    var button = document.getElementById('share-button');
    button.disabled = true;
  }

  // Parse URL parameters.
  var urlParams = getUrlParameters_();
  if ('mode' in urlParams) {
    if (urlParams['mode'] == 'me2me') {
      var hostId = urlParams['hostId'];
      remoting.connectMe2Me(hostId, true);
      return;
    }
  }

  // No valid URL parameters, start up normally.
  remoting.initDaemonUi();
};

// initDaemonUi is called if the app is not starting up in session mode, and
// also if the user cancels pin entry or the connection in session mode.
remoting.initDaemonUi = function () {
  remoting.daemonPlugin = new remoting.DaemonPlugin();
  remoting.daemonPlugin.updateDom();
  remoting.setMode(getAppStartupMode_());
  remoting.askPinDialog = new remoting.AskPinDialog(remoting.daemonPlugin);
};

/**
 * Log information about the current extension.
 * The extension manifest is loaded and parsed to extract this info.
 */
remoting.logExtensionInfoAsync_ = function() {
  /** @type {XMLHttpRequest} */
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'manifest.json');
  xhr.onload = function(e) {
    var manifest =
        /** @type {{name: string, version: string, default_locale: string}} */
        JSON.parse(xhr.responseText);
    var name = chrome.i18n.getMessage('PRODUCT_NAME');
    console.log(name + ' version: ' + manifest.version);
  }
  xhr.send(null);
};

/**
 * If the client is connected, or the host is shared, prompt before closing.
 *
 * @return {?string} The prompt string if a connection is active.
 */
remoting.promptClose = function() {
  switch (remoting.currentMode) {
    case remoting.AppMode.CLIENT_CONNECTING:
    case remoting.AppMode.HOST_WAITING_FOR_CODE:
    case remoting.AppMode.HOST_WAITING_FOR_CONNECTION:
    case remoting.AppMode.HOST_SHARED:
    case remoting.AppMode.IN_SESSION:
      var result = chrome.i18n.getMessage(/*i18n-content*/'CLOSE_PROMPT');
      return result;
    default:
      return null;
  }
};

/**
 * Sign the user out of Chromoting by clearing the OAuth refresh token.
 */
remoting.clearOAuth2 = function() {
  remoting.oauth2.clear();
  window.localStorage.removeItem(KEY_EMAIL_);
  remoting.setMode(remoting.AppMode.UNAUTHENTICATED);
};

/**
 * Callback function called when the browser window gets a paste operation.
 *
 * @param {Event} eventUncast
 * @return {boolean}
 */
function pluginGotPaste_(eventUncast) {
  var event = /** @type {remoting.ClipboardEvent} */ eventUncast;
  if (event && event.clipboardData) {
    remoting.clipboard.toHost(event.clipboardData);
  }
  return false;
}

/**
 * If the user is authenticated, but there is no email address cached, get one.
 */
function refreshEmail_() {
  if (!getEmail_() && remoting.oauth2.isAuthenticated()) {
    remoting.oauth2.getEmail(setEmail_);
  }
}

/**
 * The key under which the email address is stored.
 * @private
 */
var KEY_EMAIL_ = 'remoting-email';

/**
 * Save the user's email address in local storage.
 *
 * @param {?string} email The email address to place in local storage.
 * @return {void} Nothing.
 */
function setEmail_(email) {
  if (email) {
    document.getElementById('current-email').innerText = email;
  } else {
    // TODO(ajwong): Have a better way of showing an error.
    document.getElementById('current-email').innerText = '???';
  }
}

/**
 * Read the user's email address from local storage.
 *
 * @return {?string} The email address associated with the auth credentials.
 */
function getEmail_() {
  var result = window.localStorage.getItem(KEY_EMAIL_);
  return typeof result == 'string' ? result : null;
}

/**
 * Gets the major-mode that this application should start up in.
 *
 * @return {remoting.AppMode} The mode to start in.
 */
function getAppStartupMode_() {
  if (!remoting.oauth2.isAuthenticated()) {
    return remoting.AppMode.UNAUTHENTICATED;
  }
  return remoting.AppMode.HOME;
}

/**
 * Returns whether Host mode is supported on this platform.
 *
 * @return {boolean} True if Host mode is supported.
 */
function isHostModeSupported_() {
  // Currently, sharing on Chromebooks is not supported.
  return !navigator.userAgent.match(/\bCrOS\b/);
}

/**
 * @return {Object.<string, string>} The URL parameters.
 */
function getUrlParameters_() {
  var result = {};
  var parts = window.location.search.substring(1).split('&');
  for (var i = 0; i < parts.length; i++) {
    var pair = parts[i].split('=');
    result[pair[0]] = decodeURIComponent(pair[1]);
  }
  return result;
}
