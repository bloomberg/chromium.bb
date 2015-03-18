// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides view port management utilities below for a desktop remoting session.
 * - Enabling bump scrolling
 * - Resizing the viewport to fit the host desktop
 * - Resizing the host desktop to fit the client viewport.
 */

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * @param {HTMLElement} rootElement The outer element with id=scroller that we
 *   are showing scrollbars on.
 * @param {remoting.HostDesktop} hostDesktop
 * @param {remoting.Host.Options} hostOptions
 *
 * @constructor
 * @implements {base.Disposable}
 */
remoting.DesktopViewport = function(rootElement, hostDesktop, hostOptions) {
  /** @private */
  this.rootElement_ = rootElement;
  /** @private */
  // TODO(kelvinp): Query the container by class name instead of id.
  this.pluginContainer_ = rootElement.querySelector('#client-container');
  /** @private */
  this.pluginElement_ = rootElement.querySelector('embed');
  /** @private */
  this.hostDesktop_ = hostDesktop;
  /** @private */
  this.hostOptions_ = hostOptions;
  /** @private {number?} */
  this.resizeTimer_ = null;
  /** @private {remoting.BumpScroller} */
  this.bumpScroller_ = null;
  // Bump-scroll test variables. Override to use a fake value for the width
  // and height of the client plugin so that bump-scrolling can be tested
  // without relying on the actual size of the host desktop.
  /** @private {number} */
  this.pluginWidthForBumpScrollTesting_ = 0;
  /** @private {number} */
  this.pluginHeightForBumpScrollTesting_ = 0;

  this.eventHooks_ = new base.Disposables(
      new base.EventHook(
        this.hostDesktop_, remoting.HostDesktop.Events.sizeChanged,
        this.onDesktopSizeChanged_.bind(this)),
      // TODO(kelvinp): Move window shape related logic into
      // remoting.AppConnectedView.
      new base.EventHook(
        this.hostDesktop_, remoting.HostDesktop.Events.shapeChanged,
        remoting.windowShape.setDesktopRects.bind(remoting.windowShape)));

  if (this.hostOptions_.resizeToClient) {
    // TODO(kelvinp): This call is required in app remoting to set the host
    // desktop size.  Move this to remoting.AppConnectedView later.
    this.resizeHostDesktop_();
  } else {
    this.onDesktopSizeChanged_();
  }
};

remoting.DesktopViewport.prototype.dispose = function() {
  base.dispose(this.eventHooks_);
  this.eventHooks_ = null;
  base.dispose(this.bumpScroller_);
  this.bumpScroller_ = null;
};

/**
 * @return {boolean} True if shrink-to-fit is enabled; false otherwise.
 */
remoting.DesktopViewport.prototype.getShrinkToFit = function() {
  return this.hostOptions_.shrinkToFit;
};

/**
 * @return {boolean} True if resize-to-client is enabled; false otherwise.
 */
remoting.DesktopViewport.prototype.getResizeToClient = function() {
  return this.hostOptions_.resizeToClient;
};

/**
 * @return {{top:number, left:number}} The top-left corner of the plugin.
 */
remoting.DesktopViewport.prototype.getPluginPositionForTesting = function() {
  /**
   * @param {number|string} value
   * @return {number}
   */
  function toFloat(value) {
    var number = parseFloat(value);
    return isNaN(number) ? 0 : number;
  }
  return {
    top: toFloat(this.pluginContainer_.style.marginTop),
    left: toFloat(this.pluginContainer_.style.marginLeft)
  };
};

/**
 * @param {number} width
 * @param {number} height
 */
remoting.DesktopViewport.prototype.setPluginSizeForBumpScrollTesting =
    function(width, height) {
  this.pluginWidthForBumpScrollTesting_ = width;
  this.pluginHeightForBumpScrollTesting_ = height;
};

/**
 * @return {remoting.BumpScroller}
 */
