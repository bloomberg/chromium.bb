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
   * @type {remoting.ClientSession}
   * @private
   */
  this.clientSession_ = null;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.titleBar_ = titleBar;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.title_ = /** @type {HTMLElement} */
      (titleBar.querySelector('.window-title'));
  base.debug.assert(this.title_ != null);

  /**
   * @type {HTMLElement}
   * @private
   */
  this.maximizeRestoreControl_ = /** @type {HTMLElement} */
      (titleBar.querySelector('.window-maximize-restore'));
  base.debug.assert(this.maximizeRestoreControl_ != null);

  var optionsButton = titleBar.querySelector('.window-options');
  base.debug.assert(optionsButton != null);
  this.optionMenuButton_ = new remoting.MenuButton(
      optionsButton,
      this.onShowOptionsMenu_.bind(this),
      this.onHideOptionsMenu_.bind(this));

  /**
   * @type {HTMLElement}
   * @private
   */
  this.optionsMenuList_ = /** @type {HTMLElement} */
      (optionsButton.querySelector('.window-options-menu'));
  base.debug.assert(this.optionsMenuList_ != null);

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
  this.handleWindowStateChange_();
  chrome.app.window.current().onMaximized.addListener(
      this.handleWindowStateChange_.bind(this));
  chrome.app.window.current().onRestored.addListener(
      this.handleWindowStateChange_.bind(this));
  chrome.app.window.current().onFullscreened.addListener(
      this.handleWindowStateChange_.bind(this));
  chrome.app.window.current().onFullscreened.addListener(
      this.showWindowControlsPreview_.bind(this));
};

/**
 * @return {remoting.OptionsMenu}
 */
remoting.WindowFrame.prototype.createOptionsMenu = function() {
  return new remoting.OptionsMenu(
      this.titleBar_.querySelector('.menu-send-ctrl-alt-del'),
      this.titleBar_.querySelector('.menu-send-print-screen'),
      this.titleBar_.querySelector('.menu-resize-to-client'),
      this.titleBar_.querySelector('.menu-shrink-to-fit'),
      this.titleBar_.querySelector('.menu-new-connection'),
      this.titleBar_.querySelector('.window-fullscreen'),
      this.titleBar_.querySelector('.menu-start-stop-recording'));
};

/**
 * @param {remoting.ClientSession} clientSession The client session, or null if
 *     there is no connection.
 */
remoting.WindowFrame.prototype.setClientSession = function(clientSession) {
  this.clientSession_ = clientSession;
  var windowTitle = document.head.querySelector('title');
  if (this.clientSession_) {
    this.title_.innerText = clientSession.getHostDisplayName();
    windowTitle.innerText = clientSession.getHostDisplayName() + ' - ' +
        chrome.i18n.getMessage(/*i18n-content*/'PRODUCT_NAME');
  } else {
    this.title_.innerHTML = '&nbsp;';
    windowTitle.innerText =
        chrome.i18n.getMessage(/*i18n-content*/'PRODUCT_NAME');
  }
  this.handleWindowStateChange_();
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
    chrome.app.window.current().restore();
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
  this.titleBar_.classList.toggle('opened');
};

/**
 * Update the tool-top for the maximize/full-screen/restore icon to reflect
 * its current behaviour.
 *
 * @private
 */
remoting.WindowFrame.prototype.handleWindowStateChange_ = function() {
  // Set the title for the maximize/restore/full-screen button
  /** @type {string} */
  var tag = '';
  if (chrome.app.window.current().isFullscreen()) {
    tag = /*i18n-content*/'EXIT_FULL_SCREEN';
  } else if (chrome.app.window.current().isMaximized()) {
    tag = /*i18n-content*/'RESTORE_WINDOW';
  } else {
    tag = /*i18n-content*/'MAXIMIZE_WINDOW';
  }
  this.maximizeRestoreControl_.title = l10n.getTranslationOrError(tag);

  // Ensure that the options menu aligns correctly for the side of the window
  // it occupies.
  if (chrome.app.window.current().isFullscreen()) {
    this.optionsMenuList_.classList.add('right-align');
  } else {
    this.optionsMenuList_.classList.remove('right-align');
  }
};

/**
 * Callback invoked when the options menu is shown.
 * @private
 */
remoting.WindowFrame.prototype.onShowOptionsMenu_ = function() {
  remoting.optionsMenu.onShow();
  this.titleBar_.classList.add('menu-opened');
};

/**
 * Callback invoked when the options menu is shown.
 * @private
 */
remoting.WindowFrame.prototype.onHideOptionsMenu_ = function() {
  this.titleBar_.classList.remove('menu-opened');
};

/**
 * Show the window controls for a few seconds
 *
 * @private
 */
remoting.WindowFrame.prototype.showWindowControlsPreview_ = function() {
  /**
   * @type {HTMLElement}
   */
  var target =  this.titleBar_;
  var kPreviewTimeoutMs = 3000;
  var hidePreview = function() {
    target.classList.remove('preview');
  };
  target.classList.add('preview');
  window.setTimeout(hidePreview, kPreviewTimeoutMs);
};


/** @type {remoting.WindowFrame} */
remoting.windowFrame = null;
