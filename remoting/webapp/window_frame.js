// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Apps v2 custom title bar implementation
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {HTMLElement} titleBar The root node of the title-bar DOM hierarchy.
 * @constructor
 */
remoting.WindowFrame = function(titleBar) {
  /**
   * @type {boolean}
   * @private
   */
  this.isConnected_ = false;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.titleBar_ = titleBar;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.hoverTarget_ = /** @type {HTMLElement} */
      (titleBar.querySelector('.window-controls-hover-target'));
  base.debug.assert(this.hoverTarget_ != null);

  /**
   * @type {HTMLElement}
   * @private
   */
  this.maximizeRestoreControl_ = /** @type {HTMLElement} */
      (titleBar.querySelector('.window-maximize-restore'));
  base.debug.assert(this.maximizeRestoreControl_ != null);

  /**
   * @type {Array.<{cls:string, fn: function()}>}
   */
  var handlers = [
    { cls: 'window-disconnect', fn: this.disconnectSession_.bind(this) },
    { cls: 'window-maximize-restore',
      fn: this.maximizeOrRestoreWindow_.bind(this) },
    { cls: 'window-minimize', fn: this.minimizeWindow_.bind(this) },
    { cls: 'window-close', fn: window.close.bind(window) },
    { cls: 'window-controls-stub', fn: this.toggleWindowControls_.bind(this) }
  ];
  for (var i = 0; i < handlers.length; ++i) {
    var element = titleBar.querySelector('.' + handlers[i].cls);
    base.debug.assert(element != null);
    element.addEventListener('click', handlers[i].fn, false);
  }

  // Ensure that tool-tips are always correct.
  this.updateMaximizeOrRestoreIconTitle_();
  chrome.app.window.current().onMaximized.addListener(
      this.updateMaximizeOrRestoreIconTitle_.bind(this));
  chrome.app.window.current().onRestored.addListener(
      this.updateMaximizeOrRestoreIconTitle_.bind(this));
  chrome.app.window.current().onFullscreened.addListener(
      this.updateMaximizeOrRestoreIconTitle_.bind(this));
};

/**
 * @param {boolean} isConnected True if there is a connection active.
 */
remoting.WindowFrame.prototype.setConnected = function(isConnected) {
  this.isConnected_ = isConnected;
  if (this.isConnected_) {
    document.body.classList.add('connected');
  } else {
    document.body.classList.remove('connected');
  }
  this.updateMaximizeOrRestoreIconTitle_();
};

/**
 * @return {{width: number, height: number}} The size of the window, ignoring
 *     the title-bar and window borders, if visible.
 */
remoting.WindowFrame.prototype.getClientArea = function() {
  if (chrome.app.window.current().isFullscreen()) {
    return { 'height': window.innerHeight, 'width': window.innerWidth };
  } else {
    var kBorderWidth = 1;
    var titleHeight = this.titleBar_.clientHeight;
    return {
      'height': window.innerHeight - titleHeight - 2 * kBorderWidth,
      'width': window.innerWidth - 2 * kBorderWidth
    };
  }
};

/**
 * @private
 */
remoting.WindowFrame.prototype.disconnectSession_ = function() {
  // When the user disconnects, exit full-screen mode. This should not be
  // necessary, as we do the same thing in client_session.js when the plugin
  // is removed. However, there seems to be a bug in chrome.AppWindow.restore
  // that causes it to get stuck in full-screen mode without this.
  if (chrome.app.window.current().isFullscreen()) {
    chrome.app.window.current().restore();
  }
  remoting.disconnect();
};

/**
 * @private
 */
remoting.WindowFrame.prototype.maximizeOrRestoreWindow_ = function() {
  /** @type {boolean} */
  var restore =
      chrome.app.window.current().isFullscreen() ||
      chrome.app.window.current().isMaximized();
  if (restore) {
    // Restore twice: once to exit full-screen and once to exit maximized.
    // If the app is not full-screen, or went full-screen without first
    // being maximized, then the second restore has no effect.
    chrome.app.window.current().restore();
  } else if (this.isConnected_) {
    chrome.app.window.current().fullscreen();
  } else {
    chrome.app.window.current().maximize();
  }
};

/**
 * @private
 */
remoting.WindowFrame.prototype.minimizeWindow_ = function() {
  chrome.app.window.current().minimize();
};

/**
 * @private
 */
remoting.WindowFrame.prototype.toggleWindowControls_ = function() {
  this.hoverTarget_.classList.toggle('opened');
};

/**
 * Update the tool-top for the maximize/full-screen/restore icon to reflect
 * its current behaviour.
 *
 * @private
 */
remoting.WindowFrame.prototype.updateMaximizeOrRestoreIconTitle_ = function() {
  /** @type {string} */
  var tag = '';
  if (chrome.app.window.current().isFullscreen()) {
    tag = /*i18n-content*/'EXIT_FULL_SCREEN';
  } else if (chrome.app.window.current().isMaximized()) {
    tag = /*i18n-content*/'RESTORE_WINDOW';
  } else if (this.isConnected_) {
    tag = /*i18n-content*/'FULL_SCREEN';
  } else {
    tag = /*i18n-content*/'MAXIMIZE_WINDOW';
  }
  this.maximizeRestoreControl_.title = l10n.getTranslationOrError(tag);
};

/** @type {remoting.WindowFrame} */
remoting.windowFrame = null;
