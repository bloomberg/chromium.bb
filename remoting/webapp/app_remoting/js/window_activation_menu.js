// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class to update the application's context menu to include host-side windows
 * and to notify the host when one of these menu items is selected.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {remoting.ContextMenuAdapter} adapter
 * @constructor
 */
remoting.WindowActivationMenu = function(adapter) {
  /** @private {remoting.SubmenuManager} */
  this.submenuManager_ = new remoting.SubmenuManager(
      adapter,
      chrome.i18n.getMessage(/*i18n-content*/'WINDOWS_SUBMENU_TITLE'),
      false);

  adapter.addListener(this.onContextMenu_.bind(this));
};

/**
 * Add a window to the application's context menu, or update the title of an
 * existing window.
 *
 * @param {number} id The window id.
 * @param {string} title The window title.
 */
remoting.WindowActivationMenu.prototype.add = function(id, title) {
  this.submenuManager_.add(this.makeMenuId_(id), title);
  // TODO(jamiewalch): Once crbug.com/426283 is fixed, call drawAttention()
  // here if the window does not have focus.
};

/**
 * Remove a window from the application's context menu.
 *
 * @param {number} id The window id.
 */
remoting.WindowActivationMenu.prototype.remove = function(id) {
  this.submenuManager_.remove(this.makeMenuId_(id));
};

/**
 * Create a menu id from the given window id.
 *
 * @param {number} windowId
 * @return {string}
 * @private
 */
remoting.WindowActivationMenu.prototype.makeMenuId_ = function(windowId) {
  return 'window-' + windowId;
};

/**
 * Handle a click on the application's context menu.
 *
 * @param {OnClickData=} info
 * @private
 */
remoting.WindowActivationMenu.prototype.onContextMenu_ = function(info) {
  /** @type {Array<string>} */
  var components = info.menuItemId.split('-');
  if (components.length == 2 &&
      this.makeMenuId_(parseInt(components[1], 10)) == info.menuItemId) {
    remoting.clientSession.sendClientMessage(
        'activateWindow',
        JSON.stringify({ id: parseInt(components[1], 0) }));
    if (chrome.app.window.current().isMinimized()) {
      chrome.app.window.current().restore();
    }
  }
};
