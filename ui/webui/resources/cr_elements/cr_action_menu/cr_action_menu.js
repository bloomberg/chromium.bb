// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   top: number,
 *   left: number,
 *   width: (number| undefined),
 *   height: (number| undefined),
 *   anchorAlignmentX: (number| undefined),
 *   anchorAlignmentY: (number| undefined),
 *   minX: (number| undefined),
 *   minY: (number| undefined),
 *   maxX: (number| undefined),
 *   maxY: (number| undefined),
 * }}
 */
var ShowConfig;

/**
 * @enum {number}
 * @const
 */
var AnchorAlignment = {
  BEFORE_START: -2,
  AFTER_START: -1,
  CENTER: 0,
  BEFORE_END: 1,
  AFTER_END: 2,
};

(function() {
/**
 * Returns the point to start along the X or Y axis given a start and end
 * point to anchor to, the length of the target and the direction to anchor
 * in. If honoring the anchor would force the menu outside of min/max, this
 * will ignore the anchor position and try to keep the menu within min/max.
 * @private
 * @param {number} start
 * @param {number} end
 * @param {number} length
 * @param {AnchorAlignment} anchorAlignment
 * @param {number} min
 * @param {number} max
 * @return {number}
 */
function getStartPointWithAnchor(
    start, end, length, anchorAlignment, min, max) {
  var startPoint = 0;
  switch (anchorAlignment) {
    case AnchorAlignment.BEFORE_START:
      startPoint = -length;
      break;
    case AnchorAlignment.AFTER_START:
      startPoint = start;
      break;
    case AnchorAlignment.CENTER:
      startPoint = (start + end - length) / 2;
      break;
    case AnchorAlignment.BEFORE_END:
      startPoint = end - length;
      break;
    case AnchorAlignment.AFTER_END:
      startPoint = end;
      break;
  }

  if (startPoint + length > max)
    startPoint = end - length;
  if (startPoint < min)
    startPoint = start;
  return startPoint;
}


/**
 * @private
 * @return {!ShowConfig}
 */
function getDefaultShowConfig() {
  return {
    top: 0,
    left: 0,
    height: 0,
    width: 0,
    anchorAlignmentX: AnchorAlignment.AFTER_START,
    anchorAlignmentY: AnchorAlignment.AFTER_START,
    minX: 0,
    minY: 0,
    maxX: window.innerWidth,
    maxY: window.innerHeight,
  };
}

Polymer({
  is: 'cr-action-menu',
  extends: 'dialog',

  /**
   * The element which the action menu will be anchored to. Also the element
   * where focus will be returned after the menu is closed. Only populated if
   * menu is opened with showAt().
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
    var options = this.querySelectorAll('.dropdown-item');
    var numOptions = options.length;
    var focusedIndex =
        Array.prototype.indexOf.call(options, this.root.activeElement);

    // Handle case where nothing is focused and up is pressed.
    if (focusedIndex === -1 && step === -1)
      focusedIndex = 0;

    do {
      focusedIndex = (numOptions + focusedIndex + step) % numOptions;
      nextOption = options[focusedIndex];
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
    if (this.anchorElement_) {
      cr.ui.focusWithoutInk(assert(this.anchorElement_));
      this.anchorElement_ = null;
    }
  },

  /**
   * Shows the menu anchored to the given element.
   * @param {!Element} anchorElement
   */
  showAt: function(anchorElement) {
    this.anchorElement_ = anchorElement;
    this.anchorElement_.scrollIntoViewIfNeeded();
    var rect = this.anchorElement_.getBoundingClientRect();
    this.showAtPosition({
      top: rect.top,
      left: rect.left,
      height: rect.height,
      width: rect.width,
      // Default to anchoring towards the left.
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
    });
  },

  /**
   * Shows the menu anchored to the given box. The anchor alignment is
   * specified as an X and Y alignment which represents a point in the anchor
   * where the menu will align to, which can have the menu either before or
   * after the given point in each axis. Center alignment places the center of
   * the menu in line with the center of the anchor.
   *
   *            y-start
   *         _____________
   *         |           |
   *         |           |
   *         |   CENTER  |
   * x-start |     x     | x-end
   *         |           |
   *         |anchor box |
   *         |___________|
   *
   *             y-end
   *
   * For example, aligning the menu to the inside of the top-right edge of
   * the anchor, extending towards the bottom-left would use a alignment of
   * (BEFORE_END, AFTER_START), whereas centering the menu below the bottom
   * edge of the anchor would use (CENTER, AFTER_END).
   *
   * @param {!ShowConfig} config
   */
  showAtPosition: function(config) {
    var c = Object.assign(getDefaultShowConfig(), config);

    var top = c.top;
    var left = c.left;
    var bottom = top + c.height;
    var right = left + c.width;

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

    this.showModal();

    // Flip the X anchor in RTL.
    var rtl = getComputedStyle(this).direction == 'rtl';
    if (rtl)
      c.anchorAlignmentX *= -1;

    var menuLeft = getStartPointWithAnchor(
        left, right, this.offsetWidth, c.anchorAlignmentX, c.minX, c.maxX);

    if (rtl) {
      var menuRight = window.innerWidth - menuLeft - this.offsetWidth;
      this.style.right = menuRight + 'px';
    } else {
      this.style.left = menuLeft + 'px';
    }

    var menuTop = getStartPointWithAnchor(
        top, bottom, this.offsetHeight, c.anchorAlignmentY, c.minY, c.maxY);
    this.style.top = menuTop + 'px';
  },
});
})();
