// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class handling user-facing aspects of the client session.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * True to enable mouse lock.
 * This is currently disabled because the current client plugin does not
 * properly handle mouse lock and delegated large cursors at the same time.
 * This should be re-enabled (by removing this flag) once a version of
 * the plugin that supports both has reached Chrome Stable channel.
 * (crbug.com/429322).
 *
 * @type {boolean}
 */
remoting.enableMouseLock = false;

/**
 * @param {remoting.ClientSession} session
 * @param {HTMLElement} container
 * @param {remoting.Host} host
 * @param {remoting.DesktopConnectedView.Mode} mode The mode of this connection.
 * @param {string} defaultRemapKeys The default set of remap keys, to use
 *     when the client doesn't define any.
 * @param {function(remoting.Error, remoting.ClientPlugin): void} onInitialized
 * @constructor
 * @extends {base.EventSourceImpl}
 */
remoting.DesktopConnectedView = function(session, container, host, mode,
                                         defaultRemapKeys, onInitialized) {
  this.session_ = session;

  /** @type {HTMLElement} @private */
  this.container_ = container;

  /** @type {remoting.ClientPlugin} @private */
  this.plugin_ = null;

  /** @private */
  this.host_ = host;

  /** @private */
  this.mode_ = mode;

  /** @type {string} @private */
  this.defaultRemapKeys_ = defaultRemapKeys;

  /**
   * Called when the UI is finished initializing.
   * @type {function(remoting.Error, remoting.ClientPlugin):void}
   */
  this.onInitialized_ = onInitialized;

  /** @type {function(boolean=):void} @private */
  this.callOnFullScreenChanged_ = this.onFullScreenChanged_.bind(this)

  /** @private */
  this.callPluginLostFocus_ = this.pluginLostFocus_.bind(this);
  /** @private */
  this.callPluginGotFocus_ = this.pluginGotFocus_.bind(this);

  /** @type {number?} @private */
  this.notifyClientResolutionTimer_ = null;

  /** @type {Element} @private */
  this.debugRegionContainer_ =
      this.container_.querySelector('.debug-region-container');

  /** @type {Element} @private */
  this.mouseCursorOverlay_ =
      this.container_.querySelector('.mouse-cursor-overlay');

  /** @type {Element} */
  var img = this.mouseCursorOverlay_;
  /** @param {Event} event @private */
  this.updateMouseCursorPosition_ = function(event) {
    img.style.top = event.offsetY + 'px';
    img.style.left = event.offsetX + 'px';
  };

  /** @type {number?} @private */
  this.bumpScrollTimer_ = null;

  // Bump-scroll test variables. Override to use a fake value for the width
  // and height of the client plugin so that bump-scrolling can be tested
  // without relying on the actual size of the host desktop.
  /** @type {number} @private */
  this.pluginWidthForBumpScrollTesting_ = 0;
  /** @type {number} @private */
  this.pluginHeightForBumpScrollTesting_ = 0;

  /** @type {remoting.VideoFrameRecorder} @private */
  this.videoFrameRecorder_ = null;

  /** @private {base.Disposables} */
  this.eventHooks_ = null;

  this.defineEvents(base.values(remoting.DesktopConnectedView.Events));
};

base.extend(remoting.DesktopConnectedView, base.EventSourceImpl);

/** @enum {string} */
remoting.DesktopConnectedView.Events = {
  bumpScrollStarted: 'bumpScrollStarted',
  bumpScrollStopped: 'bumpScrollStopped'
};

// The mode of this session.
/** @enum {number} */
remoting.DesktopConnectedView.Mode = {
  IT2ME: 0,
  ME2ME: 1,
  APP_REMOTING: 2
};

// Keys for per-host settings.
remoting.DesktopConnectedView.KEY_REMAP_KEYS = 'remapKeys';
remoting.DesktopConnectedView.KEY_RESIZE_TO_CLIENT = 'resizeToClient';
remoting.DesktopConnectedView.KEY_SHRINK_TO_FIT = 'shrinkToFit';
remoting.DesktopConnectedView.KEY_DESKTOP_SCALE = 'desktopScale';

