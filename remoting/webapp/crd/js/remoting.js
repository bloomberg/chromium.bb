// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @type {base.EventSourceImpl} An event source object for handling global
 *    events. This is an interim hack.  Eventually, we should move
 *    functionalities away from the remoting namespace and into smaller objects.
 */
remoting.testEvents;

/**
 * Initialization tasks that are common to all remoting apps.
 */
remoting.initGlobalObjects = function() {
  if (base.isAppsV2()) {
    var htmlNode = /** @type {HTMLElement} */ (document.body.parentNode);
    htmlNode.classList.add('apps-v2');
  }

  console.log(remoting.getExtensionInfo());
  l10n.localize();

  remoting.stats = new remoting.ConnectionStats(
      document.getElementById('statistics'));
  remoting.formatIq = new remoting.FormatIq();

  remoting.clipboard = new remoting.Clipboard();
  var sandbox =
      /** @type {HTMLIFrameElement} */ (document.getElementById('wcs-sandbox'));
  remoting.wcsSandbox = new remoting.WcsSandboxContainer(sandbox.contentWindow);

  // The plugin's onFocus handler sends a paste command to |window|, because
  // it can't send one to the plugin element itself.
  window.addEventListener('paste', pluginGotPaste_, false);
  window.addEventListener('copy', pluginGotCopy_, false);

  remoting.initModalDialogs();

  remoting.testEvents = new base.EventSourceImpl();
  /** @enum {string} */
  remoting.testEvents.Names = {
    uiModeChanged: 'uiModeChanged'
  };
  remoting.testEvents.defineEvents(base.values(remoting.testEvents.Names));
}

/**
 * Returns true if the current platform is fully supported. It's only used when
 * we detect that host native messaging components are not installed. In that
 * case the result of this function determines if the webapp should show the
 * controls that allow to install and enable Me2Me host.
 *
 * @return {boolean}
 */
remoting.isMe2MeInstallable = function() {
  // The chromoting host is currently not installable on ChromeOS.
  // For Linux, we have a install package for Ubuntu but not other distros.
  // Since we cannot tell from javascript alone the Linux distro the client is
  // on, we don't show the daemon-control UI for Linux unless the host is
  // installed.
  return remoting.platformIsWindows() || remoting.platformIsMac();
}

/**
 * @return {string} Information about the current extension.
 */
remoting.getExtensionInfo = function() {
  var v2OrLegacy = base.isAppsV2() ? " (v2)" : " (legacy)";
  var manifest = chrome.runtime.getManifest();
  if (manifest && manifest.version) {
    var name = remoting.app.getApplicationName();
    return name + ' version: ' + manifest.version + v2OrLegacy;
  } else {
    return 'Failed to get product version. Corrupt manifest?';
  }
};

/**
 * If an IT2Me client or host is active then prompt the user before closing.
 * If a Me2Me client is active then don't bother, since closing the window is
 * the more intuitive way to end a Me2Me session, and re-connecting is easy.
 */
remoting.promptClose = function() {
  if (remoting.desktopConnectedView &&
      remoting.desktopConnectedView.getMode() ==
          remoting.DesktopConnectedView.Mode.IT2ME) {
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
  }
};

/**
 * Sign the user out of Chromoting by clearing (and revoking, if possible) the
 * OAuth refresh token.
 *
 * Also clear all local storage, to avoid leaking information.
 */
remoting.signOut = function() {
  remoting.oauth2.removeCachedAuthToken().then(function(){
    chrome.storage.local.clear();
    remoting.setMode(remoting.AppMode.HOME);
    window.location.reload();
  });
};

/**
 * Callback function called when the browser window gets a paste operation.
 *
 * @param {Event} event
 * @return {void} Nothing.
 */
function pluginGotPaste_(event) {
  if (event && event.clipboardData) {
    remoting.clipboard.toHost(event.clipboardData);
  }
}

/**
 * Callback function called when the browser window gets a copy operation.
 *
 * @param {Event} event
 * @return {void} Nothing.
 */
function pluginGotCopy_(event) {
  if (event && event.clipboardData) {
    if (remoting.clipboard.toOs(event.clipboardData)) {
      // The default action may overwrite items that we added to clipboardData.
      event.preventDefault();
    }
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
 * @param {!remoting.Error} error
 * @return {void} Nothing.
 */
remoting.showErrorMessage = function(error) {
  l10n.localizeElementFromTag(
      document.getElementById('token-refresh-error-message'),
      error.tag);
  var auth_failed = (error.tag == remoting.Error.Tag.AUTHENTICATION_FAILED);
  if (auth_failed && base.isAppsV2()) {
    remoting.handleAuthFailureAndRelaunch();
  } else {
    document.getElementById('token-refresh-auth-failed').hidden = !auth_failed;
    document.getElementById('token-refresh-other-error').hidden = auth_failed;
    remoting.setMode(remoting.AppMode.TOKEN_REFRESH_FAILED);
  }
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