remoting.DesktopViewport.prototype.getBumpScrollerForTesting = function() {
  return this.bumpScroller_;
};

/**
 * Set the shrink-to-fit and resize-to-client flags and save them if this is
 * a Me2Me connection.
 *
 * @param {boolean} shrinkToFit True if the remote desktop should be scaled
 *     down if it is larger than the client window; false if scroll-bars
 *     should be added in this case.
 * @param {boolean} resizeToClient True if window resizes should cause the
 *     host to attempt to resize its desktop to match the client window size;
 *     false to disable this behaviour for subsequent window resizes--the
 *     current host desktop size is not restored in this case.
 * @return {void} Nothing.
 */
remoting.DesktopViewport.prototype.setScreenMode =
    function(shrinkToFit, resizeToClient) {
  if (resizeToClient && !this.hostOptions_.resizeToClient) {
    this.resizeHostDesktop_();
  }

  // If enabling shrink, reset bump-scroll offsets.
  var needsScrollReset = shrinkToFit && !this.hostOptions_.shrinkToFit;
  this.hostOptions_.shrinkToFit = shrinkToFit;
  this.hostOptions_.resizeToClient = resizeToClient;
  this.hostOptions_.save();
  this.updateScrollbarVisibility_();

  this.updateDimensions_();
  if (needsScrollReset) {
    this.resetScroll_();
  }
};

/**
 * Scroll the client plugin by the specified amount, keeping it visible.
 * Note that this is only used in content full-screen mode (not windowed or
 * browser full-screen modes), where window.scrollBy and the scrollTop and
 * scrollLeft properties don't work.
 *
 * @param {number} dx The amount by which to scroll horizontally. Positive to
 *     scroll right; negative to scroll left.
 * @param {number} dy The amount by which to scroll vertically. Positive to
 *     scroll down; negative to scroll up.
 * @return {boolean} False if the requested scroll had no effect because both
 *     vertical and horizontal edges of the screen have been reached.
 */
remoting.DesktopViewport.prototype.scroll = function(dx, dy) {
  /**
   * Helper function for x- and y-scrolling
   * @param {number|string} curr The current margin, eg. "10px".
   * @param {number} delta The requested scroll amount.
   * @param {number} windowBound The size of the window, in pixels.
   * @param {number} pluginBound The size of the plugin, in pixels.
   * @param {{stop: boolean}} stop Reference parameter used to indicate when
   *     the scroll has reached one of the edges and can be stopped in that
   *     direction.
   * @return {string} The new margin value.
   */
  var adjustMargin = function(curr, delta, windowBound, pluginBound, stop) {
    var minMargin = Math.min(0, windowBound - pluginBound);
    var result = (curr ? parseFloat(curr) : 0) - delta;
    result = Math.min(0, Math.max(minMargin, result));
    stop.stop = (result === 0 || result == minMargin);
    return result + 'px';
  };

  var style = this.pluginContainer_.style;

  var pluginWidth =
      this.pluginWidthForBumpScrollTesting_ || this.pluginElement_.clientWidth;
  var pluginHeight = this.pluginHeightForBumpScrollTesting_ ||
                     this.pluginElement_.clientHeight;

  var clientArea = this.getClientArea();
  var stopX = { stop: false };
  style.marginLeft =
      adjustMargin(style.marginLeft, dx, clientArea.width, pluginWidth, stopX);

  var stopY = { stop: false };
  style.marginTop =
      adjustMargin(style.marginTop, dy, clientArea.height, pluginHeight, stopY);
  return !stopX.stop || !stopY.stop;
};

/**
 * Enable or disable bump-scrolling. When disabling bump scrolling, also reset
 * the scroll offsets to (0, 0).
 * @param {boolean} enable True to enable bump-scrolling, false to disable it.
 */
remoting.DesktopViewport.prototype.enableBumpScroll = function(enable) {
  if (enable) {
    this.bumpScroller_ = new remoting.BumpScroller(this);
  } else {
    base.dispose(this.bumpScroller_);
    this.bumpScroller_ = null;
    this.resetScroll_();
  }
};