/**
 * Get host display name.
 *
 * @return {string}
 */
remoting.DesktopConnectedView.prototype.getHostDisplayName = function() {
  return this.host_.hostName;
};

/**
 * @return {remoting.DesktopConnectedView.Mode} The current state.
 */
remoting.DesktopConnectedView.prototype.getMode = function() {
  return this.mode_;
};

/**
 * @return {boolean} True if shrink-to-fit is enabled; false otherwise.
 */
remoting.DesktopConnectedView.prototype.getShrinkToFit = function() {
  return this.host_.options.shrinkToFit;
};

/**
 * @return {boolean} True if resize-to-client is enabled; false otherwise.
 */
remoting.DesktopConnectedView.prototype.getResizeToClient = function() {
  return this.host_.options.resizeToClient;
};

/**
 * @return {Element} The element that should host the plugin.
 */
remoting.DesktopConnectedView.prototype.getPluginContainer = function() {
  return this.container_.querySelector('.client-plugin-container')
};

/**
 * @return {{width: number, height: number}} The height of the window's client
 *     area. This differs between apps v1 and apps v2 due to the custom window
 *     borders used by the latter.
 * TODO: make private
 */
remoting.DesktopConnectedView.prototype.getClientArea_ = function() {
  return remoting.windowFrame ?
      remoting.windowFrame.getClientArea() :
      { 'width': window.innerWidth, 'height': window.innerHeight };
};

/**
 * @param {number} width
 * @param {number} height
 */
remoting.DesktopConnectedView.prototype.setPluginSizeForBumpScrollTesting =
    function(width, height) {
  this.pluginWidthForBumpScrollTesting_ = width;
  this.pluginHeightForBumpScrollTesting_ = height;
}

/**
 * Notifies the host of the client's current dimensions and DPI.
 * Also takes into account per-host scaling factor, if configured.
 * TODO: private
 */
remoting.DesktopConnectedView.prototype.notifyClientResolution_ = function() {
  var clientArea = this.getClientArea_();
  var desktopScale = this.host_.options.desktopScale;
  this.plugin_.hostDesktop().resize(clientArea.width * desktopScale,
                                    clientArea.height * desktopScale,
                                    window.devicePixelRatio);
};

/**
 * Adds <embed> element to the UI container and readies the session object.
 *
 * @param {function(string, string):boolean} onExtensionMessage The handler for
 *     protocol extension messages. Returns true if a message is recognized;
 *     false otherwise.
 * @param {Array<string>} requiredCapabilities A list of capabilities
 *     required by this application.
 */
remoting.DesktopConnectedView.prototype.createPluginAndConnect =
    function(onExtensionMessage, requiredCapabilities) {
  this.plugin_ = remoting.ClientPlugin.factory.createPlugin(
      this.getPluginContainer(),
      onExtensionMessage, requiredCapabilities);
  var that = this;
  this.host_.options.load().then(function(){
    that.plugin_.initialize(that.onPluginInitialized_.bind(that));
  });
};

/**
 * @param {boolean} initialized
 */
