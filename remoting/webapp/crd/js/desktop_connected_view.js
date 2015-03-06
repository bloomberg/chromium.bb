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

  /** @private {HTMLElement} */
  this.container_ = container;

  /** @private {remoting.ClientPlugin} */
  this.plugin_ = null;

  /** @private */
  this.host_ = host;

  /** @private */
  this.mode_ = mode;

  /** @private {string} */
  this.defaultRemapKeys_ = defaultRemapKeys;

  /**
   * Called when the UI is finished initializing.
   * @type {function(remoting.Error, remoting.ClientPlugin):void}
   */
  this.onInitialized_ = onInitialized;

  /** @private */
  this.callPluginLostFocus_ = this.pluginLostFocus_.bind(this);
  /** @private */
  this.callPluginGotFocus_ = this.pluginGotFocus_.bind(this);
  /** @private {Element} */
  this.debugRegionContainer_ =
      this.container_.querySelector('.debug-region-container');

  /** @private {Element} */
  this.mouseCursorOverlay_ =
      this.container_.querySelector('.mouse-cursor-overlay');

  /** @private {remoting.DesktopViewport} */
  this.viewport_ = null;

  /** @type {Element} */
  var img = this.mouseCursorOverlay_;
  /**
   * @param {Event} event
   * @private
   */
  this.updateMouseCursorPosition_ = function(event) {
    img.style.top = event.offsetY + 'px';
    img.style.left = event.offsetX + 'px';
  };

  /** @private {remoting.VideoFrameRecorder} */
  this.videoFrameRecorder_ = null;

  /** private {base.Disposable} */
  this.eventHooks_ = null;
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
  if (this.viewport_) {
    return this.viewport_.getShrinkToFit();
  }
  return false;
};

/**
 * @return {boolean} True if resize-to-client is enabled; false otherwise.
 */
remoting.DesktopConnectedView.prototype.getResizeToClient = function() {
  if (this.viewport_) {
    return this.viewport_.getResizeToClient();
  }
  return false;
};

/**
 * @return {Element} The element that should host the plugin.
 * @private
 */
remoting.DesktopConnectedView.prototype.getPluginContainer_ = function() {
  return this.container_.querySelector('.client-plugin-container');
};

/** @return {remoting.DesktopViewport} */
remoting.DesktopConnectedView.prototype.getViewportForTesting = function() {
  return this.viewport_;
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
      this.getPluginContainer_(),
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

  this.plugin_.setMouseCursorHandler(this.updateMouseCursorImage_.bind(this));

  this.onInitialized_(remoting.Error.NONE, this.plugin_);
};

/**
 * This is a callback that gets called when the window is resized.
 *
 * @return {void} Nothing.
 * @private.
 */
remoting.DesktopConnectedView.prototype.onResize_ = function() {
  if (this.viewport_) {
    this.viewport_.onResize();
  }
};

/**
 * Called when the app window is hidden.
 * @return {void} Nothing.
 */
remoting.DesktopConnectedView.prototype.onVisibilityChanged_ = function() {
  this.pauseVideo(document.hidden);
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
  if (clientSession === null) {
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
    base.dispose(this.eventHooks_);
    this.eventHooks_ = null;
    base.dispose(this.viewport_);
    this.viewport_ = null;
  } else {
    var scrollerElement = document.getElementById('scroller');
    this.viewport_ = new remoting.DesktopViewport(
        scrollerElement || document.body,
        this.plugin_.hostDesktop(),
        this.host_.options);
    if (remoting.windowFrame) {
      remoting.windowFrame.setDesktopConnectedView(this);
    }
    if (remoting.toolbar) {
      remoting.toolbar.setDesktopConnectedView(this);
    }
    if (remoting.optionsMenu) {
      remoting.optionsMenu.setDesktopConnectedView(this);
    }

    document.body.classList.add('connected');
    this.container_.addEventListener(
        'mousemove', this.updateMouseCursorPosition_, true);
    // Activate full-screen related UX.
    this.setFocusHandlers_();
    this.eventHooks_ = new base.Disposables(
      new base.DomEventHook(window, 'resize', this.onResize_.bind(this), false),
      new base.DomEventHook(document, 'visibilitychange',
                            this.onVisibilityChanged_.bind(this), false),
      new remoting.Fullscreen.EventHook(this.onFullScreenChanged_.bind(this))
    );
    this.onFullScreenChanged_(remoting.fullscreen.isActive());
  }
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
  this.viewport_.setScreenMode(shrinkToFit, resizeToClient);
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
  if (this.viewport_) {
    // When a window goes full-screen, a resize event is triggered, but the
    // Fullscreen.isActive call is not guaranteed to return true until the
    // full-screen event is triggered. In apps v2, the size of the window's
    // client area is calculated differently in full-screen mode, so register
    // for both events.
    this.viewport_.onResize();
    this.viewport_.enableBumpScroll(Boolean(fullscreen));
  }
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
};