/**
 * This is a callback that gets called when the window is resized.
 *
 * @return {void} Nothing.
 */
remoting.DesktopViewport.prototype.onResize = function() {
  this.updateDimensions_();

  if (this.resizeTimer_) {
    window.clearTimeout(this.resizeTimer_);
    this.resizeTimer_ = null;
  }

  // Defer notifying the host of the change until the window stops resizing, to
  // avoid overloading the control channel with notifications.
  if (this.hostOptions_.resizeToClient) {
    var kResizeRateLimitMs = 250;
    var clientArea = this.getClientArea();
    this.resizeTimer_ = window.setTimeout(this.resizeHostDesktop_.bind(this),
                                          kResizeRateLimitMs);
  }

  // If bump-scrolling is enabled, adjust the plugin margins to fully utilize
  // the new window area.
  this.resetScroll_();
  this.updateScrollbarVisibility_();
};

/**
 * @return {{width:number, height:number}} The height of the window's client
 *     area. This differs between apps v1 and apps v2 due to the custom window
 *     borders used by the latter.
 */
remoting.DesktopViewport.prototype.getClientArea = function() {
  return remoting.windowFrame ?
      remoting.windowFrame.getClientArea() :
      { 'width': window.innerWidth, 'height': window.innerHeight };
};

/**
 * Notifies the host of the client's current dimensions and DPI.
 * Also takes into account per-host scaling factor, if configured.
 * @private
 */
remoting.DesktopViewport.prototype.resizeHostDesktop_ = function() {
  var clientArea = this.getClientArea();
  this.hostDesktop_.resize(clientArea.width * this.hostOptions_.desktopScale,
                           clientArea.height * this.hostOptions_.desktopScale,
                           window.devicePixelRatio);
};

/**
 * This is a callback that gets called when the plugin notifies us of a change
 * in the size of the remote desktop.
 *
 * @return {void} Nothing.
 * @private
 */
remoting.DesktopViewport.prototype.onDesktopSizeChanged_ = function() {
  var dimensions = this.hostDesktop_.getDimensions();
  console.log('desktop size changed: ' +
              dimensions.width + 'x' +
              dimensions.height +' @ ' +
              dimensions.xDpi + 'x' +
              dimensions.yDpi + ' DPI');
  this.updateDimensions_();
  this.updateScrollbarVisibility_();
};

/**
 * Called when the window or desktop size or the scaling settings change,
 * to set the scroll-bar visibility.
 *
 * TODO(jamiewalch): crbug.com/252796: Remove this once crbug.com/240772 is
 * fixed.
 */
remoting.DesktopViewport.prototype.updateScrollbarVisibility_ = function() {
  // TODO(kelvinp): Remove the check once app-remoting no longer depends on
  // this.
  if (!this.rootElement_) {
    return;
  }

  var needsScrollY = false;
  var needsScrollX = false;
  if (!this.hostOptions_.shrinkToFit) {
    // Determine whether or not horizontal or vertical scrollbars are
    // required, taking into account their width.
    var clientArea = this.getClientArea();
    var hostDesktop = this.hostDesktop_.getDimensions();
    needsScrollY = clientArea.height < hostDesktop.height;
    needsScrollX = clientArea.width < hostDesktop.width;
    var kScrollBarWidth = 16;
    if (needsScrollX && !needsScrollY) {
      needsScrollY = clientArea.height - kScrollBarWidth < hostDesktop.height;
    } else if (!needsScrollX && needsScrollY) {
      needsScrollX = clientArea.width - kScrollBarWidth < hostDesktop.width;
    }
  }

  this.rootElement_.classList.toggle('no-horizontal-scroll', !needsScrollX);
  this.rootElement_.classList.toggle('no-vertical-scroll', !needsScrollY);
};

