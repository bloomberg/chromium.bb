// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This class implements the functionality that is specific to application
 * remoting ("AppRemoting" or AR).
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Interval to test the connection speed.
 * @const {number}
 */
var CONNECTION_SPEED_PING_INTERVAL_MS = 10 * 1000;

/**
 * Interval to refresh the google drive access token.
 * @const {number}
 */
var DRIVE_ACCESS_TOKEN_REFRESH_INTERVAL_MS = 15 * 60 * 1000;

/**
 * @param {Array<string>} appCapabilities Array of application capabilities.
 * @constructor
 * @implements {remoting.ApplicationInterface}
 * @implements {remoting.ProtocolExtension}
 * @implements {remoting.ClientSession.EventHandler}
 * @extends {remoting.Application}
 */
remoting.AppRemoting = function(appCapabilities) {
  base.inherits(this, remoting.Application);

  /** @private {remoting.ApplicationContextMenu} */
  this.contextMenu_ = null;

  /** @private {remoting.KeyboardLayoutsMenu} */
  this.keyboardLayoutsMenu_ = null;

  /** @private {remoting.WindowActivationMenu} */
  this.windowActivationMenu_ = null;

  /** @private {remoting.AppConnectedView} */
  this.connectedView_ = null;

  /** @private {base.Disposables} */
  this.extensionDisposables_ = null;

  /** @private */
  this.supportsGoogleDrive_ = false;

  /** @private */
  this.sessionConnector_ = remoting.SessionConnector.factory.createConnector(
      document.getElementById('client-container'),
      appCapabilities, this);
};

/**
 * Type definition for the RunApplicationResponse returned by the API.
 *
 * @constructor
 * @private
 */
remoting.AppRemoting.AppHostResponse = function() {
  /** @type {string} */
  this.status = '';
  /** @type {string} */
  this.hostJid = '';
  /** @type {string} */
  this.authorizationCode = '';
  /** @type {string} */
  this.sharedSecret = '';

  this.host = {
    /** @type {string} */
    applicationId: '',

    /** @type {string} */
    hostId: ''};
};

/**
 * @return {string} Application product name to be used in UI.
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.getApplicationName = function() {
  var manifest = chrome.runtime.getManifest();
  return manifest.name;
};

/**
 * @param {!remoting.Error} error The failure reason.
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.signInFailed_ = function(error) {
  this.onError(error);
};

/**
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.initApplication_ = function() {
  // TODO(jamiewalch): Remove ClientSession's dependency on remoting.fullscreen
  // so that this is no longer required.
  remoting.fullscreen = new remoting.FullscreenAppsV2();

  remoting.windowShape.updateClientWindowShape();

  // Initialize the context menus.
  if (remoting.platformIsChromeOS()) {
    var adapter = new remoting.ContextMenuChrome();
  } else {
    var adapter = new remoting.ContextMenuDom(
        document.getElementById('context-menu'));
  }
  this.contextMenu_ = new remoting.ApplicationContextMenu(adapter);
  this.keyboardLayoutsMenu_ = new remoting.KeyboardLayoutsMenu(adapter);
  this.windowActivationMenu_ = new remoting.WindowActivationMenu(adapter);
};

/**
 * @param {string} token An OAuth access token.
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.startApplication_ = function(token) {
  remoting.LoadingWindow.show();

  /** @type {remoting.AppRemoting} */
  var that = this;

  /** @param {!remoting.Xhr.Response} xhrResponse */
  var parseAppHostResponse = function(xhrResponse) {
    if (xhrResponse.status == 200) {
      var response = /** @type {remoting.AppRemoting.AppHostResponse} */
          (base.jsonParseSafe(xhrResponse.getText()));
      if (response &&
          response.status &&
          response.status == 'done' &&
          response.hostJid &&
          response.authorizationCode &&
          response.sharedSecret &&
          response.host &&
          response.host.hostId) {
        var hostJid = response.hostJid;
        that.contextMenu_.setHostId(response.host.hostId);
        var host = new remoting.Host(response.host.hostId);
        host.jabberId = hostJid;
        host.authorizationCode = response.authorizationCode;
        host.sharedSecret = response.sharedSecret;

        remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);

        var idleDetector = new remoting.IdleDetector(
            document.getElementById('idle-dialog'),
            remoting.app.disconnect.bind(remoting.app));

        /**
         * @param {string} tokenUrl Token-issue URL received from the host.
         * @param {string} hostPublicKey Host public key (DER and Base64
         *     encoded).
         * @param {string} scope OAuth scope to request the token for.
         * @param {function(string, string):void} onThirdPartyTokenFetched
         *     Callback.
         */
        var fetchThirdPartyToken = function(
            tokenUrl, hostPublicKey, scope, onThirdPartyTokenFetched) {
          // Use the authentication tokens returned by the app-remoting server.
          onThirdPartyTokenFetched(host['authorizationCode'],
                                   host['sharedSecret']);
        };

        that.sessionConnector_.connect(
            remoting.Application.Mode.APP_REMOTING, host,
            new remoting.CredentialsProvider(
                {fetchThirdPartyToken: fetchThirdPartyToken}));
      } else if (response && response.status == 'pending') {
        that.onError(new remoting.Error(
            remoting.Error.Tag.SERVICE_UNAVAILABLE));
      }
    } else {
      console.error('Invalid "runApplication" response from server.');
      that.onError(remoting.Error.fromHttpStatus(xhrResponse.status));
    }
  };

  new remoting.Xhr({
    method: 'POST',
    url: that.runApplicationUrl_(),
    oauthToken: token
  }).start().then(parseAppHostResponse);
};

