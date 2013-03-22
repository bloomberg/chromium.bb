// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @type {remoting.HostSession} */ remoting.hostSession = null;

/**
 * Show the authorization consent UI and register a one-shot event handler to
 * continue the authorization process.
 *
 * @param {function():void} authContinue Callback to invoke when the user
 *     clicks "Continue".
 */
function consentRequired_(authContinue) {
  /** @type {HTMLElement} */
  var dialog = document.getElementById('auth-dialog');
  /** @type {HTMLElement} */
  var button = document.getElementById('auth-button');
  var consentGranted = function(event) {
    dialog.hidden = true;
    button.removeEventListener('click', consentGranted, false);
    authContinue();
  };
  dialog.hidden = false;
  button.addEventListener('click', consentGranted, false);
}

/**
 * Entry point for app initialization.
 */
remoting.init = function() {
  // TODO(jamiewalch): Remove this when we migrate to apps v2
  // (http://crbug.com/ 134213).
  remoting.initMockStorage();

  remoting.logExtensionInfo_();
  l10n.localize();
  // Create global objects.
  remoting.settings = new remoting.Settings();
  remoting.oauth2 = new remoting.OAuth2();
  // TODO(jamiewalch): Reinstate this when we migrate to apps v2
  // (http://crbug.com/ 134213).
  // remoting.identity = new remoting.Identity(consentRequired_);
  remoting.identity = remoting.oauth2;
  remoting.stats = new remoting.ConnectionStats(
      document.getElementById('statistics'));
  remoting.formatIq = new remoting.FormatIq();
  remoting.hostList = new remoting.HostList(
      document.getElementById('host-list'),
      document.getElementById('host-list-empty'),
      document.getElementById('host-list-error-message'),
      document.getElementById('host-list-refresh-failed-button'));
  remoting.toolbar = new remoting.Toolbar(
      document.getElementById('session-toolbar'));
  remoting.clipboard = new remoting.Clipboard();
  var sandbox = /** @type {HTMLIFrameElement} */
      document.getElementById('wcs-sandbox');
  remoting.wcsSandbox = new remoting.WcsSandboxContainer(sandbox.contentWindow);

  remoting.identity.getEmail(remoting.onEmail, remoting.showErrorMessage);

  remoting.showOrHideIT2MeUi();
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

  var onLoad = function() {
    // Parse URL parameters.
    var urlParams = getUrlParameters_();
    if ('mode' in urlParams) {
      if (urlParams['mode'] == 'me2me') {
        var hostId = urlParams['hostId'];
        remoting.connectMe2Me(hostId);
        return;
      }
    }
    // No valid URL parameters, start up normally.
    remoting.initHomeScreenUi();
  }
  remoting.hostList.load(onLoad);

  // Show the tab-type warnings if necessary.
  /** @param {boolean} isWindowed */
  var onIsWindowed = function(isWindowed) {
    if (!isWindowed &&
        navigator.platform.indexOf('Mac') == -1) {
      document.getElementById('startup-mode-box-me2me').hidden = false;
      document.getElementById('startup-mode-box-it2me').hidden = false;
    }
  };
  isWindowed_(onIsWindowed);
};

/**
 * Display the user's email address and allow access to the rest of the app,
 * including parsing URL parameters.
 *
 * @param {string} email The user's email address.
 * @return {void} Nothing.
 */
remoting.onEmail = function(email) {
  document.getElementById('current-email').innerText = email;
  document.getElementById('get-started-it2me').disabled = false;
  document.getElementById('get-started-me2me').disabled = false;
};

/**
 * initHomeScreenUi is called if the app is not starting up in session mode,
 * and also if the user cancels pin entry or the connection in session mode.
 */
remoting.initHomeScreenUi = function() {
  remoting.hostController = new remoting.HostController();
  document.getElementById('share-button').disabled =
      !remoting.hostController.isPluginSupported();
  remoting.setMode(remoting.AppMode.HOME);
  if (!remoting.oauth2.isAuthenticated()) {
    document.getElementById('auth-dialog').hidden = false;
  }
  remoting.hostSetupDialog =
      new remoting.HostSetupDialog(remoting.hostController);
  // Display the cached host list, then asynchronously update and re-display it.
  remoting.updateLocalHostState();
  remoting.hostList.refresh(remoting.updateLocalHostState);
  remoting.initSurvey();
};

/**
 * Fetches local host state and updates host list accordingly.
 */
