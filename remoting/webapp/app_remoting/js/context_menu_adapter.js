// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Wrapper interface for chrome.contextMenus.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @interface
 * @extends {base.Disposable}
 */
remoting.ContextMenuAdapter = function() {
};

/**
 * @param {string} id An identifier for the menu entry.
 * @param {string} title The text to display in the menu.
 * @param {boolean} isCheckable True if the state of this menu entry should
 *     have a check-box and manage its toggle state automatically. Note that
 *     checkable menu entries always start off unchecked; use updateCheckState
 *     to programmatically change the state.
 * @param {string=} opt_parentId The id of the parent menu item for submenus.
 */
remoting.ContextMenuAdapter.prototype.create = function(
    id, title, isCheckable, opt_parentId) {
};

/**
 * @param {string} id
 * @param {string} title
 */
remoting.ContextMenuAdapter.prototype.updateTitle = function(id, title) {
};

/**
 * @param {string} id
 * @param {boolean} checked
 */
remoting.ContextMenuAdapter.prototype.updateCheckState = function(id, checked) {
};

/**
 * @param {string} id
 */
remoting.ContextMenuAdapter.prototype.remove = function(id) {
};

/**
 * @param {function(OnClickData=):void} listener
 */
remoting.ContextMenuAdapter.prototype.addListener = function(listener) {
};
