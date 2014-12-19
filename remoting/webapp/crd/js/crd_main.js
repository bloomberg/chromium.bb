// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Display the user's email address and allow access to the rest of the app,
 * including parsing URL parameters.
 *
 * @param {string} email The user's email address.
 * @return {void} Nothing.
 */
remoting.onEmailAvailable = function(email, fullName) {
  document.getElementById('current-email').innerText = email;
  document.getElementById('get-started-it2me').disabled = false;
  document.getElementById('get-started-me2me').disabled = false;
};

/**
 * Initialize the host list.
 */
remoting.initHostlist_ = function() {
  remoting.hostList = new remoting.HostList(
      document.getElementById('host-list'),
      document.getElementById('host-list-empty'),
      document.getElementById('host-list-error-message'),
      document.getElementById('host-list-refresh-failed-button'),
      document.getElementById('host-list-loading-indicator'));

  isHostModeSupported_().then(
    /** @param {Boolean} supported */
    function(supported){
      if (supported) {
        var noShare = document.getElementById('chrome-os-no-share');
        noShare.parentNode.removeChild(noShare);
      } else {
        var button = document.getElementById('share-button');
        button.disabled = true;
      }
    });

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
          var connector = remoting.app.getSessionConnector();
          remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
          connector.connectIT2Me(accessCode);

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
}

/**
 * Returns whether Host mode is supported on this platform for It2me.
 * TODO(kelvinp): Remove this function once It2me is enabled on Chrome OS (See
 * crbug.com/429860).
 *
 * @return {Promise} Resolves to true if Host mode is supported.
 */
function isHostModeSupported_() {
  if (!remoting.platformIsChromeOS()) {
    return Promise.resolve(true);
  }
  // Sharing on Chrome OS is currently behind a flag.
  // isInstalled() will return false if the flag is disabled.
  var hostInstaller = new remoting.HostInstaller();
  return hostInstaller.isInstalled();
}

/**
 * initHomeScreenUi is called if the app is not starting up in session mode,
 * and also if the user cancels pin entry or the connection in session mode.
 */
remoting.initHomeScreenUi = function() {
  remoting.hostController = new remoting.HostController();
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
 * Entry point for test code. In order to make test and production
 * code as similar as possible, the same entry point is used for
 * production code, but since production code should never have
 * 'source' set to 'test', it continue with initialization
 * immediately. As a fail-safe, the mechanism by which initialization
 * completes when under test is controlled by a simple UI, making it
 * possible to use the app even if the previous assumption fails to
 * hold in some corner cases.
 */
remoting.startDesktopRemotingForTesting = function() {
  var urlParams = getUrlParameters_();
  if (urlParams['source'] === 'test') {
    document.getElementById('browser-test-continue-init').addEventListener(
        'click', remoting.startDesktopRemoting, false);
    document.getElementById('browser-test-deferred-init').hidden = false;
  } else {
    remoting.startDesktopRemoting();
  }
}


remoting.startDesktopRemoting = function() {
  remoting.app = new remoting.Application();
  var desktop_remoting = new remoting.DesktopRemoting(remoting.app);
  remoting.app.start();
};

window.addEventListener('load', remoting.startDesktopRemotingForTesting, false);
