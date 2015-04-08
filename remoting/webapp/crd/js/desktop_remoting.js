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

  /** @private {remoting.DesktopConnectedView} */
  this.connectedView_ = null;

  /** @private {remoting.Activity} */
  this.activity_ = null;
};

/** @private */
remoting.DesktopRemoting.prototype.reset_ = function() {
  base.dispose(this.connectedView_);
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
  document.getElementById('access-mode-button').addEventListener(
      'click', this.connectIt2Me_.bind(this), false);

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

  if (connectionInfo.session().hasCapability(
          remoting.ClientSession.Capability.VIDEO_RECORDER)) {
    var recorder = new remoting.VideoFrameRecorder();
    connectionInfo.plugin().extensions().register(recorder);
    this.connectedView_.setVideoFrameRecorder(recorder);
  }

  this.activity_.onConnected(connectionInfo);
};

/**
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.onDisconnected_ = function() {
  this.activity_.onDisconnected();
  this.reset_();
};

/**
 * @param {!remoting.Error} error
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.onConnectionFailed_ = function(error) {
  this.activity_.onConnectionFailed(error);
};

/**
 * @param {!remoting.Error} error The error to be localized and displayed.
 * @override {remoting.ApplicationInterface}
 */
remoting.DesktopRemoting.prototype.onError_ = function(error) {
  console.error('Connection failed: ' + error.toString());

  if (error.hasTag(remoting.Error.Tag.AUTHENTICATION_FAILED)) {
    remoting.setMode(remoting.AppMode.HOME);
    remoting.handleAuthFailureAndRelaunch();
    return;
  }

  this.activity_.onError(error);
  this.reset_();
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
  // The Me2MeActivity triggers a reconnect underneath the hood on connection
  // failure, but it won't notify the DesktopRemoting upon successful
  // re-connection.  Therefore, we can't dispose the activity on connection
  // failure.  Instead, the activity is only disposed when a new one is
  // created.  This would be fixed once |sessionConnector| is moved out of the
  // application.
  base.dispose(this.activity_);
  this.activity_ = new remoting.Me2MeActivity(this.sessionConnector_, host);
  this.activity_.start();
};

/**
 * Entry-point for It2Me connections.
 *
 * @private
 */
remoting.DesktopRemoting.prototype.connectIt2Me_ = function() {
  base.dispose(this.activity_);
  this.activity_ = new remoting.It2MeActivity(this.sessionConnector_);
  this.activity_.start();
};
