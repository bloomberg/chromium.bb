// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Full-screen implementation for apps v2, using chrome.AppWindow.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @implements {remoting.Fullscreen}
 */
remoting.FullscreenAppsV2 = function() {
  /**
   * @type {boolean} True if maximize/restore events are being hooked.
   * @private
   */
  this.hookingWindowEvents_ = false;

  /**
   * @type {boolean} True if the next onRestored event should cause callbacks
   *     to be notified of a full-screen changed event. onRestored fires when
   *     full-screen mode is exited and also when the window is restored from
   *     being minimized; callbacks should not be notified of the latter.
   * @private
   */
  this.notifyCallbacksOnRestore_ = this.isActive();

  /**
   * @type {string} Internal 'full-screen changed' event name
   * @private
   */
  this.kEventName_ = '_fullscreenchanged';

  /**
   * @type {base.EventSource}
   * @private
   */
  this.eventSource_ = new base.EventSource();
  this.eventSource_.defineEvents([this.kEventName_]);

  chrome.app.window.current().onFullscreened.addListener(
      this.onFullscreened_.bind(this));
  chrome.app.window.current().onMaximized.addListener(
      this.onMaximized_.bind(this));
  chrome.app.window.current().onRestored.addListener(
      this.onRestored_.bind(this));
};

remoting.FullscreenAppsV2.prototype.activate = function(
    fullscreen, opt_onDone) {
  if (opt_onDone) {
    if (this.isActive() == fullscreen) {
      opt_onDone();
    } else {
      /** @type {remoting.Fullscreen} */
      var that = this;
      var callbackAndRemoveListener = function() {
        that.removeListener(callbackAndRemoveListener);
        opt_onDone();
      };
      this.addListener(callbackAndRemoveListener);
    }
  }

  if (fullscreen) {
    chrome.app.window.current().fullscreen();
  } else if (this.isActive()) {
    chrome.app.window.current().restore();
  }
};

remoting.FullscreenAppsV2.prototype.toggle = function() {
  this.activate(!this.isActive());
};

remoting.FullscreenAppsV2.prototype.isActive = function() {
  return chrome.app.window.current().isFullscreen();
};

remoting.FullscreenAppsV2.prototype.addListener = function(callback) {
  this.eventSource_.addEventListener(this.kEventName_, callback);
};

remoting.FullscreenAppsV2.prototype.removeListener = function(callback) {
  this.eventSource_.removeEventListener(this.kEventName_, callback);
};

remoting.FullscreenAppsV2.prototype.syncWithMaximize = function(sync) {
  if (sync && chrome.app.window.current().isMaximized()) {
    chrome.app.window.current().restore();
    this.activate(true);
  }
  this.hookingWindowEvents_ = sync;
};

remoting.FullscreenAppsV2.prototype.onFullscreened_ = function() {
  this.notifyCallbacksOnRestore_ = true;
  this.eventSource_.raiseEvent(this.kEventName_, true);
  document.body.classList.add('fullscreen');
};

remoting.FullscreenAppsV2.prototype.onMaximized_ = function() {
  if (this.hookingWindowEvents_) {
    chrome.app.window.current().restore();
    this.activate(true);
  }
};

remoting.FullscreenAppsV2.prototype.onRestored_ = function() {
  // TODO(jamiewalch): ChromeOS generates a spurious onRestore event if
  // fullscreen() is called from an onMaximized handler (crbug.com/394819),
  // so ignore the callback if the window is still full-screen.
  if (this.isActive()) {
    return;
  }

  document.body.classList.remove('fullscreen');
  if (this.hookingWindowEvents_) {
    this.activate(false);
  }
  if (this.notifyCallbacksOnRestore_) {
    this.notifyCallbacksOnRestore_ = false;
    this.eventSource_.raiseEvent(this.kEventName_, false);
  }
};
