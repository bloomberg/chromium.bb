// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Implements a basic UX control for a connected app remoting session.
 */

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

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
 * @param {HTMLElement} containerElement
 * @param {remoting.WindowShape} windowShape
 * @param {remoting.ConnectionInfo} connectionInfo
 * @param {base.WindowMessageDispatcher} windowMessageDispatcher
 *
 * @constructor
 * @implements {base.Disposable}
 * @implements {remoting.ProtocolExtension}
 */
remoting.AppConnectedView = function(containerElement, windowShape,
                                     connectionInfo, windowMessageDispatcher) {
  /** @private */
  this.plugin_ = connectionInfo.plugin();

  /** @private */
  this.host_ = connectionInfo.host();

  var menuAdapter = new remoting.ContextMenuChrome();

  // Initialize the context menus.
  if (!remoting.platformIsChromeOS()) {
    menuAdapter = new remoting.ContextMenuDom(
        document.getElementById('context-menu'), windowShape);
  }

  this.contextMenu_ = new remoting.ApplicationContextMenu(
      menuAdapter, this.plugin_, connectionInfo.session(), windowShape);
  this.contextMenu_.setHostId(connectionInfo.host().hostId);

  /** @private */
  this.keyboardLayoutsMenu_ = new remoting.KeyboardLayoutsMenu(menuAdapter);

  /** @private */
  this.windowActivationMenu_ = new remoting.WindowActivationMenu(menuAdapter);

  var baseView = new remoting.ConnectedView(
      this.plugin_, containerElement,
      containerElement.querySelector('.mouse-cursor-overlay'));

  var windowShapeHook = new base.EventHook(
      this.plugin_.hostDesktop(),
      remoting.HostDesktop.Events.shapeChanged,
      windowShape.setDesktopRects.bind(windowShape));

  var desktopSizeHook = new base.EventHook(
      this.plugin_.hostDesktop(),
      remoting.HostDesktop.Events.sizeChanged,
      this.onDesktopSizeChanged_.bind(this));

  /** @private */
  this.disposables_ =
      new base.Disposables(baseView, windowShapeHook, desktopSizeHook,
                           this.contextMenu_, menuAdapter);

  /** @private */
  this.supportsGoogleDrive_ = this.plugin_.hasCapability(
      remoting.ClientSession.Capability.GOOGLE_DRIVE);

  this.resizeHostToClientArea_();
  this.plugin_.extensions().register(this);

  /** @private {remoting.CloudPrintDialogContainer} */
  this.cloudPrintDialogContainer_ = new remoting.CloudPrintDialogContainer(
      /** @type {!Webview} */ (document.getElementById('cloud-print-webview')),
      windowShape, windowMessageDispatcher, baseView);

  this.disposables_.add(this.cloudPrintDialogContainer_);
};

/**
 * @return {void} Nothing.
 */
remoting.AppConnectedView.prototype.dispose = function() {
  this.windowActivationMenu_.setExtensionMessageSender(base.doNothing);
  this.keyboardLayoutsMenu_.setExtensionMessageSender(base.doNothing);
  base.dispose(this.disposables_);
};

/**
 * Resize the host to the dimensions of the current window.
 * @private
 */
remoting.AppConnectedView.prototype.resizeHostToClientArea_ = function() {
  var hostDesktop = this.plugin_.hostDesktop();
  var desktopScale = this.host_.options.getDesktopScale();
  hostDesktop.resize(window.innerWidth * desktopScale,
                     window.innerHeight * desktopScale,
                     window.devicePixelRatio);
};

/**
 * Adjust the size of the plugin according to the dimensions of the hostDesktop.
 *
 * @param {{width:number, height:number, xDpi:number, yDpi:number}} hostDesktop
 * @private
 */
