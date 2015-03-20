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

  /** @private {remoting.DesktopConnectedView} */
  this.connectedView_ = null;

  remoting.desktopDelegateForTesting = this;
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

    window.addEventListener('beforeunload',
                            this.promptClose_.bind(this), false);
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
    this.isWindowed_(onIsWindowed);
  }

  remoting.ClientPlugin.factory.preloadPlugin();
};

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
 * @param {!remoting.Error} error The failure reason.
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
 * Called when a new session has been connected.
 *
 * @param {remoting.ConnectionInfo} connectionInfo
 * @return {void} Nothing.
 */
remoting.DesktopRemoting.prototype.handleConnected = function(connectionInfo) {
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

  this.connectedView_ = new remoting.DesktopConnectedView(
      document.getElementById('client-container'), connectionInfo);

  // By default, under ChromeOS, remap the right Control key to the right
  // Win / Cmd key.
  if (remoting.platformIsChromeOS()) {
    connectionInfo.plugin().setRemapKeys('0x0700e4>0x0700e7');
  }

  if (connectionInfo.mode() === remoting.DesktopConnectedView.Mode.ME2ME) {
    var sessionConnector = remoting.app.getSessionConnector();
    if (remoting.app.hasCapability(remoting.ClientSession.Capability.CAST)) {
      sessionConnector.registerProtocolExtension(
          new remoting.CastExtensionHandler());
    }
    sessionConnector.registerProtocolExtension(
        new remoting.GnubbyAuthHandler());
  }

  if (remoting.pairingRequested) {
    /**
     * @param {string} clientId
     * @param {string} sharedSecret
     */
    var onPairingComplete = function(clientId, sharedSecret) {
      var connector = remoting.app.getSessionConnector();
      var host = remoting.hostList.getHostForId(connector.getHostId());
      host.options.pairingInfo.clientId = clientId;
      host.options.pairingInfo.sharedSecret = sharedSecret;
      host.options.save();
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
    connectionInfo.session().requestPairing(clientName, onPairingComplete);
  }
};

/**
 * Called when the current session has been disconnected.
 *
 * @return {void} Nothing.
 */
remoting.DesktopRemoting.prototype.handleDisconnected = function() {
  var mode = this.connectedView_.getMode();
  if (mode === remoting.DesktopConnectedView.Mode.IT2ME) {
    remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_IT2ME);
  } else {
    remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_ME2ME);
  }
  base.dispose(this.connectedView_);
  this.connectedView_ = null;
};

/**
 * Called when the current session's connection has failed.
 *
 * @param {remoting.SessionConnector} connector
 * @param {!remoting.Error} error
 * @return {void} Nothing.
 */
remoting.DesktopRemoting.prototype.handleConnectionFailed = function(
    connector, error) {
  var that = this;
  var onHostListRefresh = function(/** boolean */ success) {
    if (success) {
      var host = remoting.hostList.getHostForId(connector.getHostId());
      if (host) {
        connector.retryConnectMe2Me(host);
        return;
      }
    }
    that.handleError(error);
  };

  var mode = this.app_.getSessionConnector().getConnectionMode();
  if (error.hasTag(remoting.Error.Tag.HOST_IS_OFFLINE) &&
      mode === remoting.DesktopConnectedView.Mode.ME2ME &&
      this.refreshHostJidIfOffline_) {
    this.refreshHostJidIfOffline_ = false;

    // The plugin will be re-created when the host finished refreshing
    remoting.hostList.refresh(onHostListRefresh);
  } else {
    this.handleError(error);
  }
};

/**
 * Called when an error needs to be displayed to the user.
 *
 * @param {!remoting.Error} error The error to be localized and displayed.
 * @return {void} Nothing.
 */
remoting.DesktopRemoting.prototype.handleError = function(error) {
  console.error('Connection failed: ' + error.toString());
  var mode = this.connectedView_ ? this.connectedView_.getMode()
      : this.app_.getSessionConnector().getConnectionMode();
  base.dispose(this.connectedView_);
  this.connectedView_ = null;

  if (error.hasTag(remoting.Error.Tag.AUTHENTICATION_FAILED)) {
    remoting.setMode(remoting.AppMode.HOME);
    remoting.handleAuthFailureAndRelaunch();
    return;
  }

  // Reset the refresh flag so that the next connection will retry if needed.
  this.refreshHostJidIfOffline_ = true;

  var errorDiv = document.getElementById('connect-error-message');
  l10n.localizeElementFromTag(errorDiv, error.getTag());

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

/**
 * Determine whether or not the app is running in a window.
 * @param {function(boolean):void} callback Callback to receive whether or not
 *     the current tab is running in windowed mode.
 * @private
 */
remoting.DesktopRemoting.prototype.isWindowed_ = function(callback) {
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
 * If an IT2Me client or host is active then prompt the user before closing.
 * If a Me2Me client is active then don't bother, since closing the window is
 * the more intuitive way to end a Me2Me session, and re-connecting is easy.
 * @private
 */
remoting.DesktopRemoting.prototype.promptClose_ = function() {
  var sessionConnector = remoting.app.getSessionConnector();
  if (sessionConnector &&
      sessionConnector.getConnectionMode() ==
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

/** @returns {remoting.DesktopConnectedView} */
remoting.DesktopRemoting.prototype.getConnectedViewForTesting = function() {
  return this.connectedView_;
};

/**
 * Global instance of remoting.DesktopRemoting used for testing.
 * @type {remoting.DesktopRemoting}
 */
remoting.desktopDelegateForTesting = null;
