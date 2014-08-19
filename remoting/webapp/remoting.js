// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @type {remoting.HostSession} */ remoting.hostSession = null;

/**
 * @type {base.EventSource} An event source object for handling global events.
 *    This is an interim hack.  Eventually, we should move functionalities
 *    away from the remoting namespace and into smaller objects.
 */
remoting.testEvents;

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
  if (base.isAppsV2()) {
    var htmlNode = /** @type {HTMLElement} */ (document.body.parentNode);
    htmlNode.classList.add('apps-v2');
  } else {
    migrateLocalToChromeStorage_();
  }

  console.log(remoting.getExtensionInfo());
  l10n.localize();

  // Create global objects.
  remoting.settings = new remoting.Settings();
  if (base.isAppsV2()) {
    remoting.identity = new remoting.Identity(consentRequired_);
    remoting.fullscreen = new remoting.FullscreenAppsV2();
    remoting.windowFrame = new remoting.WindowFrame(
        document.getElementById('title-bar'));
  } else {
    remoting.oauth2 = new remoting.OAuth2();
    if (!remoting.oauth2.isAuthenticated()) {
      document.getElementById('auth-dialog').hidden = false;
    }
    remoting.identity = remoting.oauth2;
    remoting.fullscreen = new remoting.FullscreenAppsV1();
  }
  remoting.stats = new remoting.ConnectionStats(
      document.getElementById('statistics'));
  remoting.formatIq = new remoting.FormatIq();
  remoting.hostList = new remoting.HostList(
      document.getElementById('host-list'),
      document.getElementById('host-list-empty'),
      document.getElementById('host-list-error-message'),
      document.getElementById('host-list-refresh-failed-button'),
      document.getElementById('host-list-loading-indicator'));
  remoting.toolbar = new remoting.Toolbar(
      document.getElementById('session-toolbar'));
  remoting.clipboard = new remoting.Clipboard();
  var sandbox = /** @type {HTMLIFrameElement} */
      document.getElementById('wcs-sandbox');
  remoting.wcsSandbox = new remoting.WcsSandboxContainer(sandbox.contentWindow);
  var homeFeedback = new remoting.MenuButton(
      document.getElementById('help-feedback-main'));
  var toolbarFeedback = new remoting.MenuButton(
      document.getElementById('help-feedback-toolbar'));
  remoting.manageHelpAndFeedback(
      document.getElementById('title-bar'));
  remoting.manageHelpAndFeedback(
      document.getElementById('help-feedback-toolbar'));
  remoting.manageHelpAndFeedback(
      document.getElementById('help-feedback-main'));

  /** @param {remoting.Error} error */
  var onGetEmailError = function(error) {
    // No need to show the error message for NOT_AUTHENTICATED
    // because we will show "auth-dialog".
    if (error != remoting.Error.NOT_AUTHENTICATED) {
      remoting.showErrorMessage(error);
    }
  }
  remoting.identity.getEmail(remoting.onEmail, onGetEmailError);

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

  /**
   * @return {Promise} A promise that resolves to the id of the current
   * containing tab/window.
   */
  var getCurrentId = function () {
    if (base.isAppsV2()) {
      return Promise.resolve(chrome.app.window.current().id);
    }

    /**
     * @param {function(*=):void} resolve
     * @param {function(*=):void} reject
     */
    return new Promise(function(resolve, reject) {
      /** @param {chrome.Tab} tab */
      chrome.tabs.getCurrent(function(tab){
        if (tab) {
          resolve(String(tab.id));
        }
        reject('Cannot retrieve the current tab.');
      });
    });
  };

  var onLoad = function() {
    // Parse URL parameters.
    var urlParams = getUrlParameters_();
    if ('mode' in urlParams) {
      if (urlParams['mode'] === 'me2me') {
        var hostId = urlParams['hostId'];
        remoting.connectMe2Me(hostId);
        return;
      } else if (urlParams['mode'] === 'hangout') {
        /** @param {*} id */
        getCurrentId().then(function(id) {
          /** @type {string} */
          var accessCode = urlParams['accessCode'];
          remoting.ensureSessionConnector_();
          remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
          remoting.connector.connectIT2Me(accessCode);

          document.body.classList.add('hangout-remote-desktop');
          var senderId = /** @type {string} */ String(id);
          var hangoutSession = new remoting.HangoutSession(senderId);
          hangoutSession.init();
        });
        return;
      }
    }
    // No valid URL parameters, start up normally.
    remoting.initHomeScreenUi();
  }
  remoting.hostList.load(onLoad);

  // For Apps v1, check the tab type to warn the user if they are not getting
  // the best keyboard experience.
  if (!base.isAppsV2() && !remoting.platformIsMac()) {
    /** @param {boolean} isWindowed */
    var onIsWindowed = function(isWindowed) {
      if (!isWindowed) {
        document.getElementById('startup-mode-box-me2me').hidden = false;
        document.getElementById('startup-mode-box-it2me').hidden = false;
      }
    };
    isWindowed_(onIsWindowed);
  }

  remoting.testEvents = new base.EventSource();

  /** @enum {string} */
  remoting.testEvents.Names = {
    uiModeChanged: 'uiModeChanged'
  };
  remoting.testEvents.defineEvents(base.values(remoting.testEvents.Names));

  remoting.ClientPlugin.preload();
};

