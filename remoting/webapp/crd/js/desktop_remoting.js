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
   * @type {remoting.Application}
   * @private
   */
  this.app_ = app;
  app.setDelegate(this);
};

remoting.DesktopRemoting.prototype.init = function() {
  remoting.initGlobalObjects();
  remoting.initIdentity();
  remoting.initIdentityEmail(remoting.onEmailAvailable);

  remoting.initElementEventHandlers();
  remoting.initGlobalEventHandlers();

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
 * @return {Array.<string>} A list of |ClientSession.Capability|s required
 *     by this application.
 */
remoting.DesktopRemoting.prototype.getRequiredCapabilities = function() {
  return [
    remoting.ClientSession.Capability.SEND_INITIAL_RESOLUTION,
    remoting.ClientSession.Capability.RATE_LIMIT_RESIZE_REQUESTS,
    remoting.ClientSession.Capability.VIDEO_RECORDER,
    // TODO(aiguha): Add this capability based on a gyp/command-line flag,
    // rather than by default.
    remoting.ClientSession.Capability.CAST,
    remoting.ClientSession.Capability.GNUBBY_AUTH
  ];
};

/**
 * @param {remoting.ClientSession} clientSession
 */
remoting.DesktopRemoting.prototype.onConnected = function(clientSession) {
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

remoting.DesktopRemoting.prototype.onDisconnected = function() {
};

remoting.DesktopRemoting.prototype.onVideoStreamingStarted = function() {
};

/**
 * @param {string} type The type of the extension message.
 * @param {string} data The payload of the extension message.
 * @return {boolean} Return true if the extension message was recognized.
 */
remoting.DesktopRemoting.prototype.onExtensionMessage = function(type, data) {
  return false;
};

/**
 * Show a client-side error message.
 *
 * @param {remoting.Error} errorTag The error to be localized and displayed.
 * @return {void} Nothing.
 */
remoting.DesktopRemoting.prototype.onError = function(errorTag) {
  console.error('Connection failed: ' + errorTag);
  remoting.accessCode = '';

  var errorDiv = document.getElementById('connect-error-message');
  l10n.localizeElementFromTag(errorDiv, /** @type {string} */ (errorTag));

  var mode = remoting.clientSession ? remoting.clientSession.getMode()
      : this.app_.getSessionConnector().getConnectionMode();
  if (mode == remoting.ClientSession.Mode.IT2ME) {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME);
    remoting.hangoutSessionEvents.raiseEvent(
        remoting.hangoutSessionEvents.sessionStateChanged,
        remoting.ClientSession.State.FAILED
    );
  } else {
    remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_ME2ME);
  }
};
