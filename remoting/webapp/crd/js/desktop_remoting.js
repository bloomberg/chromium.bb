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
 * @param {Array<string>} appCapabilities Array of application capabilities.
 * @constructor
 * @implements {remoting.ApplicationInterface}
 * @extends {remoting.Application}
 */
remoting.DesktopRemoting = function(appCapabilities) {
  base.inherits(this, remoting.Application, appCapabilities);

  /**
   * Whether to refresh the JID and retry the connection if the current JID
   * is offline.
   *
   * @private {boolean}
   */
  this.refreshHostJidIfOffline_ = true;

  /** @private {remoting.DesktopConnectedView} */
  this.connectedView_ = null;
};

/**
 * @return {string} Application product name to be used in UI.
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.getApplicationName = function() {
  return chrome.i18n.getMessage(/*i18n-content*/'PRODUCT_NAME');
};

/**
 * @param {!remoting.Error} error The failure reason.
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.signInFailed_ = function(error) {
  remoting.showErrorMessage(error);
};

/**
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.initApplication_ = function() {
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

  remoting.initHostlist_(this.connectMe2Me_.bind(this));

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
 * @param {string} token An OAuth access token.
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.startApplication_ = function(token) {
  remoting.identity.getEmail().then(
      function(/** string */ email) {
        document.getElementById('current-email').innerText = email;
        document.getElementById('get-started-it2me').disabled = false;
        document.getElementById('get-started-me2me').disabled = false;
      });
};

/** @override {remoting.ApplicationInterface} */
remoting.DesktopRemoting.prototype.exitApplication_ = function() {
  this.closeMainWindow_();
};

/**
 * @param {remoting.ConnectionInfo} connectionInfo
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.onConnected_ = function(connectionInfo) {
  this.initSession_(connectionInfo);

  // Set the text on the buttons shown under the error message so that they are
  // easy to understand in the case where a successful connection failed, as
  // opposed to the case where a connection never succeeded.
  // TODO(garykac): Investigate to see if these need to be reverted to their
  // original values in the onDisconnected_ method.
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

  if (remoting.app.getConnectionMode() === remoting.Application.Mode.ME2ME) {
    if (remoting.app.hasCapability(remoting.ClientSession.Capability.CAST)) {
      this.sessionConnector_.registerProtocolExtension(
          new remoting.CastExtensionHandler());
    }
    this.sessionConnector_.registerProtocolExtension(
        new remoting.GnubbyAuthHandler());
  }
  if (connectionInfo.session().hasCapability(
          remoting.ClientSession.Capability.VIDEO_RECORDER)) {
    var recorder = new remoting.VideoFrameRecorder();
    this.sessionConnector_.registerProtocolExtension(recorder);
    this.connectedView_.setVideoFrameRecorder(recorder);
  }

  if (remoting.pairingRequested) {
    var that = this;
    /**
     * @param {string} clientId
     * @param {string} sharedSecret
     */
    var onPairingComplete = function(clientId, sharedSecret) {
      var connector = that.sessionConnector_;
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
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.onDisconnected_ = function() {
  var mode = this.getConnectionMode();
  if (mode === remoting.Application.Mode.IT2ME) {
    remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_IT2ME);
  } else {
    remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED_ME2ME);
  }
  base.dispose(this.connectedView_);
  this.connectedView_ = null;
};

/**
 * @param {!remoting.Error} error
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.onConnectionFailed_ = function(error) {
  var that = this;
  var onHostListRefresh = function(/** boolean */ success) {
    if (success) {
      var connector = that.sessionConnector_;
      var host = remoting.hostList.getHostForId(connector.getHostId());
      if (host) {
        connector.retryConnectMe2Me(host);
        return;
      }
    }
    that.onError_(error);
  };

  var mode = this.getConnectionMode();
  if (error.hasTag(remoting.Error.Tag.HOST_IS_OFFLINE) &&
      mode === remoting.Application.Mode.ME2ME &&
      this.refreshHostJidIfOffline_) {
    this.refreshHostJidIfOffline_ = false;

    // The plugin will be re-created when the host finished refreshing
    remoting.hostList.refresh(onHostListRefresh);
  } else {
    this.onError_(error);
  }
};

/**
 * @param {!remoting.Error} error The error to be localized and displayed.
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.onError_ = function(error) {
  console.error('Connection failed: ' + error.toString());
  var mode = this.getConnectionMode();
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

  if (mode == remoting.Application.Mode.IT2ME) {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME);
  } else {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_ME2ME);
  }
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
  if (this.getConnectionMode() === remoting.Application.Mode.IT2ME) {
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
 * Entry-point for Me2Me connections.
 *
 * @param {string} hostId The unique id of the host.
 * @return {void} Nothing.
 * @private
 */
remoting.DesktopRemoting.prototype.connectMe2Me_ = function(hostId) {
  var host = remoting.hostList.getHostForId(hostId);
  var flow = new remoting.Me2MeConnectFlow(this.sessionConnector_, host);
  flow.start();
};