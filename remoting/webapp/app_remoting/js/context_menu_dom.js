// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provide an alternative location for the application's context menu items
 * on platforms that don't provide it.
 *
 * To mimic the behaviour of an OS-provided context menu, the menu is dismissed
 * in three situations:
 *
 * 1. When the window loses focus (i.e, the user has clicked on another window
 *    or on the desktop).
 * 2. When the user selects an option from the menu.
 * 3. When the user clicks on another part of the same window; this is achieved
 *    using an invisible screen element behind the menu, but in front of all
 *    other DOM.
 *
 * TODO(jamiewalch): Fold this functionality into remoting.MenuButton.
 */
'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @implements {remoting.WindowShape.ClientUI}
 * @implements {remoting.ContextMenuAdapter}
 * @param {HTMLElement} root The root of the context menu DOM.
 * @param {remoting.WindowShape} windowShape
 */
remoting.ContextMenuDom = function(root, windowShape) {
  /** @private {HTMLElement} */
  this.root_ = root;
  /** @private {HTMLElement} */
  this.stub_ = /** @type {HTMLElement} */
      (this.root_.querySelector('.context-menu-stub'));
  /** @private {HTMLElement} */
  this.icon_ = /** @type {HTMLElement} */
      (this.root_.querySelector('.context-menu-icon'));
  /** @private {HTMLElement} */
  this.screen_ = /** @type {HTMLElement} */
      (this.root_.querySelector('.context-menu-screen'));
  /** @private {HTMLElement} */
  this.menu_ = /** @type {HTMLElement} */ (this.root_.querySelector('ul'));
  /** @private {number} */
  this.bottom_ = 8;
  /** @private {base.EventSourceImpl} */
  this.eventSource_ = new base.EventSourceImpl();
  /** @private {string} */
  this.eventName_ = '_click';
  /**
   * Since the same element is used to lock the icon open and to drag it, we
   * must keep track of drag events so that the corresponding click event can
   * be ignored.
   *
   * @private {boolean}
   */
  this.stubDragged_ = false;

  /** @private */
  this.windowShape_ = windowShape;

  /**
   * @private
   */
  this.dragAndDrop_ = new remoting.DragAndDrop(
      this.stub_, this.onDragUpdate_.bind(this));

  this.eventSource_.defineEvents([this.eventName_]);
  this.root_.addEventListener(
      'transitionend', this.onTransitionEnd_.bind(this), false);
  this.stub_.addEventListener('click', this.onStubClick_.bind(this), false);
  this.icon_.addEventListener('click', this.onIconClick_.bind(this), false);
  this.screen_.addEventListener('click', this.onIconClick_.bind(this), false);

  this.root_.hidden = false;
  this.root_.style.bottom = this.bottom_ + 'px';
  this.windowShape_.registerClientUI(this);
};

remoting.ContextMenuDom.prototype.dispose = function() {
  this.windowShape_.unregisterClientUI(this);
};

/**
 * @param {Array<{left: number, top: number, width: number, height: number}>}
 *     rects List of rectangles.
 */
remoting.ContextMenuDom.prototype.addToRegion = function(rects) {
  var rect = /** @type {ClientRect} */ (this.root_.getBoundingClientRect());
  // Clip the menu position to the main window in case the screen size has
  // changed or a recent drag event tried to move it out of bounds.
  if (rect.top < 0) {
    this.bottom_ += rect.top;
    this.root_.style.bottom = this.bottom_ + 'px';
    rect = this.root_.getBoundingClientRect();
  }

  rects.push(rect);
  if (this.root_.classList.contains('menu-opened')) {
    var menuRect = this.menu_.getBoundingClientRect();
    rects.push(menuRect);
  }
};

/**
 * @param {string} id An identifier for the menu entry.
 * @param {string} title The text to display in the menu.
 * @param {boolean} isCheckable True if the state of this menu entry should
 *     have a check-box and manage its toggle state automatically. Note that
 *     checkable menu entries always start off unchecked.
 * @param {string=} opt_parentId The id of the parent menu item for submenus.
 */
remoting.ContextMenuDom.prototype.create = function(
    id, title, isCheckable, opt_parentId) {
  var menuEntry = /** @type {HTMLElement} */ (document.createElement('li'));
  menuEntry.innerText = title;
  menuEntry.setAttribute('data-id', id);
  if (isCheckable) {
    menuEntry.setAttribute('data-checkable', true);
  }
  menuEntry.addEventListener('click', this.onClick_.bind(this), false);
  /** @type {Node} */
  var insertBefore = null;
  if (opt_parentId) {
    var parent = /** @type {HTMLElement} */
        (this.menu_.querySelector('[data-id="' + opt_parentId + '"]'));
    console.assert(
        parent != null,
        'No parent match for [data-id="' + /** @type {string} */(opt_parentId) +
        '"] in create().');
    console.assert(!parent.classList.contains('menu-group-item'),
                   'Nested sub-menus are not supported.');
    parent.classList.add('menu-group-header');
    menuEntry.classList.add('menu-group-item');
    insertBefore = this.getInsertionPointForParent(
        /** @type {string} */(opt_parentId));
  }
  this.menu_.insertBefore(menuEntry, insertBefore);
};