remoting.DesktopConnectedView.prototype.onPluginInitialized_ = function(
    initialized) {
  if (!initialized) {
    console.error('ERROR: remoting plugin not loaded');
    this.onInitialized_(remoting.Error.MISSING_PLUGIN, this.plugin_);
    return;
  }

  if (!this.plugin_.isSupportedVersion()) {
    this.onInitialized_(remoting.Error.BAD_PLUGIN_VERSION, this.plugin_);
    return;
  }

  // Show the Send Keys menu only if the plugin has the injectKeyEvent feature,
  // and the Ctrl-Alt-Del button only in Me2Me mode.
  if (!this.plugin_.hasFeature(
          remoting.ClientPlugin.Feature.INJECT_KEY_EVENT)) {
    var sendKeysElement = document.getElementById('send-keys-menu');
    sendKeysElement.hidden = true;
  } else if (this.mode_ != remoting.DesktopConnectedView.Mode.ME2ME &&
      this.mode_ != remoting.DesktopConnectedView.Mode.APP_REMOTING) {
    var sendCadElement = document.getElementById('send-ctrl-alt-del');
    sendCadElement.hidden = true;
  }

  // Apply customized key remappings if the plugin supports remapKeys.
  if (this.plugin_.hasFeature(remoting.ClientPlugin.Feature.REMAP_KEY)) {
    this.applyRemapKeys_(true);
  }

  // TODO(wez): Only allow mouse lock if the app has the pointerLock permission.
  // Enable automatic mouse-lock.
  if (remoting.enableMouseLock &&
      this.plugin_.hasFeature(remoting.ClientPlugin.Feature.ALLOW_MOUSE_LOCK)) {
    this.plugin_.allowMouseLock();
  }

  base.dispose(this.eventHooks_);
  var hostDesktop = this.plugin_.hostDesktop();
  this.eventHooks_ = new base.Disposables(
      new base.EventHook(
        hostDesktop, remoting.HostDesktop.Events.sizeChanged,
        this.onDesktopSizeChanged_.bind(this)),
      new base.EventHook(
        hostDesktop, remoting.HostDesktop.Events.shapeChanged,
        this.onDesktopShapeChanged_.bind(this)));

  this.plugin_.setMouseCursorHandler(this.updateMouseCursorImage_.bind(this));

  this.onInitialized_(remoting.Error.NONE, this.plugin_);
};

/**
 * This is a callback that gets called when the plugin notifies us of a change
 * in the size of the remote desktop.
 *
 * @return {void} Nothing.
 * @private
 */
remoting.DesktopConnectedView.prototype.onDesktopSizeChanged_ = function() {
  var desktop = this.plugin_.hostDesktop().getDimensions();
  console.log('desktop size changed: ' +
              desktop.width + 'x' +
              desktop.height +' @ ' +
              desktop.xDpi + 'x' +
              desktop.yDpi + ' DPI');
  this.updateDimensions();
  this.updateScrollbarVisibility();
};

/**
 * Sets the non-click-through area of the client in response to notifications
 * from the plugin of desktop shape changes.
 *
 * @param {Array<Array<number>>} rects List of rectangles comprising the
 *     desktop shape.
 * @return {void} Nothing.
 * @private
 */
remoting.DesktopConnectedView.prototype.onDesktopShapeChanged_ = function(
    rects) {
  // Build the list of rects for the input region.
  var inputRegion = [];
  for (var i = 0; i < rects.length; ++i) {
    var rect = {};
    rect.left = rects[i][0];
    rect.top = rects[i][1];
    rect.width = rects[i][2];
    rect.height = rects[i][3];
    inputRegion.push(rect);
  }

  remoting.windowShape.setDesktopRects(inputRegion);
};

/**
 * This is a callback that gets called when the window is resized.
 *
 * @return {void} Nothing.
 */
remoting.DesktopConnectedView.prototype.onResize = function() {
  this.updateDimensions();

  if (this.notifyClientResolutionTimer_) {
    window.clearTimeout(this.notifyClientResolutionTimer_);
    this.notifyClientResolutionTimer_ = null;
  }

  // Defer notifying the host of the change until the window stops resizing, to
  // avoid overloading the control channel with notifications.
  if (this.getResizeToClient()) {
    var kResizeRateLimitMs = 250;
    var clientArea = this.getClientArea_();
    this.notifyClientResolutionTimer_ = window.setTimeout(
        this.notifyClientResolution_.bind(this),
        kResizeRateLimitMs);
  }

  // If bump-scrolling is enabled, adjust the plugin margins to fully utilize
  // the new window area.
  this.resetScroll_();

  this.updateScrollbarVisibility();
};

