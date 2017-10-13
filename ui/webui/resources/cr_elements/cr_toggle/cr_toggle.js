// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-toggle' is a component for showing an on/off switch. It
 * fires a 'change' event *only* when its state changes as a result of a user
 * interaction. Besides just tapping the element, its state can be changed by
 * dragging (pointerdown+pointermove) the element towards the desired direction.
 */
Polymer({
  is: 'cr-toggle',

  behaviors: [Polymer.PaperRippleBehavior],

  properties: {
    checked: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'checkedChanged_',
    },

    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'disabledChanged_',
    },

    tabIndex: {
      type: Number,
      value: 0,
    },
  },

  hostAttributes: {
    'aria-disabled': 'false',
    'aria-pressed': 'false',
    role: 'button',
  },

  listeners: {
    'pointerdown': 'onPointerDown_',
    'pointerup': 'onPointerUp_',
    'tap': 'onTap_',
    'keypress': 'onKeyPress_',
    'focus': 'onFocus_',
    'blur': 'onBlur_',
  },

  /** @private {?Function} */
  boundPointerMove_: null,

  /**
   * Number of pixels required to move to consider the pointermove event as
   * intentional.
   * @type {number}
   */
  MOVE_THRESHOLD_PX: 5,

  /**
   * Whether the state of the toggle has already taken into account by
   * |pointeremove| handlers. Used in the 'tap' handler.
   * @private {boolean}
   */
  handledInPointerMove_: false,

  /** @override */
  attached: function() {
    let direction = getComputedStyle(this).direction == 'rtl' ? -1 : 1;

    this.boundPointerMove_ = (e) => {
      // Prevent unwanted text selection to occur while moving the pointer, this
      // is important.
      e.preventDefault();

      let diff = e.clientX - this.pointerDownX_;
      if (Math.abs(diff) < this.MOVE_THRESHOLD_PX)
        return;

      this.handledInPointerMove_ = true;

      let shouldToggle = (diff * direction < 0 && this.checked) ||
          (diff * direction > 0 && !this.checked);
      if (shouldToggle)
        this.toggleState_(false);
    };
  },

  /** @private */
  checkedChanged_: function() {
    this.setAttribute('aria-pressed', this.checked ? 'true' : 'false');
  },

  /** @private */
  disabledChanged_: function() {
    this.tabIndex = this.disabled ? -1 : 0;
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
  },

  /** @private */
  onFocus_: function() {
    this.ensureRipple();
    this.$$('paper-ripple').holdDown =
        cr.ui.FocusOutlineManager.forDocument(document).visible;
  },

  /** @private */
  onBlur_: function() {
    this.ensureRipple();
    this.$$('paper-ripple').holdDown = false;
  },

  /** @private */
  onPointerUp_: function() {
    this.removeEventListener('pointermove', this.boundPointerMove_);
  },

  /**
   * @param {!PointerEvent} e
   * @private
   */
  onPointerDown_: function(e) {
    // Don't do anything if this was not a primary button click or touch event.
    if (e.button != 0)
      return;

    // This is necessary to have follow up events fire on |this|, even
    // if they occur outside of its bounds.
    this.setPointerCapture(e.pointerId);
    this.pointerDownX_ = e.clientX;
    this.handledInPointerMove_ = false;
    this.addEventListener('pointermove', this.boundPointerMove_);
  },

  /** @private */
  onTap_: function(e) {
    // User gesture has already been taken care of inside |pointermove|
    // handlers, Do nothing here.
    if (this.handledInPointerMove_)
      return;

    // If no pointermove event fired, then user just tapped on the
    // toggle button and therefore it should be toggled.
    this.toggleState_(false);
  },

  /**
   * @param {boolean} fromKeyboard
   * @private
   */
  toggleState_: function(fromKeyboard) {
    this.checked = !this.checked;

    if (!fromKeyboard) {
      this.ensureRipple();
      this.$$('paper-ripple').holdDown = false;
    }

    this.fire('change', this.checked);
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyPress_: function(e) {
    if (e.code == 'Space' || e.code == 'Enter')
      this.toggleState_(true);
  },

  // customize the element's ripple
  _createRipple: function() {
    this._rippleContainer = this.$.button;
    let ripple = Polymer.PaperRippleBehavior._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle', 'toggle-ink');
    return ripple;
  },
});
