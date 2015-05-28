// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * remoting.ContextMenuAdapter implementation backed by chrome.contextMenus.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @implements {remoting.ContextMenuAdapter}
 */
remoting.ContextMenuChrome = function() {};

remoting.ContextMenuChrome.prototype.dispose = function() {
  chrome.contextMenus.removeAll();
};

/**
 * @param {string} id An identifier for the menu entry.
 * @param {string} title The text to display in the menu.
 * @param {boolean} isCheckable True if the state of this menu entry should
 *     have a check-box and manage its toggle state automatically.
 * @param {string=} opt_parentId The id of the parent menu item for submenus.
 */
remoting.ContextMenuChrome.prototype.create = function(
    id, title, isCheckable, opt_parentId) {
  if (!opt_parentId) {
    var message = {
      method: 'addContextMenuId',
      id: id
    };
    chrome.runtime.getBackgroundPage(this.postMessage_.bind(this, message));
  }
  var params = {
    id: id,
    contexts: ['launcher'],
    title: title,
    parentId: opt_parentId
  };
  if (isCheckable) {
    params.type = 'checkbox';
  }
  chrome.contextMenus.create(params);
};

/**
 * @param {string} id
 * @param {string} title
 */
remoting.ContextMenuChrome.prototype.updateTitle = function(id, title) {
  chrome.contextMenus.update(id, {title: title});
};

/**
 * @param {string} id
 * @param {boolean} checked
 */
remoting.ContextMenuChrome.prototype.updateCheckState = function(id, checked) {
  chrome.contextMenus.update(id, {checked: checked});
};

/**
 * @param {string} id
 */
remoting.ContextMenuChrome.prototype.remove = function(id) {
  chrome.contextMenus.remove(id);
  var message = {
    method: 'removeContextMenuId',
    id: id
  };
  chrome.runtime.getBackgroundPage(this.postMessage_.bind(this, message));
};

/**
 * @param {function(OnClickData):void} listener
 */
remoting.ContextMenuChrome.prototype.addListener = function(listener) {
  chrome.contextMenus.onClicked.addListener(
      /** @type {function(Object, Tab=)} */ (listener));
};

/**
 * @param {*} message
 * @param {Window=} backgroundPage
 */
remoting.ContextMenuChrome.prototype.postMessage_ = function(
    message, backgroundPage) {
  backgroundPage.postMessage(message, '*');
};