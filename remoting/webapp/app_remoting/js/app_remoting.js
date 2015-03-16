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
 * @param {remoting.Application} app The main app that owns this delegate.
 * @constructor
 * @implements {remoting.Application.Delegate}
 */
remoting.AppRemoting = function(app) {
  app.setDelegate(this);

  /** @private {remoting.ApplicationContextMenu} */
  this.contextMenu_ = null;

  /** @private {remoting.KeyboardLayoutsMenu} */
  this.keyboardLayoutsMenu_ = null;

  /** @private {remoting.WindowActivationMenu} */
  this.windowActivationMenu_ = null;

   /** @private {number} */
   this.pingTimerId_ = 0;
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
 * Initialize the application. This is called before an OAuth token is requested
 * and should be used for tasks such as initializing the DOM, registering event
 * handlers, etc.
 */
remoting.AppRemoting.prototype.init = function() {
  // TODO(jamiewalch): Remove ClientSession's dependency on remoting.fullscreen
  // so that this is no longer required.
  remoting.fullscreen = new remoting.FullscreenAppsV2();

  var restoreHostWindows = function() {
    if (remoting.clientSession) {
      remoting.clientSession.sendClientMessage('restoreAllWindows', '');
    }
  };
  chrome.app.window.current().onRestored.addListener(restoreHostWindows);

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

  remoting.LoadingWindow.show();
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
remoting.AppRemoting.prototype.start = function(connector, token) {
  /** @type {remoting.AppRemoting} */
  var that = this;

  /** @param {XMLHttpRequest} xhr */
  var parseAppHostResponse = function(xhr) {
    if (xhr.status == 200) {
      var response = /** @type {remoting.AppRemoting.AppHostResponse} */
          (base.jsonParseSafe(xhr.responseText));
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
        var host = new remoting.Host;
        host.hostId = response.host.hostId;
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

        connector.connectMe2App(host, fetchThirdPartyToken);
      } else if (response && response.status == 'pending') {
        that.handleError(new remoting.Error(
            remoting.Error.Tag.SERVICE_UNAVAILABLE));
      }
    } else {
      console.error('Invalid "runApplication" response from server.');
      // TODO(garykac) Start using remoting.Error.fromHttpStatus once it has
      // been updated to properly report 'unknown' errors (rather than
      // reporting them as AUTHENTICATION_FAILED).
      if (xhr.status == 0) {
        that.handleError(new remoting.Error(
            remoting.Error.Tag.NETWORK_FAILURE));
      } else if (xhr.status == 401) {
        that.handleError(new remoting.Error(
            remoting.Error.Tag.AUTHENTICATION_FAILED));
      } else if (xhr.status == 403) {
        that.handleError(new remoting.Error(
            remoting.Error.Tag.APP_NOT_AUTHORIZED));
      } else if (xhr.status == 502 || xhr.status == 503) {
        that.handleError(new remoting.Error(
            remoting.Error.Tag.SERVICE_UNAVAILABLE));
      } else {
        that.handleError(remoting.Error.unexpected());
      }
    }
  };

  remoting.xhr.start({
    method: 'POST',
    url: that.runApplicationUrl(),
    onDone: parseAppHostResponse,
    oauthToken: token
  });
};

/**
 * Report an authentication error to the user. This is called in lieu of start()
 * if the user cannot be authenticated or if they decline the app permissions.
 *
 * @param {!remoting.Error} error The failure reason.
 */
remoting.AppRemoting.prototype.signInFailed = function(error) {
  this.handleError(error);
};

/**
 * @return {string} Application product name to be used in UI.
 */
remoting.AppRemoting.prototype.getApplicationName = function() {
  var manifest = chrome.runtime.getManifest();
  return manifest.name;
};

/** @return {string} */
remoting.AppRemoting.prototype.runApplicationUrl = function() {
  return remoting.settings.APP_REMOTING_API_BASE_URL + '/applications/' +
      remoting.settings.getAppRemotingApplicationId() + '/run';
};

/**
 * @return {string} The default remap keys for the current platform.
 */
remoting.AppRemoting.prototype.getDefaultRemapKeys = function() {
  // Map Cmd to Ctrl on Mac since hosts typically use Ctrl for keyboard
  // shortcuts, but we want them to act as natively as possible.
  if (remoting.platformIsMac()) {
    return '0x0700e3>0x0700e0,0x0700e7>0x0700e4';
  }
  return '';
};

/**
 * Called when a new session has been connected.
 *
 * @param {remoting.ClientSession} clientSession
 * @return {void} Nothing.
 */
remoting.AppRemoting.prototype.handleConnected = function(clientSession) {
  remoting.identity.getUserInfo().then(
      function(userInfo) {
        remoting.clientSession.sendClientMessage(
            'setUserDisplayInfo',
            JSON.stringify({fullName: userInfo.name}));
      });

  // Set up a ping at 10-second intervals to test the connection speed.
  function ping() {
    var message = { timestamp: new Date().getTime() };
    clientSession.sendClientMessage('pingRequest', JSON.stringify(message));
  };
  ping();
  this.pingTimerId_ = window.setInterval(ping, 10 * 1000);
};

/**
 * Called when the current session has been disconnected.
 *
 * @return {void} Nothing.
 */
remoting.AppRemoting.prototype.handleDisconnected = function() {
  // Cancel the ping when the connection closes.
  window.clearInterval(this.pingTimerId_);

  chrome.app.window.current().close();
};

/**
 * Called when the current session's connection has failed.
 *
 * @param {remoting.SessionConnector} connector
 * @param {!remoting.Error} error
 * @return {void} Nothing.
 */
remoting.AppRemoting.prototype.handleConnectionFailed = function(
    connector, error) {
  this.handleError(error);
};

/**
 * Called when the current session has reached the point where the host has
 * started streaming video frames to the client.
 *
 * @return {void} Nothing.
 */
remoting.AppRemoting.prototype.handleVideoStreamingStarted = function() {
  remoting.LoadingWindow.close();
};

/**
 * Called when an extension message needs to be handled.
 *
 * @param {string} type The type of the extension message.
 * @param {Object} message The parsed extension message data.
 * @return {boolean} True if the extension message was recognized.
 */
remoting.AppRemoting.prototype.handleExtensionMessage = function(
    type, message) {
  switch (type) {

    case 'openURL':
      // URL requests from the hosted app are untrusted, so disallow anything
      // other than HTTP or HTTPS.
      var url = getStringAttr(message, 'url');
      if (url.indexOf('http:') != 0 && url.indexOf('https:') != 0) {
        console.error('Bad URL: ' + url);
      } else {
        window.open(url);
      }
      return true;

    case 'onWindowRemoved':
      var id = getNumberAttr(message, 'id');
      this.windowActivationMenu_.remove(id);
      return true;

    case 'onWindowAdded':
      var id = getNumberAttr(message, 'id');
      var title = getStringAttr(message, 'title');
      this.windowActivationMenu_.add(id, title);
      return true;

    case 'onAllWindowsMinimized':
      chrome.app.window.current().minimize();
      return true;

    case 'setKeyboardLayouts':
      var supportedLayouts = getArrayAttr(message, 'supportedLayouts');
      var currentLayout = getStringAttr(message, 'currentLayout');
      console.log('Current host keyboard layout: ' + currentLayout);
      console.log('Supported host keyboard layouts: ' + supportedLayouts);
      this.keyboardLayoutsMenu_.setLayouts(supportedLayouts, currentLayout);
      return true;

    case 'pingResponse':
      var then = getNumberAttr(message, 'timestamp');
      var now = new Date().getTime();
      this.contextMenu_.updateConnectionRTT(now - then);
      return true;
  }

  return false;
};

/**
 * Called when an error needs to be displayed to the user.
 *
 * @param {!remoting.Error} error The error to be localized and displayed.
 * @return {void} Nothing.
 */
remoting.AppRemoting.prototype.handleError = function(error) {
  console.error('Connection failed: ' + error.toString());
  remoting.LoadingWindow.close();
  remoting.MessageWindow.showErrorMessage(
      chrome.i18n.getMessage(/*i18n-content*/'CONNECTION_FAILED'),
      chrome.i18n.getMessage(error.getTag()));
};

/**
 * Close the loading window before exiting.
 */
remoting.AppRemoting.prototype.handleExit = function() {
  remoting.LoadingWindow.close();
};