/**
 * Returns whether or not IT2Me is supported via the host NPAPI plugin.
 *
 * @return {boolean}
 */
function isIT2MeSupported_() {
  // Currently, IT2Me on Chromebooks is not supported.
  return !remoting.runningOnChromeOS();
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
  document.getElementById('share-button').disabled = !isIT2MeSupported_();
  remoting.setMode(remoting.AppMode.HOME);
  remoting.hostSetupDialog =
      new remoting.HostSetupDialog(remoting.hostController);
  var dialog = document.getElementById('paired-clients-list');
  var message = document.getElementById('paired-client-manager-message');
  var deleteAll = document.getElementById('delete-all-paired-clients');
  var close = document.getElementById('close-paired-client-manager-dialog');
  var working = document.getElementById('paired-client-manager-dialog-working');
  var error = document.getElementById('paired-client-manager-dialog-error');
  var noPairedClients = document.getElementById('no-paired-clients');
  remoting.pairedClientManager =
      new remoting.PairedClientManager(remoting.hostController, dialog, message,
                                       deleteAll, close, noPairedClients,
                                       working, error);
  // Display the cached host list, then asynchronously update and re-display it.
  remoting.updateLocalHostState();
  remoting.hostList.refresh(remoting.updateLocalHostState);
  remoting.butterBar = new remoting.ButterBar();
};

/**
 * Fetches local host state and updates the DOM accordingly.
 */
remoting.updateLocalHostState = function() {
  /**
   * @param {remoting.HostController.State} state Host state.
   */
  var onHostState = function(state) {
    if (state == remoting.HostController.State.STARTED) {
      remoting.hostController.getLocalHostId(onHostId.bind(null, state));
    } else {
      onHostId(state, null);
    }
  };

  /**
   * @param {remoting.HostController.State} state Host state.
   * @param {string?} hostId Host id.
   */
  var onHostId = function(state, hostId) {
    remoting.hostList.setLocalHostStateAndId(state, hostId);
    remoting.hostList.display();
  };

  /**
   * @param {boolean} response True if the feature is present.
   */
  var onHasFeatureResponse = function(response) {
    /**
     * @param {remoting.Error} error
     */
    var onError = function(error) {
      console.error('Failed to get pairing status: ' + error);
      remoting.pairedClientManager.setPairedClients([]);
    };

    if (response) {
      remoting.hostController.getPairedClients(
          remoting.pairedClientManager.setPairedClients.bind(
              remoting.pairedClientManager),
          onError);
    } else {
      console.log('Pairing registry not supported by host.');
      remoting.pairedClientManager.setPairedClients([]);
    }
  };

  remoting.hostController.hasFeature(
      remoting.HostController.Feature.PAIRING_REGISTRY, onHasFeatureResponse);
  remoting.hostController.getLocalHostState(onHostState);
};

