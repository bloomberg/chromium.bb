// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /**
   * A class to manage focus between given horizontally arranged elements.
   * For example, given the page:
   *
   *   <input type="checkbox"> <label>Check me!</label> <button>X</button>
   *
   * One could create a FocusRow by doing:
   *
   *   new cr.ui.FocusRow([checkboxEl, labelEl, buttonEl])
   *
   * if there are references to each node or querying them from the DOM like so:
   *
   *   new cr.ui.FocusRow(dialog.querySelectorAll('list input[type=checkbox]'))
   *
   * Pressing left cycles backward and pressing right cycles forward in item
   * order. Pressing Home goes to the beginning of the list and End goes to the
   * end of the list.
   *
   * If an item in this row is focused, it'll stay active (accessible via tab).
   * If no items in this row are focused, the row can stay active until focus
   * changes to a node inside |this.boundary_|. If opt_boundary isn't
   * specified, any focus change deactivates the row.
   *
   * @param {!Array.<!Element>|!NodeList} items Elements to track focus of.
   * @param {Node=} opt_boundary Focus events are ignored outside of this node.
   * @param {FocusRow.Delegate=} opt_delegate A delegate to handle key events.
   * @param {FocusRow.Observer=} opt_observer An observer that's notified if
   *     this focus row is added to or removed from the focus order.
   * @constructor
   */
  function FocusRow(items, opt_boundary, opt_delegate, opt_observer) {
    /** @type {!Array.<!Element>} */
    this.items = Array.prototype.slice.call(items);
    assert(this.items.length > 0);

    /** @type {!Node} */
    this.boundary_ = opt_boundary || document;

    /** @private {FocusRow.Delegate|undefined} */
    this.delegate_ = opt_delegate;

    /** @private {FocusRow.Observer|undefined} */
    this.observer_ = opt_observer;

    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker;
    this.eventTracker_.add(cr.doc, 'focusin', this.onFocusin_.bind(this));
    this.eventTracker_.add(cr.doc, 'keydown', this.onKeydown_.bind(this));

    this.items.forEach(function(item) {
      if (item != document.activeElement)
        item.tabIndex = -1;

      this.eventTracker_.add(item, 'click', this.onClick_.bind(this));
    }, this);

    /**
     * The index that should be actively participating in the page tab order.
     * @type {number}
     * @private
     */
    this.activeIndex_ = this.items.indexOf(document.activeElement);
  }

  /** @interface */
  FocusRow.Delegate = function() {};

  FocusRow.Delegate.prototype = {
    /**
     * Called when a key is pressed while an item in |this.items| is focused. If
     * |e|'s default is prevented, further processing is skipped.
     * @param {cr.ui.FocusRow} row The row that detected a keydown.
     * @param {Event} e The keydown event.
     */
    onKeydown: assertNotReached,
  };

  /** @interface */
  FocusRow.Observer = function() {};

  FocusRow.Observer.prototype = {
    /**
     * Called when the row is activated (added to the focus order).
     * @param {cr.ui.FocusRow} row The row added to the focus order.
     */
    onActivate: assertNotReached,

    /**
     * Called when the row is deactivated (removed from the focus order).
     * @param {cr.ui.FocusRow} row The row removed from the focus order.
     */
    onDeactivate: assertNotReached,
  };

  FocusRow.prototype = {
    get activeIndex() {
      return this.activeIndex_;
    },
    set activeIndex(index) {
      var wasActive = this.items[this.activeIndex_];
      if (wasActive)
        wasActive.tabIndex = -1;

      this.items.forEach(function(item) { assert(item.tabIndex == -1); });
      this.activeIndex_ = index;

      if (this.items[index])
        this.items[index].tabIndex = 0;

      if (!this.observer_)
        return;

      var isActive = index >= 0 && index < this.items.length;
      if (isActive == !!wasActive)
        return;

      if (isActive)
        this.observer_.onActivate(this);
      else
        this.observer_.onDeactivate(this);
    },

    /**
     * Focuses the item at |index|.
     * @param {number} index An index to focus. Must be between 0 and
     *     this.items.length - 1.
     */
    focusIndex: function(index) {
      this.items[index].focus();
    },

    /** Call this to clean up event handling before dereferencing. */
    destroy: function() {
      this.eventTracker_.removeAll();
    },

    /**
     * @param {Event} e The focusin event.
     * @private
     */
    onFocusin_: function(e) {
      if (this.boundary_.contains(e.target))
        this.activeIndex = this.items.indexOf(e.target);
    },

    /**
     * @param {Event} e A focus event.
     * @private
     */
    onKeydown_: function(e) {
      var item = this.items.indexOf(e.target);
      if (item < 0)
        return;

      if (this.delegate_)
        this.delegate_.onKeydown(this, e);

      if (e.defaultPrevented)
        return;

      var index = -1;

      if (e.keyIdentifier == 'Left')
        index = item + (isRTL() ? 1 : -1);
      else if (e.keyIdentifier == 'Right')
        index = item + (isRTL() ? -1 : 1);
      else if (e.keyIdentifier == 'Home')
        index = 0;
      else if (e.keyIdentifier == 'End')
        index = this.items.length - 1;

      if (!this.items[index])
        return;

      this.focusIndex(index);
      e.preventDefault();
    },

    /**
     * @param {Event} e A click event.
     * @private
     */
    onClick_: function(e) {
      if (!e.button)
        this.activeIndex = this.items.indexOf(e.currentTarget);
    },
  };

  return {
    FocusRow: FocusRow,
  };
});