remoting.DesktopViewport.prototype.updateDimensions_ = function() {
  var dimensions = this.hostDesktop_.getDimensions();
  if (dimensions.width === 0 || dimensions.height === 0) {
    return;
  }

  var desktopSize = { width: dimensions.width,
                      height: dimensions.height };
  var desktopDpi = { x: dimensions.xDpi,
                     y: dimensions.yDpi };
  var newSize = remoting.DesktopViewport.choosePluginSize(
      this.getClientArea(), window.devicePixelRatio,
      desktopSize, desktopDpi, this.hostOptions_.desktopScale,
      remoting.fullscreen.isActive(), this.hostOptions_.shrinkToFit);

  // Resize the plugin if necessary.
  console.log('plugin dimensions:' + newSize.width + 'x' + newSize.height);
  this.pluginElement_.style.width = newSize.width + 'px';
  this.pluginElement_.style.height = newSize.height + 'px';

  // When we receive the first plugin dimensions from the host, we know that
  // remote host has started.
  remoting.app.onVideoStreamingStarted();
};

/**
 * Helper function accepting client and host dimensions, and returning a chosen
 * size for the plugin element, in DIPs.
 *
 * @param {{width: number, height: number}} clientSizeDips Available client
 *     dimensions, in DIPs.
 * @param {number} clientPixelRatio Number of physical pixels per client DIP.
 * @param {{width: number, height: number}} desktopSize Size of the host desktop
 *     in physical pixels.
 * @param {{x: number, y: number}} desktopDpi DPI of the host desktop in both
 *     dimensions.
 * @param {number} desktopScale The scale factor configured for the host.
 * @param {boolean} isFullscreen True if full-screen mode is active.
 * @param {boolean} shrinkToFit True if shrink-to-fit should be applied.
 * @return {{width: number, height: number}} Chosen plugin dimensions, in DIPs.
 */
