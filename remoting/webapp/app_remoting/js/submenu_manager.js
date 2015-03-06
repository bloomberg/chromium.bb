// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Helper class for submenus that should add or remove the parent menu entry
 * depending on whether or not any child items exist.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {remoting.ContextMenuAdapter} adapter
 * @param {string} title The title of the parent menu item.
 * @param {boolean} checkable True if the child menus should be checkable.
 * @constructor
 */
remoting.SubmenuManager = function(adapter, title, checkable) {
  /** @private {remoting.ContextMenuAdapter} */
  this.adapter_ = adapter;
  /** @private {string} */
  this.title_ = title;
  /** @private {boolean} */
  this.checkable_ = checkable;
  /** @private {!Object} */
  this.childIds_ = {};
  /** @private {string} */
  this.parentId_ = '';
};

/**
 * Add a submenu item, or update the title of an existing submenu item.
 *
 * @param {string} id The child id.
 * @param {string} title The window title.
 * @return {boolean} True if a new menu item was created, false if an existing
 *     menu item was renamed.
 */
remoting.SubmenuManager.prototype.add = function(id, title) {
  if (id in this.childIds_) {
    this.adapter_.updateTitle(id, title);
    return false;
  } else {
    this.childIds_[id] = true;
    this.addOrRemoveParent_();
    this.adapter_.create(id, title, this.checkable_, this.parentId_);
    return true;
  }
};

/**
 * Remove a submenu item.
 *
 * @param {string} id The child id.
 */
remoting.SubmenuManager.prototype.remove = function(id) {
  this.adapter_.remove(id);
  delete this.childIds_[id];
  this.addOrRemoveParent_();
};

/**
 * Remove all submenu items.
 */
remoting.SubmenuManager.prototype.removeAll = function() {
  var submenus = Object.keys(this.childIds_);
  for (var i = 0; i < submenus.length; ++i) {
    this.remove(submenus[i]);
  }
};

/**
 * Add the parent menu item if it doesn't exist but there are submenus items,
 * or remove it if it exists but there are no submenus.
 *
 * @private
 */
remoting.SubmenuManager.prototype.addOrRemoveParent_ = function() {
  if (Object.getOwnPropertyNames(this.childIds_).length != 0) {
    if (!this.parentId_) {
      this.parentId_ = base.generateXsrfToken();  // Use a random id
      this.adapter_.create(this.parentId_, this.title_, false);
    }
  } else {
    if (this.parentId_) {
      this.adapter_.remove(this.parentId_);
      this.parentId_ = '';
    }
  }
};
