// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This class implements the functionality that is specific to desktop
 * remoting ("Chromoting" or CRD).
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {remoting.Application} app The main app that owns this delegate.
 * @constructor
 * @implements {remoting.Application.Delegate}
 */
remoting.DesktopRemoting = function(app) {
  /**
   * TODO(garykac): Remove this reference to the Application. It's only
   * needed to get the current mode when reporting errors. So we should be
   * able to refactor and remove this reference cycle.
   *
   * @private {remoting.Application}
   */
  this.app_ = app;
  app.setDelegate(this);

  /**
   * Whether to refresh the JID and retry the connection if the current JID
   * is offline.
   *
   * @private {boolean}
   */
  this.refreshHostJidIfOffline_ = true;
};

/**
 * Initialize the application and register all event handlers. After this
 * is called, the app is running and waiting for user events.
 *
 * @return {void} Nothing.
 */
remoting.DesktopRemoting.prototype.init = function() {
  remoting.initElementEventHandlers();

  if (base.isAppsV2()) {
    remoting.windowFrame = new remoting.WindowFrame(
        document.getElementById('title-bar'));
    remoting.optionsMenu = remoting.windowFrame.createOptionsMenu();

    var START_FULLSCREEN = 'start-fullscreen';
    remoting.fullscreen = new remoting.FullscreenAppsV2();
    remoting.fullscreen.addListener(function(isFullscreen) {
      chrome.storage.local.set({START_FULLSCREEN: isFullscreen});
    });
    // TODO(jamiewalch): This should be handled by the background page when the
    // window is created, but due to crbug.com/51587 it needs to be done here.
    // Remove this hack once that bug is fixed.
    chrome.storage.local.get(
        START_FULLSCREEN,
        /** @param {Object} values */
        function(values) {
          if (values[START_FULLSCREEN]) {
            remoting.fullscreen.activate(true);
          }
        }
    );

  } else {
    remoting.fullscreen = new remoting.FullscreenAppsV1();
    remoting.toolbar = new remoting.Toolbar(
        document.getElementById('session-toolbar'));
    remoting.optionsMenu = remoting.toolbar.createOptionsMenu();

    window.addEventListener('beforeunload', remoting.promptClose, false);
    window.addEventListener('unload',
                            remoting.app.disconnect.bind(remoting.app), false);
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
 * Start the application. Once start() is called, the delegate can assume that
 * the user has consented to all permissions specified in the manifest.
 *
 * @param {remoting.SessionConnector} connector
 * @param {string} token An OAuth access token. The delegate should not cache
 *     this token, but can assume that it will remain valid during application
 *     start-up.
 */
remoting.DesktopRemoting.prototype.start = function(connector, token) {
  remoting.identity.getEmail().then(
      function(/** string */ email) {
        document.getElementById('current-email').innerText = email;
        document.getElementById('get-started-it2me').disabled = false;
        document.getElementById('get-started-me2me').disabled = false;
      });
};

/**
 * Report an authentication error to the user. This is called in lieu of start()
 * if the user cannot be authenticated or if they decline the app permissions.
 *
 * @param {remoting.Error} error The failure reason.
 */
remoting.DesktopRemoting.prototype.signInFailed = function(error) {
  remoting.showErrorMessage(error);
};

/**
 * @return {string} Application product name to be used in UI.
 */
remoting.DesktopRemoting.prototype.getApplicationName = function() {
  return chrome.i18n.getMessage(/*i18n-content*/'PRODUCT_NAME');
};

/**
 * @return {string} The default remap keys for the current platform.
 */
remoting.DesktopRemoting.prototype.getDefaultRemapKeys = function() {
  var remapKeys = '';
  // By default, under ChromeOS, remap the right Control key to the right
  // Win / Cmd key.
  if (remoting.platformIsChromeOS()) {
    remapKeys = '0x0700e4>0x0700e7';
  }
  return remapKeys;
};

/**
 * Called when a new session has been connected.
 *
 * @param {remoting.ClientSession} clientSession
 * @return {void} Nothing.
 */
remoting.DesktopRemoting.prototype.handleConnected = function(clientSession) {
  // Set the text on the buttons shown under the error message so that they are
  // easy to understand in the case where a successful connection failed, as
  // opposed to the case where a connection never succeeded.
  // TODO(garykac): Investigate to see if these need to be reverted to their
  // original values in the onDisconnected method.
  var button1 = document.getElementById('client-reconnect-button');
  l10n.localizeElementFromTag(button1, /*i18n-content*/'RECONNECT');
  button1.removeAttribute('autofocus');
  var button2 = document.getElementById('client-finished-me2me-button');
  l10n.localizeElementFromTag(button2, /*i18n-content*/'OK');
  button2.setAttribute('autofocus', 'autofocus');

  // Reset the refresh flag so that the next connection will retry if needed.
  this.refreshHostJidIfOffline_ = true;

  document.getElementById('access-code-entry').value = '';
  remoting.setMode(remoting.AppMode.IN_SESSION);
  if (!base.isAppsV2()) {
    remoting.toolbar.center();
    remoting.toolbar.preview();
  }

  if (remoting.pairingRequested) {
    /**
     * @param {string} clientId
     * @param {string} sharedSecret
     */
    var onPairingComplete = function(clientId, sharedSecret) {
      var pairingInfo = {
        pairingInfo: {
          clientId: clientId,
          sharedSecret: sharedSecret
        }
      };
      var connector = remoting.app.getSessionConnector();
      remoting.HostSettings.save(connector.getHostId(), pairingInfo);
      connector.updatePairingInfo(clientId, sharedSecret);
    };
    // Use the platform name as a proxy for the local computer name.
    // TODO(jamiewalch): Use a descriptive name for the local computer, for
    // example, its Chrome Sync name.
    var clientName = '';
    if (remoting.platformIsMac()) {
      clientName = 'Mac';
    } else if (remoting.platformIsWindows()) {
      clientName = 'Windows';
    } else if (remoting.platformIsChromeOS()) {
      clientName = 'ChromeOS';
    } else if (remoting.platformIsLinux()) {
      clientName = 'Linux';
    } else {
      console.log('Unrecognized client platform. Using navigator.platform.');
      clientName = navigator.platform;
    }
    clientSession.requestPairing(clientName, onPairingComplete);
  }
};

/**
 * Called when the current session has been disconnected.
 *
 * @return {void} Nothing.
 */
remoting.DesktopRemoting.prototype.handleDisconnected = function() {
  if (remoting.desktopConnectedView.getMode() ==
      remoting.DesktopConnectedView.Mode.IT2ME) {
    remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_IT2ME);
  } else {
    remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_ME2ME);
  }
};