remoting.DesktopViewport.choosePluginSize = function(
    clientSizeDips, clientPixelRatio, desktopSize, desktopDpi, desktopScale,
    isFullscreen, shrinkToFit) {
  base.debug.assert(clientSizeDips.width > 0);
  base.debug.assert(clientSizeDips.height > 0);
  base.debug.assert(clientPixelRatio >= 1.0);
  base.debug.assert(desktopSize.width > 0);
  base.debug.assert(desktopSize.height > 0);
  base.debug.assert(desktopDpi.x > 0);
  base.debug.assert(desktopDpi.y > 0);
  base.debug.assert(desktopScale > 0);

  // We have the following goals in sizing the desktop display at the client:
  //  1. Avoid losing detail by down-scaling beyond 1:1 host:device pixels.
  //  2. Avoid up-scaling if that will cause the client to need scrollbars.
  //  3. Avoid introducing blurriness with non-integer up-scaling factors.
  //  4. Avoid having huge "letterboxes" around the desktop, if it's really
  //     small.
  //  5. Compensate for mismatched DPIs, so that the behaviour of features like
  //     shrink-to-fit matches their "natural" rather than their pixel size.
  //     e.g. with shrink-to-fit active a 1024x768 low-DPI host on a 640x480
  //     high-DPI client will be up-scaled to 1280x960, rather than displayed
  //     at 1:1 host:physical client pixels.
  //
  // To determine the ideal size we follow a four-stage process:
  //  1. Determine the "natural" size at which to display the desktop.
  //    a. Initially assume 1:1 mapping of desktop to client device pixels.
  //    b. If host DPI is less than the client's then up-scale accordingly.
  //    c. If desktopScale is configured for the host then allow that to
  //       reduce the amount of up-scaling from (b). e.g. if the client:host
  //       DPIs are 2:1 then a desktopScale of 1.5 would reduce the up-scale
  //       to 4:3, while a desktopScale of 3.0 would result in no up-scaling.
  //  2. If the natural size of the desktop is smaller than the client device
  //     then apply up-scaling by an integer scale factor to avoid excessive
  //     letterboxing.
  //  3. If shrink-to-fit is configured then:
  //     a. If the natural size exceeds the client size then apply down-scaling
  //        by an arbitrary scale factor.
  //     b. If we're in full-screen mode and the client & host aspect-ratios
  //        are radically different (e.g. the host is actually multi-monitor)
  //        then shrink-to-fit to the shorter dimension, rather than leaving
  //        huge letterboxes; the user can then bump-scroll around the desktop.
  //  4. If the overall scale factor is fractionally over an integer factor
  //     then reduce it to that integer factor, to avoid blurring.

  // All calculations are performed in device pixels.
  var clientWidth = clientSizeDips.width * clientPixelRatio;
  var clientHeight = clientSizeDips.height * clientPixelRatio;

  // 1. Determine a "natural" size at which to display the desktop.
  var scale = 1.0;

  // Determine the effective host device pixel ratio.
  // Note that we round up or down to the closest integer pixel ratio.
  var hostPixelRatioX = Math.round(desktopDpi.x / 96);
  var hostPixelRatioY = Math.round(desktopDpi.y / 96);
  var hostPixelRatio = Math.min(hostPixelRatioX, hostPixelRatioY);

  // Allow up-scaling to account for DPI.
  scale = Math.max(scale, clientPixelRatio / hostPixelRatio);

  // Allow some or all of the up-scaling to be cancelled by the desktopScale.
  if (desktopScale > 1.0) {
    scale = Math.max(1.0, scale / desktopScale);
  }

  // 2. If the host is still much smaller than the client, then up-scale to
  //    avoid wasting space, but only by an integer factor, to avoid blurring.
  if (desktopSize.width * scale <= clientWidth &&
      desktopSize.height * scale <= clientHeight) {
    var scaleX = Math.floor(clientWidth / desktopSize.width);
    var scaleY = Math.floor(clientHeight / desktopSize.height);
    scale = Math.min(scaleX, scaleY);
    base.debug.assert(scale >= 1.0);
  }

  // 3. Apply shrink-to-fit, if configured.
  if (shrinkToFit) {
    var scaleFitWidth = Math.min(scale, clientWidth / desktopSize.width);
    var scaleFitHeight = Math.min(scale, clientHeight / desktopSize.height);
    scale = Math.min(scaleFitHeight, scaleFitWidth);

    // If we're running full-screen then try to handle common side-by-side
    // multi-monitor combinations more intelligently.
    if (isFullscreen) {
      // If the host has two monitors each the same size as the client then
      // scale-to-fit will have the desktop occupy only 50% of the client area,
      // in which case it would be preferable to down-scale less and let the
      // user bump-scroll around ("scale-and-pan").
      // Triggering scale-and-pan if less than 65% of the client area would be
      // used adds enough fuzz to cope with e.g. 1280x800 client connecting to
      // a (2x1280)x1024 host nicely.
      // Note that we don't need to account for scrollbars while fullscreen.
      if (scale <= scaleFitHeight * 0.65) {
        scale = scaleFitHeight;
      }
      if (scale <= scaleFitWidth * 0.65) {
        scale = scaleFitWidth;
      }
    }
  }

  // 4. Avoid blurring for close-to-integer up-scaling factors.
  if (scale > 1.0) {
    var scaleBlurriness = scale / Math.floor(scale);
    if (scaleBlurriness < 1.1) {
      scale = Math.floor(scale);
    }
  }

  // Return the necessary plugin dimensions in DIPs.
  scale = scale / clientPixelRatio;
  var pluginWidth = Math.round(desktopSize.width * scale);
  var pluginHeight = Math.round(desktopSize.height * scale);
  return { width: pluginWidth, height: pluginHeight };
};

/** @private */
remoting.DesktopViewport.prototype.resetScroll_ = function() {
  this.pluginContainer_.style.marginTop = '0px';
  this.pluginContainer_.style.marginLeft = '0px';
};

}());
