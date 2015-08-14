// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /**
   * A class to manage focus between given horizontally arranged elements.
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
   * @param {!Element} root The root of this focus row. Focus classes are
   *     applied to |root| and all added elements must live within |root|.
   * @param {?Node} boundary Focus events are ignored outside of this node.
   * @param {cr.ui.FocusRow.Delegate=} opt_delegate An optional event delegate.
   * @constructor
   */
  function FocusRow(root, boundary, opt_delegate) {
    /** @type {!Element} */
    this.root = root;

    /** @private {!Node} */
    this.boundary_ = boundary || document;

    /** @type {cr.ui.FocusRow.Delegate|undefined} */
    this.delegate = opt_delegate;

    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker;
    this.eventTracker_.add(cr.doc, 'focusin', this.onFocusin_.bind(this));
    this.eventTracker_.add(cr.doc, 'keydown', this.onKeydown_.bind(this));
  }

  /** @interface */
  FocusRow.Delegate = function() {};

  FocusRow.Delegate.prototype = {
    /**
     * Called when a key is pressed while on a typed element. If |e|'s default
     * is prevented, further processing is skipped.
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

  /**
   * Whether it's possible that |element| can be focused.
   * @param {Element} element
   * @return {boolean} Whether the item is focusable.
   */
  FocusRow.isFocusable = function(element) {
    if (!element || element.disabled)
      return false;

    // We don't check that element.tabIndex >= 0 here because inactive rows set
    // a tabIndex of -1.

    function isVisible(element) {
      var style = window.getComputedStyle(element);
      if (style.visibility == 'hidden' || style.display == 'none')
        return false;

      if (element.parentNode == element.ownerDocument)
        return true;

      return isVisible(element.parentElement);
    }

    return isVisible(element);
  };

  FocusRow.prototype = {
    /**
     * Register a new type of focusable element (or add to an existing one).
     *
     * Example: an (X) button might be 'delete' or 'close'.
     *
     * When FocusRow is used within a FocusGrid, these types are used to
     * determine equivalent controls when Up/Down are pressed to change rows.
     *
     * Another example: mutually exclusive controls that hide eachother on
     * activation (i.e. Play/Pause) could use the same type (i.e. 'play-pause')
     * to indicate they're equivalent.
     *
     * @param {string} type The type of element to track focus of.
     * @param {string} query The selector of the element from this row's root.
     * @return {boolean} Whether a new item was added.
     */
    addItem: function(type, query) {
      assert(type);

      var element = this.root.querySelector(query);
      if (!element)
        return false;

      element.setAttribute('focus-type', type);
      element.tabIndex = this.isActive() ? 0 : -1;

      this.eventTracker_.add(element, 'mousedown',
                             this.onMousedown_.bind(this));
      return true;
    },

    /** Dereferences nodes and removes event handlers. */
    destroy: function() {
      this.eventTracker_.removeAll();
    },

    /**
     * @param {Element} sampleElement An element for to find an equivalent for.
     * @return {!Element} An equivalent element to focus for |sampleElement|.
     * @protected
     */
    getCustomEquivalent: function(sampleElement) {
      return assert(this.getFirstFocusable());
    },

    /**
     * @return {!Array<!Element>} All registered elements (regardless of
     *     focusability).
     */
    getElements: function() {
      var elements = this.root.querySelectorAll('[focus-type]');
      return Array.prototype.slice.call(elements);
    },

    /**
     * Find the element that best matches |sampleElement|.
     * @param {!Element} sampleElement An element from a row of the same type
     *     which previously held focus.
     * @return {!Element} The element that best matches sampleElement.
     */
    getEquivalentElement: function(sampleElement) {
      if (this.getFocusableElements().indexOf(sampleElement) >= 0)
        return sampleElement;

      var sampleFocusType = this.getTypeForElement(sampleElement);
      if (sampleFocusType) {
        var sameType = this.getFirstFocusable(sampleFocusType);
        if (sameType)
          return sameType;
      }

      return this.getCustomEquivalent(sampleElement);
    },

    /**
     * @param {string=} opt_type An optional type to search for.
     * @return {?Element} The first focusable element with |type|.
     */
    getFirstFocusable: function(opt_type) {
      var filter = opt_type ? '="' + opt_type + '"' : '';
      var elements = this.root.querySelectorAll('[focus-type' + filter + ']');
      for (var i = 0; i < elements.length; ++i) {
        if (cr.ui.FocusRow.isFocusable(elements[i]))
          return elements[i];
      }
      return null;
    },

    /** @return {!Array<!Element>} Registered, focusable elements. */
    getFocusableElements: function() {
      return this.getElements().filter(cr.ui.FocusRow.isFocusable);
    },

    /**
     * @param {!Element} element An element to determine a focus type for.
     * @return {string} The focus type for |element| or '' if none.
     */
    getTypeForElement: function(element) {
      return element.getAttribute('focus-type') || '';
    },

    /** @return {boolean} Whether this row is currently active. */
    isActive: function() {
      return this.root.classList.contains(FocusRow.ACTIVE_CLASS);
    },

    /**
     * Enables/disables the tabIndex of the focusable elements in the FocusRow.
     * tabIndex can be set properly.
     * @param {boolean} active True if tab is allowed for this row.
     */
    makeActive: function(active) {
      if (active == this.isActive())
        return;

      this.getElements().forEach(function(element) {
        element.tabIndex = active ? 0 : -1;
      });

      this.root.classList.toggle(FocusRow.ACTIVE_CLASS, active);
    },

    /**
     * Called when focus changes to activate/deactivate the row. Focus is
     * removed from the row when |element| is not in the FocusRow.
     * @param {Element} element The element that has focus. null if focus should
     *     be removed.
     * @private
    */
    onFocusChange_: function(element) {
      this.makeActive(this.root.contains(element));
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

      var elements = this.getFocusableElements();
      var elementIndex = elements.indexOf(element);
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
        index = elements.length - 1;

      var elementToFocus = elements[index];
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
