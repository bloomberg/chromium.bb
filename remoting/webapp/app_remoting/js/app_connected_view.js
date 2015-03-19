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

// A 10-second interval to test the connection speed.
var CONNECTION_SPEED_PING_INTERVAL = 10 * 1000;

/**
 * @param {HTMLElement} containerElement
 * @param {remoting.ConnectionInfo} connectionInfo
 *
 * @constructor
 * @implements {base.Disposable}
 */
remoting.AppConnectedView = function(containerElement, connectionInfo) {

  /** @private */
  this.plugin_ = connectionInfo.plugin();

  /** @private */
  this.host_ = connectionInfo.host();

  var pingTimer = new base.RepeatingTimer(function () {
    var message = { timestamp: new Date().getTime() };
    var clientSession = connectionInfo.session();
    clientSession.sendClientMessage('pingRequest', JSON.stringify(message));
  }, CONNECTION_SPEED_PING_INTERVAL);

  var baseView = new remoting.ConnectedView(
      this.plugin_, containerElement,
      containerElement.querySelector('.mouse-cursor-overlay'));;

  var eventHook = new base.EventHook(
      this.plugin_.hostDesktop(),
      remoting.HostDesktop.Events.shapeChanged,
      remoting.windowShape.setDesktopRects.bind(remoting.windowShape));

  /** @private */
  this.disposables_ = new base.Disposables(pingTimer, baseView, eventHook);

  this.resizeHostToClientArea_().then(
    this.setPluginSize_.bind(this)
  );
};

/**
 * @return {void} Nothing.
 */
remoting.AppConnectedView.prototype.dispose = function() {
  base.dispose(this.disposables_);
};

/**
 * Resize the host to the dimensions of the current window.
 *
 * @return {Promise} A promise that resolves when the host finishes responding
 *   to the resize request.
 * @private
 */
remoting.AppConnectedView.prototype.resizeHostToClientArea_ = function() {
  var hostDesktop = this.plugin_.hostDesktop();
  var desktopScale = this.host_.options.desktopScale;

  return new Promise(function(/** Function */ resolve) {
    var eventHook = new base.EventHook(
        hostDesktop, remoting.HostDesktop.Events.sizeChanged, resolve);
    hostDesktop.resize(window.innerWidth * desktopScale,
                       window.innerHeight * desktopScale,
                       window.devicePixelRatio);
  });
};


/**
 * Adjust the size of the plugin according to the dimensions of the hostDesktop.
 *
 * @param {{width:number, height:number, xDpi:number, yDpi:number}} hostDesktop
 * @private
 */
remoting.AppConnectedView.prototype.setPluginSize_ = function(hostDesktop) {
  remoting.LoadingWindow.close();
  var hostSize = { width: hostDesktop.width, height: hostDesktop.height };
  var hostDpi = { x: hostDesktop.xDpi, y: hostDesktop.yDpi };
  var clientArea = { width: window.innerWidth, height: window.innerHeight };
  var newSize = remoting.DesktopViewport.choosePluginSize(
      clientArea, window.devicePixelRatio,
      hostSize, hostDpi, this.host_.options.desktopScale,
      true /* fullscreen */ , false /* shrinkToFit */ );

  this.plugin_.element().style.width = newSize.width + 'px';
  this.plugin_.element().style.height = newSize.height + 'px';
}

})();