remoting.AppConnectedView.prototype.onDesktopSizeChanged_ =
    function(hostDesktop) {
  // The first desktop size change indicates that we can close the loading
  // window.
  remoting.LoadingWindow.close();

  var hostSize = { width: hostDesktop.width, height: hostDesktop.height };
  var hostDpi = { x: hostDesktop.xDpi, y: hostDesktop.yDpi };
  var clientArea = { width: window.innerWidth, height: window.innerHeight };
  var newSize = remoting.Viewport.choosePluginSize(
      clientArea, window.devicePixelRatio,
      hostSize, hostDpi, this.host_.options.getDesktopScale(),
      true /* fullscreen */ , true /* shrinkToFit */ );

  this.plugin_.element().style.width = newSize.width + 'px';
  this.plugin_.element().style.height = newSize.height + 'px';
};

/**
 * @return {Array<string>}
 * @override {remoting.ProtocolExtension}
 */
remoting.AppConnectedView.prototype.getExtensionTypes = function() {
  return ['openURL', 'onWindowRemoved', 'onWindowAdded',
          'onAllWindowsMinimized', 'setKeyboardLayouts', 'pingResponse'];
};

/**
 * @param {function(string,string)} sendMessageToHost Callback to send a message
 *     to the host.
 * @override {remoting.ProtocolExtension}
 */
remoting.AppConnectedView.prototype.startExtension = function(
    sendMessageToHost) {
  this.windowActivationMenu_.setExtensionMessageSender(sendMessageToHost);
  this.keyboardLayoutsMenu_.setExtensionMessageSender(sendMessageToHost);

  remoting.identity.getUserInfo().then(function(userInfo) {
    sendMessageToHost('setUserDisplayInfo',
                      JSON.stringify({fullName: userInfo.name}));
  });

  var onRestoreHook = new base.ChromeEventHook(
      chrome.app.window.current().onRestored, function() {
        sendMessageToHost('restoreAllWindows', '');
      });

  var pingTimer = new base.RepeatingTimer(function() {
    var message = {timestamp: new Date().getTime()};
    sendMessageToHost('pingRequest', JSON.stringify(message));
  }, CONNECTION_SPEED_PING_INTERVAL_MS);

  this.disposables_.add(onRestoreHook, pingTimer);

  if (this.supportsGoogleDrive_) {
    this.disposables_.add(new base.RepeatingTimer(
        this.sendGoogleDriveAccessToken_.bind(this, sendMessageToHost),
        DRIVE_ACCESS_TOKEN_REFRESH_INTERVAL_MS, true));
  }
};

/**
 * @param {string} command The message command.
 * @param {Object} message The parsed extension message data.
 * @override {remoting.ProtocolExtension}
 */
remoting.AppConnectedView.prototype.onExtensionMessage =
    function(command, message) {
  switch (command) {
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

    case 'printDocument':
      var title = base.getStringAttr(message, 'title');
      var type = base.getStringAttr(message, 'type');
      var data = base.getStringAttr(message, 'data');
      if (type == '' || data == '') {
        console.error('"type" and "data" cannot be empty.');
        return true;
      }

      this.cloudPrintDialogContainer_.printDocument(title, type, data);
      return true;
  }

  return false;
};

/**
 * Timer callback to send the access token to the host.
 * @param {function(string, string)} sendExtensionMessage
 * @private
 */
remoting.AppConnectedView.prototype.sendGoogleDriveAccessToken_ =
    function(sendExtensionMessage) {
  var googleDriveScopes = [
    'https://docs.google.com/feeds/',
    'https://www.googleapis.com/auth/drive'
  ];
  remoting.identity.getNewToken(googleDriveScopes).then(
    function(/** string */ token){
      console.assert(token !== previousToken_,
                     'getNewToken() returned the same token.');
      previousToken_ = token;
      sendExtensionMessage('accessToken', token);
  }).catch(remoting.Error.handler(function(/** remoting.Error */ error) {
    console.log('Failed to refresh access token: ' + error.toString());
  }));
};

// The access token last received from getNewToken. Saved to ensure that we
// get a fresh token each time.
var previousToken_ = '';

})();