/**
 * @override {remoting.ApplicationInterface}
 */
remoting.AppRemoting.prototype.exitApplication_ = function() {
  remoting.LoadingWindow.close();
  this.closeMainWindow_();
};

/**
 * @param {remoting.ConnectionInfo} connectionInfo
 */
remoting.AppRemoting.prototype.onConnected = function(connectionInfo) {
  this.supportsGoogleDrive_ = connectionInfo.session().hasCapability(
      remoting.ClientSession.Capability.GOOGLE_DRIVE);

  connectionInfo.plugin().extensions().register(this);

  this.connectedView_ = new remoting.AppConnectedView(
      document.getElementById('client-container'), connectionInfo);

  // Map Cmd to Ctrl on Mac since hosts typically use Ctrl for keyboard
  // shortcuts, but we want them to act as natively as possible.
  if (remoting.platformIsMac()) {
    connectionInfo.plugin().setRemapKeys('0x0700e3>0x0700e0,0x0700e7>0x0700e4');
  }
};

remoting.AppRemoting.prototype.onDisconnected = function() {
  base.dispose(this.connectedView_);
  this.connectedView_ = null;
  this.stopExtension_();
  chrome.app.window.current().close();
};

/**
 * @param {!remoting.Error} error
 */
remoting.AppRemoting.prototype.onConnectionFailed = function(error) {
  this.onError(error);
};

/**
 * @param {!remoting.Error} error The error to be localized and displayed.
 */
remoting.AppRemoting.prototype.onError = function(error) {
  console.error('Connection failed: ' + error.toString());
  remoting.LoadingWindow.close();
  remoting.MessageWindow.showErrorMessage(
      chrome.i18n.getMessage(/*i18n-content*/'CONNECTION_FAILED'),
      chrome.i18n.getMessage(error.getTag()));
  this.stopExtension_();
};


/**
 * @return {Array<string>}
 * @override {remoting.ProtocolExtension}
 */
remoting.AppRemoting.prototype.getExtensionTypes = function() {
  return ['openURL', 'onWindowRemoved', 'onWindowAdded',
          'onAllWindowsMinimized', 'setKeyboardLayouts', 'pingResponse'];
};

/**
 * @param {function(string,string)} sendMessageToHost Callback to send a message
 *     to the host.
 * @override {remoting.ProtocolExtension}
 */