/**
 * Callback that the plugin invokes to indicate when the connection is
 * ready.
 *
 * @param {boolean} ready True if the connection is ready.
 */
remoting.DesktopConnectedView.prototype.onConnectionReady = function(ready) {
  if (!ready) {
    this.container_.classList.add('session-client-inactive');
  } else {
    this.container_.classList.remove('session-client-inactive');
  }
};

/**
 * Deletes the <embed> element from the container, without sending a
 * session_terminate request.  This is to be called when the session was
 * disconnected by the Host.
 *
 * @return {void} Nothing.
 */
remoting.DesktopConnectedView.prototype.removePlugin = function() {
  if (this.plugin_) {
    base.dispose(this.eventHooks_);
    this.eventHooks_ = null;
    this.plugin_.element().removeEventListener(
        'focus', this.callPluginGotFocus_, false);
    this.plugin_.element().removeEventListener(
        'blur', this.callPluginLostFocus_, false);
    this.plugin_.dispose();
    this.plugin_ = null;
  }

  this.updateClientSessionUi_(null);
};

/**
 * @param {remoting.ClientSession} clientSession The active session, or null if
 *     there is no connection.
 */
remoting.DesktopConnectedView.prototype.updateClientSessionUi_ = function(
    clientSession) {
  if (clientSession == null) {
    if (remoting.windowFrame) {
      remoting.windowFrame.setDesktopConnectedView(null);
    }
    if (remoting.toolbar) {
      remoting.toolbar.setDesktopConnectedView(null);
    }
    if (remoting.optionsMenu) {
      remoting.optionsMenu.setDesktopConnectedView(null);
    }

    document.body.classList.remove('connected');
    this.container_.removeEventListener(
        'mousemove', this.updateMouseCursorPosition_, true);
    // Stop listening for full-screen events.
    remoting.fullscreen.removeListener(this.callOnFullScreenChanged_);

  } else {
    if (remoting.windowFrame) {
      remoting.windowFrame.setDesktopConnectedView(this);
    }
    if (remoting.toolbar) {
      remoting.toolbar.setDesktopConnectedView(this);
    }
    if (remoting.optionsMenu) {
      remoting.optionsMenu.setDesktopConnectedView(this);
    }

    if (this.getResizeToClient()) {
      this.notifyClientResolution_();
    }

    document.body.classList.add('connected');
    this.container_.addEventListener(
        'mousemove', this.updateMouseCursorPosition_, true);
    // Activate full-screen related UX.
    remoting.fullscreen.addListener(this.callOnFullScreenChanged_);
    this.onDesktopSizeChanged_();
    this.setFocusHandlers_();
  }
};

/**
 * @return {{top: number, left:number}} The top-left corner of the plugin.
 */
remoting.DesktopConnectedView.prototype.getPluginPositionForTesting = function(
    ) {
  var style = this.container_.style;
  return {
    top: parseFloat(style.marginTop),
    left: parseFloat(style.marginLeft)
  };
};

/**
 * Constrains the focus to the plugin element.
 * @private
 */
