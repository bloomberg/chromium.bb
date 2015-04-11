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

  var baseView = new remoting.ConnectedView(
      this.plugin_, containerElement,
      containerElement.querySelector('.mouse-cursor-overlay'));

  var windowShapeHook = new base.EventHook(
      this.plugin_.hostDesktop(),
      remoting.HostDesktop.Events.shapeChanged,
      remoting.windowShape.setDesktopRects.bind(remoting.windowShape));

  var desktopSizeHook = new base.EventHook(
      this.plugin_.hostDesktop(),
      remoting.HostDesktop.Events.sizeChanged,
      this.onDesktopSizeChanged_.bind(this));

  /** @private */
  this.disposables_ = new base.Disposables(
      baseView, windowShapeHook, desktopSizeHook);

  this.resizeHostToClientArea_();
};

/**
 * @return {void} Nothing.
 */
remoting.AppConnectedView.prototype.dispose = function() {
  base.dispose(this.disposables_);
};

/**
 * Resize the host to the dimensions of the current window.
 * @private
 */
remoting.AppConnectedView.prototype.resizeHostToClientArea_ = function() {
  var hostDesktop = this.plugin_.hostDesktop();
  var desktopScale = this.host_.options.desktopScale;
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
  var newSize = remoting.DesktopViewport.choosePluginSize(
      clientArea, window.devicePixelRatio,
      hostSize, hostDpi, this.host_.options.desktopScale,
      true /* fullscreen */ , true /* shrinkToFit */ );

  this.plugin_.element().style.width = newSize.width + 'px';
  this.plugin_.element().style.height = newSize.height + 'px';
};

})();
