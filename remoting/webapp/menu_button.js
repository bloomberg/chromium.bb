// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class representing a menu button and its associated menu items.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @param {Element} container The element containing the <button> and <ul>
 *     elements comprising the menu. It should have the "menu-button" class.
 * @param {function():void=} opt_onShow Optional callback invoked before the
 *     menu is shown.
 */
remoting.MenuButton = function(container, opt_onShow) {
  /**
   * @type {HTMLElement}
   * @private
   */
  this.button_ = /** @type {HTMLElement} */
      (container.querySelector('button,.menu-button-activator'));

  /**
   * @type {HTMLElement}
   * @private
   */
  this.menu_ = /** @type {HTMLElement} */ (container.querySelector('ul'));

  /**
   * @type {undefined|function():void}
   * @private
   */
  this.onShow_ = opt_onShow;

  /** @type {remoting.MenuButton} */
  var that = this;

  /**
   * @type {function(Event):void}
   * @private
   */
  var closeHandler = function(event) {
    that.button_.classList.remove(remoting.MenuButton.BUTTON_ACTIVE_CLASS_);
    document.body.removeEventListener('click', closeHandler, true);
  };

  /**
   * @type {function(Event):void}
   * @private
   */
  var onClick = function(event) {
    if (that.onShow_) {
      that.onShow_();
    }
    that.button_.classList.toggle(remoting.MenuButton.BUTTON_ACTIVE_CLASS_);
    document.body.addEventListener('click', closeHandler, false);
    event.stopPropagation();
  };

  this.button_.addEventListener('click', onClick, false);
};

/**
 * @return {HTMLElement} The button that activates the menu.
 */
remoting.MenuButton.prototype.button = function() {
  return this.button_;
};

/**
 * @return {HTMLElement} The menu.
 */
remoting.MenuButton.prototype.menu = function() {
  return this.menu_;
};

/**
 * Set or unset the selected state of an <li> menu item.
 * @param {HTMLElement} item The menu item to update.
 * @param {boolean} selected True to select the item, false to deselect it.
 * @return {void} Nothing.
 */
remoting.MenuButton.select = function(item, selected) {
  if (selected) {
    item.classList.add('selected');
  } else {
    item.classList.remove('selected');
  }
};

/** @const @private */
remoting.MenuButton.BUTTON_ACTIVE_CLASS_ = 'active';
