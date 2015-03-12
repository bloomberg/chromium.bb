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
 * @param {Element} container
 * @constructor
 * @implements {remoting.ClientPlugin}
 */
remoting.MockClientPlugin = function(container) {
  this.container_ = container;
  this.element_ = document.createElement('div');
  this.element_.style.backgroundImage = 'linear-gradient(45deg, blue, red)';
  this.connectionStatusUpdateHandler_ = null;
  this.desktopSizeUpdateHandler_ = null;
  this.container_.appendChild(this.element_);
  this.hostDesktop_ = new remoting.MockClientPlugin.HostDesktop();
};

remoting.MockClientPlugin.prototype.dispose = function() {
  this.container_.removeChild(this.element_);
  this.element_ = null;
  this.connectionStatusUpdateHandler_ = null;
};

remoting.MockClientPlugin.prototype.hostDesktop = function() {
  return this.hostDesktop_;
};

remoting.MockClientPlugin.prototype.element = function() {
  return this.element_;
};

remoting.MockClientPlugin.prototype.initialize = function(onDone) {
  window.setTimeout(onDone.bind(null, true), 0);
};


remoting.MockClientPlugin.prototype.connect =
    function(host, localJid, credentialsProvider) {
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

remoting.MockClientPlugin.prototype.onIncomingIq = function(iq) {};

remoting.MockClientPlugin.prototype.isSupportedVersion = function() {
  return true;
};

remoting.MockClientPlugin.prototype.hasFeature = function(feature) {
  return false;
};

remoting.MockClientPlugin.prototype.sendClipboardItem =
    function(mimeType, item) {};

remoting.MockClientPlugin.prototype.requestPairing =
    function(clientName, onDone) {};

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

/**
 * @param {function(number, number):void} handler
 * @private
 */
remoting.MockClientPlugin.prototype.setConnectionStatusUpdateHandler =
    function(handler) {
  /** @type {function(number, number):void} */
  this.connectionStatusUpdateHandler_ = handler;
};

remoting.MockClientPlugin.prototype.setRouteChangedHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setConnectionReadyHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setCapabilitiesHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setGnubbyAuthHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setCastExtensionHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setMouseCursorHandler =
    function(handler) {};

/**
 * @constructor
 * @implements {remoting.HostDesktop}
 * @extends {base.EventSourceImpl}
 */
remoting.MockClientPlugin.HostDesktop = function() {
  /** @private */
  this.width_ = 0;
  /** @private */
  this.height_ = 0;
  /** @private */
  this.xDpi_ = 96;
  /** @private */
  this.yDpi_ = 96;
  /** @private */
  this.resizable_ = true;
  this.defineEvents(base.values(remoting.HostDesktop.Events));
};
base.extend(remoting.MockClientPlugin.HostDesktop, base.EventSourceImpl);

/**
 * @return {{width:number, height:number, xDpi:number, yDpi:number}}
 * @override
 */
remoting.MockClientPlugin.HostDesktop.prototype.getDimensions = function() {
  return {
    width: this.width_,
    height: this.height_,
    xDpi: this.xDpi_,
    yDpi: this.yDpi_
  };
};

/**
 * @return {boolean}
 * @override
 */
remoting.MockClientPlugin.HostDesktop.prototype.isResizable = function() {
  return this.resizable_;
};

/**
 * @param {number} width
 * @param {number} height
 * @param {number} deviceScale
 * @override
 */
remoting.MockClientPlugin.HostDesktop.prototype.resize =
    function(width, height, deviceScale) {
  this.width_ = width;
  this.height_ = height;
  this.xDpi_ = this.yDpi_ = Math.floor(deviceScale * 96);
  this.raiseEvent(remoting.HostDesktop.Events.sizeChanged,
                  this.getDimensions());
};

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