remoting.DesktopConnectedView.prototype.setFocusHandlers_ = function() {
  this.plugin_.element().addEventListener(
      'focus', this.callPluginGotFocus_, false);
  this.plugin_.element().addEventListener(
      'blur', this.callPluginLostFocus_, false);
  this.plugin_.element().focus();
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
remoting.DesktopConnectedView.prototype.setScreenMode =
    function(shrinkToFit, resizeToClient) {
  if (resizeToClient && !this.getResizeToClient()) {
    this.notifyClientResolution_();
  }

  // If enabling shrink, reset bump-scroll offsets.
  var needsScrollReset = shrinkToFit && !this.getShrinkToFit();

  this.host_.options.shrinkToFit = shrinkToFit;
  this.host_.options.resizeToClient = resizeToClient;
  this.updateScrollbarVisibility();
  this.host_.options.save();

  this.updateDimensions();
  if (needsScrollReset) {
    this.resetScroll_();
  }
};

/**
 * Sets and stores the scale factor to apply to host sizing requests.
 * The desktopScale applies to the dimensions reported to the host, not
 * to the client DPI reported to it.
 *
 * @param {number} desktopScale Scale factor to apply.
 */
remoting.DesktopConnectedView.prototype.setDesktopScale = function(
    desktopScale) {
  this.host_.options.desktopScale = desktopScale;

  // onResize() will update the plugin size and scrollbars for the new
  // scaled plugin dimensions, and send a client resolution notification.
  this.onResize();

  // Save the new desktop scale setting.
  this.host_.options.save();
};

/**
 * Called when the full-screen status has changed, either via the
 * remoting.Fullscreen class, or via a system event such as the Escape key
 *
 * @param {boolean=} fullscreen True if the app is entering full-screen mode;
 *     false if it is leaving it.
 * @private
 */
remoting.DesktopConnectedView.prototype.onFullScreenChanged_ = function (
    fullscreen) {
  this.enableBumpScroll_(fullscreen);
};

/**
 * Callback function called when the plugin element gets focus.
 */
remoting.DesktopConnectedView.prototype.pluginGotFocus_ = function() {
  remoting.clipboard.initiateToHost();
};

/**
 * Callback function called when the plugin element loses focus.
 */
remoting.DesktopConnectedView.prototype.pluginLostFocus_ = function() {
  if (this.plugin_) {
    // Release all keys to prevent them becoming 'stuck down' on the host.
    this.plugin_.releaseAllKeys();
    if (this.plugin_.element()) {
      // Focus should stay on the element, not (for example) the toolbar.
      // Due to crbug.com/246335, we can't restore the focus immediately,
      // otherwise the plugin gets confused about whether or not it has focus.
      window.setTimeout(
          this.plugin_.element().focus.bind(this.plugin_.element()), 0);
    }
  }
};

/**
 * @param {string} url
 * @param {number} hotspotX
 * @param {number} hotspotY
 */
remoting.DesktopConnectedView.prototype.updateMouseCursorImage_ =
    function(url, hotspotX, hotspotY) {
  this.mouseCursorOverlay_.hidden = !url;
  if (url) {
    this.mouseCursorOverlay_.style.marginLeft = '-' + hotspotX + 'px';
    this.mouseCursorOverlay_.style.marginTop = '-' + hotspotY + 'px';
    this.mouseCursorOverlay_.src = url;
  }
};

/**
 * Enable or disable bump-scrolling. When disabling bump scrolling, also reset
 * the scroll offsets to (0, 0).
 * @param {boolean=} enable True to enable bump-scrolling, false to disable it.
 * @private
 */
remoting.DesktopConnectedView.prototype.enableBumpScroll_ = function(enable) {
  var element = /** @type{HTMLElement} */ (document.documentElement);
  if (enable) {
    /** @type {null|function(Event):void} */
    this.onMouseMoveRef_ = this.onMouseMove_.bind(this);
    element.addEventListener('mousemove', this.onMouseMoveRef_, false);
  } else {
    element.removeEventListener('mousemove', this.onMouseMoveRef_, false);
    this.onMouseMoveRef_ = null;
    this.resetScroll_();
  }
};

remoting.DesktopConnectedView.prototype.resetScroll_ = function() {
  this.container_.style.marginTop = '0px';
  this.container_.style.marginLeft = '0px';
};

/**
 * @param {Event} event The mouse event.
 * @private
 */
remoting.DesktopConnectedView.prototype.onMouseMove_ = function(event) {
  if (this.bumpScrollTimer_) {
    window.clearTimeout(this.bumpScrollTimer_);
    this.bumpScrollTimer_ = null;
  }

  /**
   * Compute the scroll speed based on how close the mouse is to the edge.
   * @param {number} mousePos The mouse x- or y-coordinate
   * @param {number} size The width or height of the content area.
   * @return {number} The scroll delta, in pixels.
   */
  var computeDelta = function(mousePos, size) {
    var threshold = 10;
    if (mousePos >= size - threshold) {
      return 1 + 5 * (mousePos - (size - threshold)) / threshold;
    } else if (mousePos <= threshold) {
      return -1 - 5 * (threshold - mousePos) / threshold;
    }
    return 0;
  };

  var clientArea = this.getClientArea_();
  var dx = computeDelta(event.x, clientArea.width);
  var dy = computeDelta(event.y, clientArea.height);

  if (dx != 0 || dy != 0) {
    this.raiseEvent(remoting.DesktopConnectedView.Events.bumpScrollStarted);
    /** @type {remoting.DesktopConnectedView} */
    var that = this;
    /**
     * Scroll the view, and schedule a timer to do so again unless we've hit
     * the edges of the screen. This timer is cancelled when the mouse moves.
     * @param {number} expected The time at which we expect to be called.
     */
    var repeatScroll = function(expected) {
      /** @type {number} */
      var now = new Date().getTime();
      /** @type {number} */
      var timeout = 10;
      var lateAdjustment = 1 + (now - expected) / timeout;
      if (that.scroll_(lateAdjustment * dx, lateAdjustment * dy)) {
        that.raiseEvent(remoting.DesktopConnectedView.Events.bumpScrollStopped);
      } else {
        that.bumpScrollTimer_ = window.setTimeout(
            function() { repeatScroll(now + timeout); },
            timeout);
      }
    };
    repeatScroll(new Date().getTime());
  }
};

/**
 * Scroll the client plugin by the specified amount, keeping it visible.
 * Note that this is only used in content full-screen mode (not windowed or
 * browser full-screen modes), where window.scrollBy and the scrollTop and
 * scrollLeft properties don't work.
 * @param {number} dx The amount by which to scroll horizontally. Positive to
 *     scroll right; negative to scroll left.
 * @param {number} dy The amount by which to scroll vertically. Positive to
 *     scroll down; negative to scroll up.
 * @return {boolean} True if the requested scroll had no effect because both
 *     vertical and horizontal edges of the screen have been reached.
 * @private
 */
remoting.DesktopConnectedView.prototype.scroll_ = function(dx, dy) {
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
    stop.stop = (result == 0 || result == minMargin);
    return result + 'px';
  };

  var plugin = this.plugin_.element();
  var style = this.container_.style;

  var stopX = { stop: false };
  var clientArea = this.getClientArea_();
  style.marginLeft = adjustMargin(style.marginLeft, dx, clientArea.width,
      this.pluginWidthForBumpScrollTesting_ || plugin.clientWidth, stopX);

  var stopY = { stop: false };
  style.marginTop = adjustMargin(
      style.marginTop, dy, clientArea.height,
      this.pluginHeightForBumpScrollTesting_ || plugin.clientHeight, stopY);
  return stopX.stop && stopY.stop;
};