/**
 * @return {string} Information about the current extension.
 */
remoting.getExtensionInfo = function() {
  var v2OrLegacy = base.isAppsV2() ? " (v2)" : " (legacy)";
  var manifest = chrome.runtime.getManifest();
  if (manifest && manifest.version) {
    var name = chrome.i18n.getMessage('PRODUCT_NAME');
    return name + ' version: ' + manifest.version + v2OrLegacy;
  } else {
    return 'Failed to get product version. Corrupt manifest?';
  }
};

/**
 * Returns Chrome version.
 * @return {string?}
 */
remoting.getChromeVersion = function() {
  var match = new RegExp('Chrome/([0-9.]*)').exec(navigator.userAgent);
  if (match && (match.length >= 2)) {
    return match[1];
  }
  return null;
};

/**
 * If an IT2Me client or host is active then prompt the user before closing.
 * If a Me2Me client is active then don't bother, since closing the window is
 * the more intuitive way to end a Me2Me session, and re-connecting is easy.
 */
remoting.promptClose = function() {
  if (!remoting.clientSession ||
      remoting.clientSession.getMode() == remoting.ClientSession.Mode.ME2ME) {
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
  chrome.storage.local.clear();
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

/**
 * Migrate settings in window.localStorage to chrome.storage.local so that
 * users of older web-apps that used the former do not lose their settings.
 */
function migrateLocalToChromeStorage_() {
  // The OAuth2 class still uses window.localStorage, so don't migrate any of
  // those settings.
  var oauthSettings = [
      'oauth2-refresh-token',
      'oauth2-refresh-token-revokable',
      'oauth2-access-token',
      'oauth2-xsrf-token',
      'remoting-email'
  ];
  for (var setting in window.localStorage) {
    if (oauthSettings.indexOf(setting) == -1) {
      var copy = {}
      copy[setting] = window.localStorage.getItem(setting);
      chrome.storage.local.set(copy);
      window.localStorage.removeItem(setting);
    }
  }
}

/**
 * Generate a nonce, to be used as an xsrf protection token.
 *
 * @return {string} A URL-Safe Base64-encoded 128-bit random value. */
remoting.generateXsrfToken = function() {
  var random = new Uint8Array(16);
  window.crypto.getRandomValues(random);
  var base64Token = window.btoa(String.fromCharCode.apply(null, random));
  return base64Token.replace(/\+/g, '-').replace(/\//g, '_').replace(/=/g, '');
};

/**
 * Tests whether we are running on Mac.
 *
 * @return {boolean} True if the platform is Mac.
 */
remoting.platformIsMac = function() {
  return navigator.platform.indexOf('Mac') != -1;
}

/**
 * Tests whether we are running on Windows.
 *
 * @return {boolean} True if the platform is Windows.
 */
remoting.platformIsWindows = function() {
  return (navigator.platform.indexOf('Win32') != -1) ||
         (navigator.platform.indexOf('Win64') != -1);
}

/**
 * Tests whether we are running on Linux.
 *
 * @return {boolean} True if the platform is Linux.
 */
remoting.platformIsLinux = function() {
  return (navigator.platform.indexOf('Linux') != -1) &&
         !remoting.platformIsChromeOS();
}

/**
 * Tests whether we are running on ChromeOS.
 *
 * @return {boolean} True if the platform is ChromeOS.
 */
remoting.platformIsChromeOS = function() {
  return navigator.userAgent.match(/\bCrOS\b/) != null;
}
