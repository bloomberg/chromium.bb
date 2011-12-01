// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  GENERIC: /*i18n-content*/'ERROR_GENERIC'
};

(function() {

/**
 * Entry point for app initialization.
 */
remoting.init = function() {
  l10n.localize();
  var button = document.getElementById('toggle-scaling');
  button.title = chrome.i18n.getMessage(/*i18n-content*/'TOOLTIP_SCALING');
  // Create global objects.
  remoting.oauth2 = new remoting.OAuth2();
  remoting.debug = new remoting.DebugLog(
      document.getElementById('debug-messages'),
      document.getElementById('statistics'));
  remoting.hostList = new remoting.HostList(
      document.getElementById('host-list'),
      document.getElementById('host-list-error'));

  refreshEmail_();
  var email = remoting.oauth2.getCachedEmail();
  if (email) {
    document.getElementById('current-email').innerText = email;
  }

  window.addEventListener('blur', pluginLostFocus_, false);

  // Parse URL parameters.
  var urlParams = getUrlParameters();
  if ('mode' in urlParams) {
    if (urlParams['mode'] == 'me2me') {
      var hostJid = urlParams['hostJid'];
      var hostPublicKey = urlParams['hostPublicKey'];
      var hostName = urlParams['hostName'];
      remoting.connectHost(hostJid, hostPublicKey, hostName);
      return;
    }
  }

  // No valid URL parameters, start up normally.
  remoting.setMode(getAppStartupMode_());
  if (isHostModeSupported_()) {
    var noShare = document.getElementById('chrome-os-no-share');
    noShare.parentNode.removeChild(noShare);
  } else {
    var button = document.getElementById('share-button');
    button.disabled = true;
  }
}

remoting.cancelPendingOperation = function() {
  document.getElementById('cancel-button').disabled = true;
  switch (remoting.getMajorMode()) {
    case remoting.AppMode.HOST:
      remoting.cancelShare();
      break;
    case remoting.AppMode.CLIENT:
      remoting.cancelConnect();
      break;
  }
}

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
}

/**
 * Sign the user out of Chromoting by clearing the OAuth refresh token.
 */
remoting.clearOAuth2 = function() {
  remoting.oauth2.clear();
  window.localStorage.removeItem(KEY_EMAIL_);
  remoting.setMode(remoting.AppMode.UNAUTHENTICATED);
}

/**
 * Callback function called when the browser window loses focus. In this case,
 * release all keys to prevent them becoming 'stuck down' on the host.
 */
function pluginLostFocus_() {
  if (remoting.clientSession && remoting.clientSession.plugin) {
    remoting.clientSession.plugin.releaseAllKeys();
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

/** The key under which the email address is stored. */
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
  return remoting.oauth2.isAuthenticated() ? remoting.AppMode.HOME
      : remoting.AppMode.UNAUTHENTICATED;
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
}());