remoting.updateLocalHostState = function() {
  /**
   * @param {remoting.HostController.State} state Host state.
   * @param {string?} localHostId
   */
  var onHostState = function(state, localHostId) {
    remoting.hostList.setLocalHostStateAndId(state, localHostId);
    remoting.hostList.display();
  };
  remoting.hostController.getLocalHostStateAndId(onHostState);
};

/**
 * Log information about the current extension.
 * The extension manifest is parsed to extract this info.
 */
remoting.logExtensionInfo_ = function() {
  var manifest = chrome.runtime.getManifest();
  if (manifest && manifest.version) {
    var name = chrome.i18n.getMessage('PRODUCT_NAME');
    console.log(name + ' version: ' + manifest.version);
  } else {
    console.error('Failed to get product version. Corrupt manifest?');
  }
};

/**
 * If an IT2Me client or host is active then prompt the user before closing.
 * If a Me2Me client is active then don't bother, since closing the window is
 * the more intuitive way to end a Me2Me session, and re-connecting is easy.
 *
 * @return {?string} The prompt string if a connection is active.
 */
remoting.promptClose = function() {
  if (!remoting.clientSession ||
      remoting.clientSession.mode == remoting.ClientSession.Mode.ME2ME) {
    return null;
  }
  switch (remoting.currentMode) {
    case remoting.AppMode.CLIENT_CONNECTING:
    case remoting.AppMode.HOST_WAITING_FOR_CODE:
    case remoting.AppMode.HOST_WAITING_FOR_CONNECTION:
    case remoting.AppMode.HOST_SHARED:
    case remoting.AppMode.IN_SESSION:
      return chrome.i18n.getMessage(/*i18n-content*/'CLOSE_PROMPT');
    default:
      return null;
  }
};

/**
 * Sign the user out of Chromoting by clearing (and revoking, if possible) the
 * OAuth refresh token.
 *
 * Also clear all local storage, to avoid leaking information.
 */
remoting.signOut = function() {
  remoting.oauth2.clear();
  remoting.storage.local.clear();
  remoting.setMode(remoting.AppMode.HOME);
  document.getElementById('auth-dialog').hidden = false;
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

/**
 * Return the current time as a formatted string suitable for logging.
 *
 * @return {string} The current time, formatted as [mmdd/hhmmss.xyz]
 */
remoting.timestamp = function() {
  /**
   * @param {number} num A number.
   * @param {number} len The required length of the answer.
   * @return {string} The number, formatted as a string of the specified length
   *     by prepending zeroes as necessary.
   */
  var pad = function(num, len) {
    var result = num.toString();
    if (result.length < len) {
      result = new Array(len - result.length + 1).join('0') + result;
    }
    return result;
  };
  var now = new Date();
  var timestamp = pad(now.getMonth() + 1, 2) + pad(now.getDate(), 2) + '/' +
      pad(now.getHours(), 2) + pad(now.getMinutes(), 2) +
      pad(now.getSeconds(), 2) + '.' + pad(now.getMilliseconds(), 3);
  return '[' + timestamp + ']';
};

/**
 * Show an error message, optionally including a short-cut for signing in to
 * Chromoting again.
 *
 * @param {remoting.Error} error
 * @return {void} Nothing.
 */
remoting.showErrorMessage = function(error) {
  l10n.localizeElementFromTag(
      document.getElementById('token-refresh-error-message'),
      error);
  var auth_failed = (error == remoting.Error.AUTHENTICATION_FAILED);
  document.getElementById('token-refresh-auth-failed').hidden = !auth_failed;
  document.getElementById('token-refresh-other-error').hidden = auth_failed;
  remoting.setMode(remoting.AppMode.TOKEN_REFRESH_FAILED);
};

/**
 * Determine whether or not the app is running in a window.
 * @param {function(boolean):void} callback Callback to receive whether or not
 *     the current tab is running in windowed mode.
 */
function isWindowed_(callback) {
  /** @param {chrome.Window} win The current window. */
  var windowCallback = function(win) {
    callback(win.type == 'popup');
  };
  /** @param {chrome.Tab} tab The current tab. */
  var tabCallback = function(tab) {
    if (tab.pinned) {
      callback(false);
    } else {
      chrome.windows.get(tab.windowId, null, windowCallback);
    }
  };
  if (chrome.tabs) {
    chrome.tabs.getCurrent(tabCallback);
  } else {
    console.error('chome.tabs is not available.');
  }
}