remoting.AppRemoting.prototype.startExtension = function(sendMessageToHost) {
  this.windowActivationMenu_.setExtensionMessageSender(sendMessageToHost);
  this.keyboardLayoutsMenu_.setExtensionMessageSender(sendMessageToHost);

  remoting.identity.getUserInfo().then(function(userInfo) {
    sendMessageToHost('setUserDisplayInfo',
                      JSON.stringify({fullName: userInfo.name}));
  });

  base.dispose(this.extensionDisposables_);

  var onRestoreHook = new base.ChromeEventHook(
      chrome.app.window.current().onRestored, function() {
        sendMessageToHost('restoreAllWindows', '');
      });

  var pingTimer = new base.RepeatingTimer(function() {
    var message = {timestamp: new Date().getTime()};
    sendMessageToHost('pingRequest', JSON.stringify(message));
  }, CONNECTION_SPEED_PING_INTERVAL_MS);

  this.extensionDisposables_ = new base.Disposables(onRestoreHook, pingTimer);

  if (this.supportsGoogleDrive_) {
    this.extensionDisposables_.add(new base.RepeatingTimer(
        this.sendGoogleDriveAccessToken_.bind(this, sendMessageToHost),
        DRIVE_ACCESS_TOKEN_REFRESH_INTERVAL_MS, true));
  }
};

/** @private */
remoting.AppRemoting.prototype.stopExtension_ = function() {
  this.windowActivationMenu_.setExtensionMessageSender(base.doNothing);
  this.keyboardLayoutsMenu_.setExtensionMessageSender(base.doNothing);

  base.dispose(this.extensionDisposables_);
  this.extensionDisposables_ = null;
};

/**
 * @param {string} type The message type.
 * @param {Object} message The parsed extension message data.
 * @override {remoting.ProtocolExtension}
 */
remoting.AppRemoting.prototype.onExtensionMessage = function(type, message) {
  switch (type) {

    case 'openURL':
      // URL requests from the hosted app are untrusted, so disallow anything
      // other than HTTP or HTTPS.
      var url = base.getStringAttr(message, 'url');
      if (url.indexOf('http:') != 0 && url.indexOf('https:') != 0) {
        console.error('Bad URL: ' + url);
      } else {
        window.open(url);
      }
      return true;

    case 'onWindowRemoved':
      var id = base.getNumberAttr(message, 'id');
      this.windowActivationMenu_.remove(id);
      return true;

    case 'onWindowAdded':
      var id = base.getNumberAttr(message, 'id');
      var title = base.getStringAttr(message, 'title');
      this.windowActivationMenu_.add(id, title);
      return true;

    case 'onAllWindowsMinimized':
      chrome.app.window.current().minimize();
      return true;

    case 'setKeyboardLayouts':
      var supportedLayouts = base.getArrayAttr(message, 'supportedLayouts');
      var currentLayout = base.getStringAttr(message, 'currentLayout');
      console.log('Current host keyboard layout: ' + currentLayout);
      console.log('Supported host keyboard layouts: ' + supportedLayouts);
      this.keyboardLayoutsMenu_.setLayouts(supportedLayouts, currentLayout);
      return true;

    case 'pingResponse':
      var then = base.getNumberAttr(message, 'timestamp');
      var now = new Date().getTime();
      this.contextMenu_.updateConnectionRTT(now - then);
      return true;
  }

  return false;
};

/**
 * Timer callback to send the access token to the host.
 * @param {function(string, string)} sendExtensionMessage
 * @private
 */
remoting.AppRemoting.prototype.sendGoogleDriveAccessToken_
    = function(sendExtensionMessage) {
  var googleDriveScopes = [
    'https://docs.google.com/feeds/',
    'https://www.googleapis.com/auth/drive'
  ];
  remoting.identity.getNewToken(googleDriveScopes).then(
    function(/** string */ token){
      sendExtensionMessage('accessToken', token);
  }).catch(remoting.Error.handler(function(/** remoting.Error */ error) {
    console.log('Failed to refresh access token: ' + error.toString());
  }));
};

/**
 * @return {string}
 * @private
 */
remoting.AppRemoting.prototype.runApplicationUrl_ = function() {
  return remoting.settings.APP_REMOTING_API_BASE_URL + '/applications/' +
      remoting.settings.getAppRemotingApplicationId() + '/run';
};