/**
 * @param {string} id
 * @param {string} title
 */
remoting.ContextMenuDom.prototype.updateTitle = function(id, title) {
  var node = this.menu_.querySelector('[data-id="' + id + '"]');
  if (node) {
    node.innerText = title;
  }
};

/**
 * @param {string} id
 * @param {boolean} checked
 */
remoting.ContextMenuDom.prototype.updateCheckState = function(id, checked) {
  var node = /** @type {HTMLElement} */
      (this.menu_.querySelector('[data-id="' + id + '"]'));
  if (node) {
    if (checked) {
      node.classList.add('selected');
    } else {
      node.classList.remove('selected');
    }
  }
};

/**
 * @param {string} id
 */
remoting.ContextMenuDom.prototype.remove = function(id) {
  var node = this.menu_.querySelector('[data-id="' + id + '"]');
  if (node) {
    this.menu_.removeChild(node);
  }
};

/**
 * @param {function(OnClickData=):void} listener
 */
remoting.ContextMenuDom.prototype.addListener = function(listener) {
  this.eventSource_.addEventListener(this.eventName_, listener);
};

/**
 * @param {Event} event
 * @private
 */
remoting.ContextMenuDom.prototype.onClick_ = function(event) {
  var element = /** @type {HTMLElement} */ (event.target);
  if (element.getAttribute('data-checkable')) {
    element.classList.toggle('selected')
  }
  var clickData = {
    menuItemId: element.getAttribute('data-id'),
    checked: element.classList.contains('selected')
  };
  this.eventSource_.raiseEvent(this.eventName_, clickData);
  this.onIconClick_();
};

/**
 * Get the insertion point for the specified sub-menu. This is the menu item
 * immediately following the last child of that menu group, or null if there
 * are no menu items after that group.
 *
 * @param {string} parentId
 * @return {Node?}
 */
remoting.ContextMenuDom.prototype.getInsertionPointForParent = function(
    parentId) {
  var parentNode = this.menu_.querySelector('[data-id="' + parentId + '"]');
  console.assert(parentNode != null,
                 'No parent match for [data-id="' + parentId +
                 '"] in getInsertionPointForParent().');
  var childNode = /** @type {HTMLElement} */ (parentNode.nextSibling);
  while (childNode != null && childNode.classList.contains('menu-group-item')) {
    childNode = childNode.nextSibling;
  }
  return childNode;
};

/**
 * Called when the CSS show/hide transition completes. Since this changes the
 * visible dimensions of the context menu, the visible region of the window
 * needs to be recomputed.
 *
 * @private
 */
remoting.ContextMenuDom.prototype.onTransitionEnd_ = function() {
  this.windowShape_.updateClientWindowShape();
};

/**
 * Toggle the visibility of the context menu icon.
 *
 * @private
 */
remoting.ContextMenuDom.prototype.onStubClick_ = function() {
  if (this.stubDragged_) {
    this.stubDragged_ = false;
    return;
  }
  this.root_.classList.toggle('opened');
};

/**
 * Toggle the visibility of the context menu.
 *
 * @private
 */
remoting.ContextMenuDom.prototype.onIconClick_ = function() {
  this.showMenu_(!this.menu_.classList.contains('opened'));
};

/**
 * Explicitly show or hide the context menu.
 *
 * @param {boolean} show True to show the menu; false to hide it.
 * @private
 */
remoting.ContextMenuDom.prototype.showMenu_ = function(show) {
  if (show) {
    // Ensure that the menu doesn't extend off the top or bottom of the
    // screen by aligning it to the top or bottom of the icon, depending
    // on the latter's vertical position.
    var menuRect =
        /** @type {ClientRect} */ (this.menu_.getBoundingClientRect());
    if (menuRect.bottom > window.innerHeight) {
      this.menu_.classList.add('menu-align-bottom');
    } else {
      this.menu_.classList.remove('menu-align-bottom');
    }

    /** @type {remoting.ContextMenuDom} */
    var that = this;
    var onBlur = function() {
      that.showMenu_(false);
      window.removeEventListener('blur', onBlur, false);
    };
    window.addEventListener('blur', onBlur, false);

    // Show the menu and prevent the icon from auto-hiding on mouse-out.
    this.menu_.classList.add('opened');
    this.root_.classList.add('menu-opened');

  } else {  // if (!show)
    this.menu_.classList.remove('opened');
    this.root_.classList.remove('menu-opened');
  }

  this.screen_.hidden = !show;
  this.windowShape_.updateClientWindowShape();
};

/**
 * @param {number} deltaX
 * @param {number} deltaY
 * @private
 */
remoting.ContextMenuDom.prototype.onDragUpdate_ = function(deltaX, deltaY) {
  this.stubDragged_ = true;
  this.bottom_ -= deltaY;
  this.root_.style.bottom = this.bottom_ + 'px';
  // Deferring the window shape update until the DOM update has completed
  // helps keep the position of the context menu consistent with the window
  // shape (though it's still not perfect).
  window.requestAnimationFrame(
      this.windowShape_.updateClientWindowShape.bind(this.windowShape_));
};