/**
 * Called when the current session's connection has failed.
 *
 * @param {remoting.SessionConnector} connector
 * @param {remoting.Error} error
 * @return {void} Nothing.
 */
remoting.DesktopRemoting.prototype.handleConnectionFailed = function(
    connector, error) {
  /** @type {remoting.DesktopRemoting} */
  var that = this;

  /** @param {boolean} success */
  var onHostListRefresh = function(success) {
    if (success) {
      var host = remoting.hostList.getHostForId(connector.getHostId());
      if (host) {
        connector.retryConnectMe2Me(host);
        return;
      }
    }
    that.handleError(error);
  };

  if (error == remoting.Error.HOST_IS_OFFLINE &&
      that.refreshHostJidIfOffline_) {
    that.refreshHostJidIfOffline_ = false;
    // The plugin will be re-created when the host finished refreshing
    remoting.hostList.refresh(onHostListRefresh);
  } else {
    this.handleError(error);
  }
};

/**
 * Called when the current session has reached the point where the host has
 * started streaming video frames to the client.
 *
 * @return {void} Nothing.
 */
remoting.DesktopRemoting.prototype.handleVideoStreamingStarted = function() {
};

/**
 * @param {string} type The type of the extension message.
 * @param {Object} message The parsed extension message data.
 * @return {boolean} Return true if the extension message was recognized.
 */
remoting.DesktopRemoting.prototype.handleExtensionMessage = function(
    type, message) {
  return false;
};

/**
 * Called when an error needs to be displayed to the user.
 *
 * @param {remoting.Error} errorTag The error to be localized and displayed.
 * @return {void} Nothing.
 */
remoting.DesktopRemoting.prototype.handleError = function(errorTag) {
  console.error('Connection failed: ' + errorTag);
  remoting.accessCode = '';

  if (errorTag === remoting.Error.AUTHENTICATION_FAILED) {
    remoting.setMode(remoting.AppMode.HOME);
    remoting.handleAuthFailureAndRelaunch();
    return;
  }

  // Reset the refresh flag so that the next connection will retry if needed.
  this.refreshHostJidIfOffline_ = true;

  var errorDiv = document.getElementById('connect-error-message');
  l10n.localizeElementFromTag(errorDiv, /** @type {string} */ (errorTag));

  var mode = remoting.clientSession ? remoting.desktopConnectedView.getMode()
      : this.app_.getSessionConnector().getConnectionMode();
  if (mode == remoting.DesktopConnectedView.Mode.IT2ME) {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME);
  } else {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_ME2ME);
  }
};

/**
 * No cleanup required for desktop remoting.
 */
remoting.DesktopRemoting.prototype.handleExit = function() {
};