/**
 * Refreshes the plugin's dimensions, taking into account the sizes of the
 * remote desktop and client window, and the current scale-to-fit setting.
 *
 * @return {void} Nothing.
 */
remoting.DesktopConnectedView.prototype.updateDimensions = function() {
  var desktopSize = this.plugin_.hostDesktop().getDimensions();

  if (desktopSize.width === 0 ||
      desktopSize.height === 0) {
    return;
  }

  var desktopDpi = { x: desktopSize.xDpi,
                     y: desktopSize.yDpi };
  var newSize = remoting.DesktopConnectedView.choosePluginSize(
      this.getClientArea_(), window.devicePixelRatio,
      desktopSize, desktopDpi, this.host_.options.desktopScale,
      remoting.fullscreen.isActive(), this.getShrinkToFit());

  // Resize the plugin if necessary.
  console.log('plugin dimensions:' + newSize.width + 'x' + newSize.height);
  this.plugin_.element().style.width = newSize.width + 'px';
  this.plugin_.element().style.height = newSize.height + 'px';

  // When we receive the first plugin dimensions from the host, we know that
  // remote host has started.
  remoting.app.onVideoStreamingStarted();
}

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
remoting.DesktopConnectedView.choosePluginSize = function(
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
}

/**
 * Called when the window or desktop size or the scaling settings change,
 * to set the scroll-bar visibility.
 *
 * TODO(jamiewalch): crbug.com/252796: Remove this once crbug.com/240772 is
 * fixed.
 */
