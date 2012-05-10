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

  remoting.showOrHideIt2MeUi();
  remoting.showOrHideMe2MeUi();

  // The plugin's onFocus handler sends a paste command to |window|, because
  // it can't send one to the plugin element itself.
  window.addEventListener('paste', pluginGotPaste_, false);
  window.addEventListener('copy', pluginGotCopy_, false);

  remoting.initModalDialogs();

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
  remoting.hostController = new remoting.HostController();
  remoting.hostController.updateDom();
  remoting.setMode(getAppStartupMode_());
  remoting.hostSetupDialog =
      new remoting.HostSetupDialog(remoting.hostController);
  remoting.extractThisHostAndDisplay(true);
  remoting.hostList.refresh(remoting.extractThisHostAndDisplay);
};

/**
 * Extract the remoting.Host object corresponding to this host (if any) and
 * display the list.
 *
 * @param {boolean} success True if the host list refresh was successful.
 * @return {void} Nothing.
 */
remoting.extractThisHostAndDisplay = function(success) {
  if (success) {
    var display = function() {
      var hostId = null;
      if (remoting.hostController.localHost) {
        hostId = remoting.hostController.localHost.hostId;
      }
      remoting.hostList.display(hostId);
    };
    remoting.hostController.onHostListRefresh(remoting.hostList, display);
  } else {
    remoting.hostController.setHost(null);
    remoting.hostList.display(null);
  }
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
        jsonParseSafe(xhr.responseText);
    if (manifest) {
      var name = chrome.i18n.getMessage('PRODUCT_NAME');
      console.log(name + ' version: ' + manifest.version);
    } else {
      console.error('Failed to get product version. Corrupt manifest?');
    }
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
 *
 * Also clear all localStorage, to avoid leaking information.
 */
remoting.signOut = function() {
  remoting.oauth2.clear();
  window.localStorage.clear();
  remoting.setMode(remoting.AppMode.UNAUTHENTICATED);
};

/**
 * Returns whether the app is running on ChromeOS.
 *
 * @return {boolean} True if the app is running on ChromeOS.
 */
remoting.runningOnChromeOS = function() {
  return !!navigator.userAgent.match(/\bCrOS\b/);
}

/**
 * Callback function called when the browser window gets a paste operation.
 *
 * @param {Event} eventUncast
 * @return {void} Nothing.
 */
function pluginGotPaste_(eventUncast) {
  var event = /** @type {remoting.ClipboardEvent} */ eventUncast;
  if (event && event.clipboardData) {
    remoting.clipboard.toHost(event.clipboardData);
  }
}

/**
 * Callback function called when the browser window gets a copy operation.
 *
 * @param {Event} eventUncast
 * @return {void} Nothing.
 */
function pluginGotCopy_(eventUncast) {
  var event = /** @type {remoting.ClipboardEvent} */ eventUncast;
  if (event && event.clipboardData) {
    if (remoting.clipboard.toOs(event.clipboardData)) {
      // The default action may overwrite items that we added to clipboardData.
      event.preventDefault();
    }
  }
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
    var e = document.getElementById('auth-error-message');
    e.innerText = chrome.i18n.getMessage(remoting.Error.SERVICE_UNAVAILABLE);
    e.classList.add('error-state');
    remoting.setMode(remoting.AppMode.UNAUTHENTICATED);
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
  return !remoting.runningOnChromeOS();
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

/**
 * @param {string} jsonString A JSON-encoded string.
 * @return {*} The decoded object, or undefined if the string cannot be parsed.
 */
function jsonParseSafe(jsonString) {
  try {
    return JSON.parse(jsonString);
  } catch (err) {
    return undefined;
  }
}
