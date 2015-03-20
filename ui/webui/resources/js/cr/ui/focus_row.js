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
   *   var focusRow = new cr.ui.FocusRow(rowBoundary, rowEl);
   *
   *   focusRow.addFocusableElement(checkboxEl);
   *   focusRow.addFocusableElement(labelEl);
   *   focusRow.addFocusableElement(buttonEl);
   *
   *   focusRow.setInitialFocusability(true);
   *
   * Pressing left cycles backward and pressing right cycles forward in item
   * order. Pressing Home goes to the beginning of the list and End goes to the
   * end of the list.
   *
   * If an item in this row is focused, it'll stay active (accessible via tab).
   * If no items in this row are focused, the row can stay active until focus
   * changes to a node inside |this.boundary_|. If |boundary| isn't specified,
   * any focus change deactivates the row.
   *
   * @constructor
   * @extends {HTMLDivElement}
   */
  function FocusRow() {}

  /** @interface */
  FocusRow.Delegate = function() {};

  FocusRow.Delegate.prototype = {
    /**
     * Called when a key is pressed while an item in |this.focusableElements| is
     * focused. If |e|'s default is prevented, further processing is skipped.
     * @param {cr.ui.FocusRow} row The row that detected a keydown.
     * @param {Event} e
     * @return {boolean} Whether the event was handled.
     */
    onKeydown: assertNotReached,

    /**
     * @param {cr.ui.FocusRow} row The row that detected the mouse going down.
     * @param {Event} e
     * @return {boolean} Whether the event was handled.
     */
    onMousedown: assertNotReached,
  };

  /** @const {string} */
  FocusRow.ACTIVE_CLASS = 'focus-row-active';

  FocusRow.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Should be called in the constructor to decorate |this|.
     * @param {Node} boundary Focus events are ignored outside of this node.
     * @param {cr.ui.FocusRow.Delegate=} opt_delegate A delegate to handle key
     *     events.
     */
    decorate: function(boundary, opt_delegate) {
      /** @private {!Node} */
      this.boundary_ = boundary || document;

      /** @type {cr.ui.FocusRow.Delegate|undefined} */
      this.delegate = opt_delegate;

      /** @type {Array<Element>} */
      this.focusableElements = [];

      /** @private {!EventTracker} */
      this.eventTracker_ = new EventTracker;
      this.eventTracker_.add(cr.doc, 'focusin', this.onFocusin_.bind(this));
      this.eventTracker_.add(cr.doc, 'keydown', this.onKeydown_.bind(this));
    },

    /**
     * Called when the row's active state changes and it is added/removed from
     * the focus order.
     * @param {boolean} state Whether the row has become active or inactive.
     */
    onActiveStateChanged: function(state) {},

    /**
     * Find the element that best matches |sampleElement|.
     * @param {Element} sampleElement An element from a row of the same type
     *     which previously held focus.
     * @return {!Element} The element that best matches sampleElement.
     */
    getEquivalentElement: function(sampleElement) { assertNotReached(); },

    /**
     * Add an element to this FocusRow. No-op if |element| is not provided.
     * @param {Element} element The element that should be added.
     */
    addFocusableElement: function(element) {
      if (!element)
        return;

      assert(this.focusableElements.indexOf(element) == -1);
      assert(this.contains(element));

      element.tabIndex = this.isActive() ? 0 : -1;

      this.focusableElements.push(element);
      this.eventTracker_.add(element, 'mousedown',
                             this.onMousedown_.bind(this));
    },

    /**
     * Called when focus changes to activate/deactivate the row. Focus is
     * removed from the row when |element| is not in the FocusRow.
     * @param {Element} element The element that has focus. null if focus should
     *     be removed.
     * @private
    */
    onFocusChange_: function(element) {
      this.makeActive(this.contains(element));
    },

    /** @return {boolean} Whether this row is currently active. */
    isActive: function() {
      return this.classList.contains(FocusRow.ACTIVE_CLASS);
    },

    /**
     * Enables/disables the tabIndex of the focusable elements in the FocusRow.
     * tabIndex can be set properly.
     * @param {boolean} active True if tab is allowed for this row.
     */
    makeActive: function(active) {
      if (active == this.isActive())
        return;

      this.focusableElements.forEach(function(element) {
        element.tabIndex = active ? 0 : -1;
      });

      this.classList.toggle(FocusRow.ACTIVE_CLASS, active);
      this.onActiveStateChanged(active);
    },

    /** Dereferences nodes and removes event handlers. */
    destroy: function() {
      this.focusableElements.length = 0;
      this.eventTracker_.removeAll();
    },

    /**
     * @param {Event} e The focusin event.
     * @private
     */
    onFocusin_: function(e) {
      var target = assertInstanceof(e.target, Element);
      if (this.boundary_.contains(target))
        this.onFocusChange_(target);
    },

    /**
     * Handles a keypress for an element in this FocusRow.
     * @param {Event} e The keydown event.
     * @private
     */
    onKeydown_: function(e) {
      var element = assertInstanceof(e.target, Element);
      var elementIndex = this.focusableElements.indexOf(element);
      if (elementIndex < 0)
        return;

      if (this.delegate && this.delegate.onKeydown(this, e))
        return;

      if (e.altKey || e.ctrlKey || e.metaKey || e.shiftKey)
        return;

      var index = -1;

      if (e.keyIdentifier == 'Left')
        index = elementIndex + (isRTL() ? 1 : -1);
      else if (e.keyIdentifier == 'Right')
        index = elementIndex + (isRTL() ? -1 : 1);
      else if (e.keyIdentifier == 'Home')
        index = 0;
      else if (e.keyIdentifier == 'End')
        index = this.focusableElements.length - 1;

      var elementToFocus = this.focusableElements[index];
      if (elementToFocus) {
        this.getEquivalentElement(elementToFocus).focus();
        e.preventDefault();
      }
    },

    /**
     * @param {Event} e A click event.
     * @private
     */
    onMousedown_: function(e) {
      if (this.delegate && this.delegate.onMousedown(this, e))
        return;

      // Only accept the left mouse click.
      if (!e.button) {
        // Focus this row if the target is one of the elements in this row.
        this.onFocusChange_(assertInstanceof(e.target, Element));
      }
    },
  };

  return {
    FocusRow: FocusRow,
  };
});
