// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cr-action-menu',
  extends: 'dialog',

  /**
   * List of all options in this action menu.
   * @private {?NodeList<!Element>}
   */
  options_: null,

  /**
   * The element which the action menu will be anchored to. Also the element
   * where focus will be returned after the menu is closed.
   * @private {?Element}
   */
  anchorElement_: null,

  /**
   * Bound reference to an event listener function such that it can be removed
   * on detach.
   * @private {?Function}
   */
  boundClose_: null,

  /** @private {boolean} */
  hasMousemoveListener_: false,

  hostAttributes: {
    tabindex: 0,
  },

  listeners: {
    'keydown': 'onKeyDown_',
    'mouseover': 'onMouseover_',
    'tap': 'onTap_',
  },

  /** override */
  attached: function() {
    this.options_ = this.querySelectorAll('.dropdown-item');
  },

  /** override */
  detached: function() {
    this.removeListeners_();
  },

  /** @private */
  removeListeners_: function() {
    window.removeEventListener('resize', this.boundClose_);
    window.removeEventListener('popstate', this.boundClose_);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onTap_: function(e) {
    if (e.target == this) {
      this.close();
      e.stopPropagation();
    }
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_: function(e) {
    if (e.key == 'Tab' || e.key == 'Escape') {
      this.close();
      e.preventDefault();
      return;
    }

    if (e.key !== 'ArrowDown' && e.key !== 'ArrowUp')
      return;

    var nextOption = this.getNextOption_(e.key == 'ArrowDown' ? 1 : -1);
    if (nextOption) {
      if (!this.hasMousemoveListener_) {
        this.hasMousemoveListener_ = true;
        listenOnce(this, 'mousemove', function(e) {
          this.onMouseover_(e);
          this.hasMousemoveListener_ = false;
        }.bind(this));
      }
      nextOption.focus();
    }

    e.preventDefault();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onMouseover_: function(e) {
    // TODO(scottchen): Using "focus" to determine selected item might mess
    // with screen readers in some edge cases.
    var i = 0;
    do {
      var target = e.path[i++];
      if (target.classList && target.classList.contains('dropdown-item')) {
        target.focus();
        return;
      }
    } while (this != target);

    // The user moved the mouse off the options. Reset focus to the dialog.
    this.focus();
  },

  /**
   * @param {number} step -1 for getting previous option (up), 1 for getting
   *     next option (down).
   * @return {?Element} The next focusable option, taking into account
   *     disabled/hidden attributes, or null if no focusable option exists.
   * @private
   */
  getNextOption_: function(step) {
    // Using a counter to ensure no infinite loop occurs if all elements are
    // hidden/disabled.
    var counter = 0;
    var nextOption = null;
    var numOptions = this.options_.length;
    var focusedIndex =
        Array.prototype.indexOf.call(this.options_, this.root.activeElement);

    // Handle case where nothing is focused and up is pressed.
    if (focusedIndex === -1 && step === -1)
      focusedIndex = 0;

    do {
      focusedIndex = (numOptions + focusedIndex + step) % numOptions;
      nextOption = this.options_[focusedIndex];
      if (nextOption.disabled || nextOption.hidden)
        nextOption = null;
      counter++;
    } while (!nextOption && counter < numOptions);

    return nextOption;
  },

  /** @override */
  close: function() {
    // Removing 'resize' and 'popstate' listeners when dialog is closed.
    this.removeListeners_();
    HTMLDialogElement.prototype.close.call(this);
    cr.ui.focusWithoutInk(assert(this.anchorElement_));
    this.anchorElement_ = null;
  },

  /**
   * Shows the menu anchored to the given element.
   * @param {!Element} anchorElement
   */
  showAt: function(anchorElement) {
    this.anchorElement_ = anchorElement;
    this.boundClose_ = this.boundClose_ || function() {
      if (this.open)
        this.close();
    }.bind(this);
    window.addEventListener('resize', this.boundClose_);
    window.addEventListener('popstate', this.boundClose_);

    // Reset position to prevent previous values from affecting layout.
    this.style.left = '';
    this.style.right = '';
    this.style.top = '';

    this.anchorElement_.scrollIntoViewIfNeeded();
    this.showModal();

    var rect = this.anchorElement_.getBoundingClientRect();
    if (getComputedStyle(this.anchorElement_).direction == 'rtl') {
      var right = window.innerWidth - rect.left - this.offsetWidth;
      this.style.right = right + 'px';
    } else {
      var left = rect.right - this.offsetWidth;
      this.style.left = left + 'px';
    }

    // Attempt to show the menu starting from the top of the rectangle and
    // extending downwards. If that does not fit within the window, fallback to
    // starting from the bottom and extending upwards.
    var top = rect.top + this.offsetHeight <= window.innerHeight ? rect.top :
                                                                   rect.bottom -
            this.offsetHeight - Math.max(rect.bottom - window.innerHeight, 0);

    this.style.top = top + 'px';
  },
});
