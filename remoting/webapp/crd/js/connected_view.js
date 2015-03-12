// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Implements a basic UX control for a connected remoting session.
 */

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

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
 * @param {remoting.ClientPlugin} plugin
 * @param {HTMLElement} viewportElement
 * @param {HTMLElement} cursorElement
 *
 * @constructor
 * @implements {base.Disposable}
 */
remoting.ConnectedView = function(plugin, viewportElement, cursorElement) {
  /** @private */
  this.viewportElement_ = viewportElement;

  /** @private */
  this.plugin_ = plugin;

  /** private */
  this.cursor_ = new remoting.ConnectedView.Cursor(
      plugin, viewportElement, cursorElement);

  var pluginElement = plugin.element();

  /** private */
  this.disposables_ = new base.Disposables(
    this.cursor_,
    new base.DomEventHook(pluginElement, 'focus',
                          this.onPluginGotFocus_.bind(this), false),
    new base.DomEventHook(pluginElement, 'blur',
                          this.onPluginLostFocus_.bind(this), false),
    new base.DomEventHook(document, 'visibilitychange',
                          this.onVisibilityChanged_.bind(this), false)
  );

  // TODO(wez): Only allow mouse lock if the app has the pointerLock permission.
  // Enable automatic mouse-lock.
  if (remoting.enableMouseLock &&
      this.plugin_.hasFeature(remoting.ClientPlugin.Feature.ALLOW_MOUSE_LOCK)) {
    this.plugin_.allowMouseLock();
  }

  pluginElement.focus();
};

/**
 * @return {void} Nothing.
 */
remoting.ConnectedView.prototype.dispose = function() {
  base.dispose(this.disposables_);
  this.disposables_ = null;
  this.cursorRender_ = null;
  this.plugin_ = null;
};

/**
 * Called when the app window is hidden.
 * @return {void} Nothing.
 * @private
 */
remoting.ConnectedView.prototype.onVisibilityChanged_ = function() {
  var pause = document.hidden;
  this.plugin_.pauseVideo(pause);
  this.plugin_.pauseAudio(pause);
};

/**
 * Callback that the plugin invokes to indicate when the connection is
 * ready.
 *
 * @param {boolean} ready True if the connection is ready.
 */
remoting.ConnectedView.prototype.onConnectionReady = function(ready) {
  this.viewportElement_.classList.toggle('session-client-inactive', !ready);
};

/**
 * Callback function called when the plugin element gets focus.
 * @private
 */
remoting.ConnectedView.prototype.onPluginGotFocus_ = function() {
  remoting.clipboard.initiateToHost();
};

/**
 * Callback function called when the plugin element loses focus.
 * @private
 */
remoting.ConnectedView.prototype.onPluginLostFocus_ = function() {
  // Release all keys to prevent them becoming 'stuck down' on the host.
  this.plugin_.releaseAllKeys();

  // Focus should stay on the element, not (for example) the toolbar.
  // Due to crbug.com/246335, we can't restore the focus immediately,
  // otherwise the plugin gets confused about whether or not it has focus.
  window.setTimeout(
      this.plugin_.element().focus.bind(this.plugin_.element()), 0);
};

/**
 * @param {remoting.ClientPlugin} plugin
 * @param {HTMLElement} viewportElement
 * @param {HTMLElement} cursorElement
 *
 * @constructor
 * @implements {base.Disposable}
 */
remoting.ConnectedView.Cursor = function(
    plugin, viewportElement, cursorElement) {
  /** @private */
  this.plugin_ = plugin;
  /** @private */
  this.cursorElement_ = cursorElement;
  /** @private */
  this.eventHook_ = new base.DomEventHook(
      viewportElement, 'mousemove', this.onMouseMoved_.bind(this), true);

  this.plugin_.setMouseCursorHandler(this.onCursorChanged_.bind(this));
};

remoting.ConnectedView.Cursor.prototype.dispose = function() {
  base.dispose(this.eventHook_);
  this.eventHook_ = null;
  this.plugin_.setMouseCursorHandler(
      /** function(string, string, number) */ base.doNothing);
  this.plugin_ = null;
};

/**
 * @param {string} url
 * @param {number} hotspotX
 * @param {number} hotspotY
 * @private
 */
remoting.ConnectedView.Cursor.prototype.onCursorChanged_ = function(
    url, hotspotX, hotspotY) {
  this.cursorElement_.hidden = !url;
  if (url) {
    this.cursorElement_.style.marginLeft = '-' + hotspotX + 'px';
    this.cursorElement_.style.marginTop = '-' + hotspotY + 'px';
    this.cursorElement_.src = url;
  }
};

/**
 * @param {Event} event
 * @private
 */
remoting.ConnectedView.Cursor.prototype.onMouseMoved_ = function(event) {
  this.cursorElement_.style.top = event.offsetY + 'px';
  this.cursorElement_.style.left = event.offsetX + 'px';
};

})();
