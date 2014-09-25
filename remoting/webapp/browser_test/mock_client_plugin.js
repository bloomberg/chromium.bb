// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Mock implementation of ClientPlugin for testing.
 * @suppress {checkTypes}
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @implements {remoting.ClientPlugin}
 */
remoting.MockClientPlugin = function(container) {
  this.container_ = container;
  this.element_ = document.createElement('div');
  this.element_.style.backgroundImage = 'linear-gradient(45deg, blue, red)';
  this.width_ = 640;
  this.height_ = 480;
  this.connectionStatusUpdateHandler_ = null;
  this.desktopSizeUpdateHandler_ = null;
  this.container_.appendChild(this.element_);
};

remoting.MockClientPlugin.prototype.dispose = function() {
  this.container_.removeChild(this.element_);
  this.element_ = null;
  this.connectionStatusUpdateHandler_ = null;
};

remoting.MockClientPlugin.prototype.getDesktopWidth = function() {
  return this.width_;
};

remoting.MockClientPlugin.prototype.getDesktopHeight = function() {
  return this.height_;
};

remoting.MockClientPlugin.prototype.getDesktopXDpi = function() {
  return 96;
};

remoting.MockClientPlugin.prototype.getDesktopYDpi = function() {
  return 96;
};

remoting.MockClientPlugin.prototype.element = function() {
  return this.element_;
};

remoting.MockClientPlugin.prototype.initialize = function(onDone) {
  window.setTimeout(onDone.bind(null, true), 0);
};

remoting.MockClientPlugin.prototype.connect = function(
    hostJid, hostPublicKey, localJid, sharedSecret,
    authenticationMethods, authenticationTag,
    clientPairingId, clientPairedSecret) {
  base.debug.assert(this.connectionStatusUpdateHandler_ != null);
  window.setTimeout(
      this.connectionStatusUpdateHandler_.bind(
          this,
          remoting.ClientSession.State.CONNECTED,
          remoting.ClientSession.ConnectionError.NONE),
      0);
};

remoting.MockClientPlugin.prototype.injectKeyEvent =
    function(key, down) {};

remoting.MockClientPlugin.prototype.remapKey = function(from, to) {};

remoting.MockClientPlugin.prototype.releaseAllKeys = function() {};

remoting.MockClientPlugin.prototype.notifyClientResolution =
    function(width, height, dpi) {
  this.width_ = width;
  this.height_ = height;
  if (this.desktopSizeUpdateHandler_) {
    window.setTimeout(this.desktopSizeUpdateHandler_, 0);
  }
};

remoting.MockClientPlugin.prototype.onIncomingIq = function(iq) {};

remoting.MockClientPlugin.prototype.isSupportedVersion = function() {
  return true;
};

remoting.MockClientPlugin.prototype.hasFeature = function(feature) {
  return false;
};

remoting.MockClientPlugin.prototype.enableMediaSourceRendering =
    function(mediaSourceRenderer) {};

remoting.MockClientPlugin.prototype.sendClipboardItem =
    function(mimeType, item) {};

remoting.MockClientPlugin.prototype.useAsyncPinDialog = function() {};

remoting.MockClientPlugin.prototype.requestPairing =
    function(clientName, onDone) {};

remoting.MockClientPlugin.prototype.onPinFetched = function(pin) {};

remoting.MockClientPlugin.prototype.onThirdPartyTokenFetched =
    function(token, sharedSecret) {};

remoting.MockClientPlugin.prototype.pauseAudio = function(pause) {};

remoting.MockClientPlugin.prototype.pauseVideo = function(pause) {};

remoting.MockClientPlugin.prototype.getPerfStats = function() {
  var result = new remoting.ClientSession.PerfStats;
  result.videoBandwidth = 999;
  result.videoFrameRate = 60;
  result.captureLatency = 10;
  result.encodeLatency = 10;
  result.decodeLatency = 10;
  result.renderLatency = 10;
  result.roundtripLatency = 10;
  return result;
};

remoting.MockClientPlugin.prototype.sendClientMessage =
    function(name, data) {};

remoting.MockClientPlugin.prototype.setOnOutgoingIqHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setOnDebugMessageHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setConnectionStatusUpdateHandler =
    function(handler) {
  this.connectionStatusUpdateHandler_ = handler;
};

remoting.MockClientPlugin.prototype.setConnectionReadyHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setDesktopSizeUpdateHandler =
    function(handler) {
  this.desktopSizeUpdateHandler_ = handler;
};

remoting.MockClientPlugin.prototype.setCapabilitiesHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setGnubbyAuthHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setCastExtensionHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setMouseCursorHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setFetchThirdPartyTokenHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setFetchPinHandler =
    function(handler) {};


/**
 * @constructor
 * @extends {remoting.ClientPluginFactory}
 */
remoting.MockClientPluginFactory = function() {};

remoting.MockClientPluginFactory.prototype.createPlugin =
    function(container, onExtensionMessage) {
  return new remoting.MockClientPlugin(container);
};

remoting.MockClientPluginFactory.prototype.preloadPlugin = function() {};