remoting.DesktopConnectedView.prototype.updateScrollbarVisibility = function() {
  var scroller = document.getElementById('scroller');
  if (!scroller) {
    return;
  }

  var needsVerticalScroll = false;
  var needsHorizontalScroll = false;
  if (!this.getShrinkToFit()) {
    // Determine whether or not horizontal or vertical scrollbars are
    // required, taking into account their width.
    var clientArea = this.getClientArea_();
    var desktopSize = this.plugin_.hostDesktop().getDimensions();
    needsVerticalScroll = clientArea.height < desktopSize.height;
    needsHorizontalScroll = clientArea.width < desktopSize.width;
    var kScrollBarWidth = 16;
    if (needsHorizontalScroll && !needsVerticalScroll) {
      needsVerticalScroll =
          clientArea.height - kScrollBarWidth < desktopSize.height;
    } else if (!needsHorizontalScroll && needsVerticalScroll) {
      needsHorizontalScroll =
          clientArea.width - kScrollBarWidth < desktopSize.width;
    }
  }

  if (needsHorizontalScroll) {
    scroller.classList.remove('no-horizontal-scroll');
  } else {
    scroller.classList.add('no-horizontal-scroll');
  }
  if (needsVerticalScroll) {
    scroller.classList.remove('no-vertical-scroll');
  } else {
    scroller.classList.add('no-vertical-scroll');
  }
};

/**
 * Sets and stores the key remapping setting for the current host.
 *
 * @param {string} remappings Comma separated list of key remappings.
 */
remoting.DesktopConnectedView.prototype.setRemapKeys = function(remappings) {
  // Cancel any existing remappings and apply the new ones.
  this.applyRemapKeys_(false);
  this.host_.options.remapKeys = remappings;
  this.applyRemapKeys_(true);

  // Save the new remapping setting.
  this.host_.options.save();
};

/**
 * Applies the configured key remappings to the session, or resets them.
 *
 * @param {boolean} apply True to apply remappings, false to cancel them.
 */
remoting.DesktopConnectedView.prototype.applyRemapKeys_ = function(apply) {
  var remapKeys = this.host_.options.remapKeys;
  if (remapKeys == '') {
    remapKeys = this.defaultRemapKeys_;
    if (remapKeys == '') {
      return;
    }
  }

  var remappings = remapKeys.split(',');
  for (var i = 0; i < remappings.length; ++i) {
    var keyCodes = remappings[i].split('>');
    if (keyCodes.length != 2) {
      console.log('bad remapKey: ' + remappings[i]);
      continue;
    }
    var fromKey = parseInt(keyCodes[0], 0);
    var toKey = parseInt(keyCodes[1], 0);
    if (!fromKey || !toKey) {
      console.log('bad remapKey code: ' + remappings[i]);
      continue;
    }
    if (apply) {
      console.log('remapKey 0x' + fromKey.toString(16) +
                  '>0x' + toKey.toString(16));
      this.plugin_.remapKey(fromKey, toKey);
    } else {
      console.log('cancel remapKey 0x' + fromKey.toString(16));
      this.plugin_.remapKey(fromKey, fromKey);
    }
  }
};

