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
  this.button_ = /** @type {HTMLElement} */ (container.querySelector('button'));

  /**
   * @type {undefined|function():void}
   * @private
   */
  this.onShow_ = opt_onShow;

  /** @type {remoting.MenuButton} */
  var that = this;

  // Create event handlers to show and hide the menu, attached to the button
  // and the document <html> tag, respectively. These handlers are added and
  // removed depending on the state of the menu. To prevent both triggering
  // for one click, they are added by a timer.
  /**
   * @type {function(Event):void}
   * @private
   */
  this.onClick_ = function(event) {
    if (that.onShow_) {
      that.onShow_();
    }
    that.button_.classList.add(remoting.MenuButton.BUTTON_ACTIVE_CLASS_);
    that.button_.removeEventListener('click', that.onClick_, false);
    window.setTimeout(
        function() {
          // Attach the click handler to the <html> node so that it includes
          // the document area outside the plugin, which is not covered by
          // the <body> node.
          var htmlNode = document.body.parentNode;
          htmlNode.addEventListener('click', that.closeHandler_, true);
        },
        100);
  };

  /**
   * @type {function(Event):void}
   * @private
   */
  this.closeHandler_ = function(event) {
    that.button_.classList.remove(remoting.MenuButton.BUTTON_ACTIVE_CLASS_);
    document.body.removeEventListener('click', that.closeHandler_, true);
    window.setTimeout(
        function() {
          that.button_.addEventListener('click', that.onClick_, false);
        },
        100);
  };

  this.button_.addEventListener('click', this.onClick_, false);
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
