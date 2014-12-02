// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Entry point ('load' handler) for Chromoting webapp.
 */
remoting.initChromoting = function() {
  remoting.initGlobalObjects();
  remoting.initIdentity();
  remoting.initIdentityEmail(remoting.onEmailAvailable);

  remoting.initEventHandlers();

  if (base.isAppsV2()) {
    remoting.fullscreen = new remoting.FullscreenAppsV2();
    remoting.windowFrame = new remoting.WindowFrame(
        document.getElementById('title-bar'));
    remoting.optionsMenu = remoting.windowFrame.createOptionsMenu();
  } else {
    remoting.fullscreen = new remoting.FullscreenAppsV1();
    remoting.toolbar = new remoting.Toolbar(
        document.getElementById('session-toolbar'));
    remoting.optionsMenu = remoting.toolbar.createOptionsMenu();
  }

  remoting.initHostlist_();

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

  remoting.windowShape.updateClientWindowShape();

  remoting.showOrHideIT2MeUi();
  remoting.showOrHideMe2MeUi();

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

  remoting.ClientPlugin.factory.preloadPlugin();

}

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

window.addEventListener('load', remoting.initChromoting, false);