/**
 * Sends a key combination to the remoting client, by sending down events for
 * the given keys, followed by up events in reverse order.
 *
 * @param {Array<number>} keys Key codes to be sent.
 * @return {void} Nothing.
 * @private
 */
remoting.DesktopConnectedView.prototype.sendKeyCombination_ = function(keys) {
  for (var i = 0; i < keys.length; i++) {
    this.plugin_.injectKeyEvent(keys[i], true);
  }
  for (var i = 0; i < keys.length; i++) {
    this.plugin_.injectKeyEvent(keys[i], false);
  }
};

/**
 * Sends a Ctrl-Alt-Del sequence to the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.DesktopConnectedView.prototype.sendCtrlAltDel = function() {
  console.log('Sending Ctrl-Alt-Del.');
  this.sendKeyCombination_([0x0700e0, 0x0700e2, 0x07004c]);
};

/**
 * Sends a Print Screen keypress to the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.DesktopConnectedView.prototype.sendPrintScreen = function() {
  console.log('Sending Print Screen.');
  this.sendKeyCombination_([0x070046]);
};

/**
 * Requests that the host pause or resume video updates.
 *
 * @param {boolean} pause True to pause video, false to resume.
 * @return {void} Nothing.
 */
remoting.DesktopConnectedView.prototype.pauseVideo = function(pause) {
  if (this.plugin_) {
    this.plugin_.pauseVideo(pause);
  }
};

/**
 * Requests that the host pause or resume audio.
 *
 * @param {boolean} pause True to pause audio, false to resume.
 * @return {void} Nothing.
 */
remoting.DesktopConnectedView.prototype.pauseAudio = function(pause) {
  if (this.plugin_) {
    this.plugin_.pauseAudio(pause)
  }
};

remoting.DesktopConnectedView.prototype.initVideoFrameRecorder = function() {
  this.videoFrameRecorder_ = new remoting.VideoFrameRecorder(this.plugin_);
};

/**
 * Returns true if the ClientSession can record video frames to a file.
 * @return {boolean}
 */
remoting.DesktopConnectedView.prototype.canRecordVideo = function() {
  return !!this.videoFrameRecorder_;
};

/**
 * Returns true if the ClientSession is currently recording video frames.
 * @return {boolean}
 */
remoting.DesktopConnectedView.prototype.isRecordingVideo = function() {
  if (!this.videoFrameRecorder_) {
    return false;
  }
  return this.videoFrameRecorder_.isRecording();
};

/**
 * Starts or stops recording of video frames.
 */
remoting.DesktopConnectedView.prototype.startStopRecording = function() {
  if (this.videoFrameRecorder_) {
    this.videoFrameRecorder_.startStopRecording();
  }
};

/**
 * Handles protocol extension messages.
 * @param {string} type Type of extension message.
 * @param {Object} message The parsed extension message data.
 * @return {boolean} True if the message was recognized, false otherwise.
 */
remoting.DesktopConnectedView.prototype.handleExtensionMessage =
    function(type, message) {
  if (this.videoFrameRecorder_) {
    return this.videoFrameRecorder_.handleMessage(type, message);
  }
  return false;
};

/**
 * Handles dirty region debug messages.
 *
 * @param {{rects:Array<Array<number>>}} region Dirty region of the latest
 *     frame.
 */
remoting.DesktopConnectedView.prototype.handleDebugRegion = function(region) {
  while (this.debugRegionContainer_.firstChild) {
    this.debugRegionContainer_.removeChild(
        this.debugRegionContainer_.firstChild);
  }
  if (region.rects) {
    var rects = region.rects;
    for (var i = 0; i < rects.length; ++i) {
      var rect = document.createElement('div');
      rect.classList.add('debug-region-rect');
      rect.style.left = rects[i][0] + 'px';
      rect.style.top = rects[i][1] +'px';
      rect.style.width = rects[i][2] +'px';
      rect.style.height = rects[i][3] + 'px';
      this.debugRegionContainer_.appendChild(rect);
    }
  }
}
