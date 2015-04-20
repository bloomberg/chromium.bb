// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Mock implementation of ClientPlugin for testing.
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
  this.element_ = /** @type {HTMLElement} */ (document.createElement('div'));
  this.element_.style.backgroundImage = 'linear-gradient(45deg, blue, red)';
  this.container_.appendChild(this.element_);
  this.hostDesktop_ = new remoting.MockClientPlugin.HostDesktop();
  this.extensions_ = new remoting.ProtocolExtensionManager(base.doNothing);
  /** @type {remoting.ClientPlugin.ConnectionEventHandler} */
  this.connectionEventHandler = null;

  // Fake initialization result to return.
  this.mock$initializationResult = true;

  // Fake capabilities to return.
  this.mock$capabilities = [
      remoting.ClientSession.Capability.SEND_INITIAL_RESOLUTION,
      remoting.ClientSession.Capability.RATE_LIMIT_RESIZE_REQUESTS
  ];
};

remoting.MockClientPlugin.prototype.dispose = function() {
  this.container_.removeChild(this.element_);
  this.element_ = null;
  this.connectionStatusUpdateHandler_ = null;
};

remoting.MockClientPlugin.prototype.extensions = function() {
  return this.extensions_;
};

remoting.MockClientPlugin.prototype.hostDesktop = function() {
  return this.hostDesktop_;
};

remoting.MockClientPlugin.prototype.element = function() {
  return this.element_;
};

remoting.MockClientPlugin.prototype.initialize = function(onDone) {
  var that = this;
  Promise.resolve().then(function() {
    onDone(that.mock$initializationResult);
  });
};


remoting.MockClientPlugin.prototype.connect =
    function(host, localJid, credentialsProvider) {
  base.debug.assert(this.connectionEventHandler !== null);
  var that = this;
  window.requestAnimationFrame(function() {
    that.connectionEventHandler.onConnectionStatusUpdate(
          remoting.ClientSession.State.CONNECTED,
          remoting.ClientSession.ConnectionError.NONE);
  });
};

remoting.MockClientPlugin.prototype.injectKeyCombination = function(keys) {};

remoting.MockClientPlugin.prototype.injectKeyEvent =
    function(key, down) {};

remoting.MockClientPlugin.prototype.setRemapKeys = function(remappings) {};

remoting.MockClientPlugin.prototype.remapKey = function(from, to) {};

remoting.MockClientPlugin.prototype.releaseAllKeys = function() {};

remoting.MockClientPlugin.prototype.onIncomingIq = function(iq) {};

remoting.MockClientPlugin.prototype.isSupportedVersion = function() {
  return true;
};

remoting.MockClientPlugin.prototype.hasFeature = function(feature) {
  return false;
};

remoting.MockClientPlugin.prototype.hasCapability = function(capability) {
  return this.mock$capabilities.indexOf(capability) !== -1;
};

remoting.MockClientPlugin.prototype.sendClipboardItem =
    function(mimeType, item) {};

remoting.MockClientPlugin.prototype.requestPairing =
    function(clientName, onDone) {};

remoting.MockClientPlugin.prototype.allowMouseLock = function() {};

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

remoting.MockClientPlugin.prototype.setConnectionEventHandler =
    function(handler) {
  this.connetionEventHandler = handler;
};

remoting.MockClientPlugin.prototype.setMouseCursorHandler =
    function(handler) {};

remoting.MockClientPlugin.prototype.setClipboardHandler = function(handler) {};

remoting.MockClientPlugin.prototype.setDebugDirtyRegionHandler =
    function(handler) {};

/**
 * @constructor
 * @implements {remoting.HostDesktop}
 * @extends {base.EventSourceImpl}
 */
remoting.MockClientPlugin.HostDesktop = function() {
  base.inherits(this, base.EventSourceImpl);
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
 * @implements {remoting.ClientPluginFactory}
 */
remoting.MockClientPluginFactory = function() {
  /** @private {remoting.MockClientPlugin} */
  this.plugin_ = null;
};

remoting.MockClientPluginFactory.prototype.createPlugin =
    function(container, onExtensionMessage) {
  this.plugin_ = new remoting.MockClientPlugin(container);
  return this.plugin_;
};

remoting.MockClientPluginFactory.prototype.preloadPlugin = function() {};
