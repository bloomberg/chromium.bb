// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for cr-radio-button-like elements.
 */

/** @polymerBehavior */
var CrRadioButtonBehaviorImpl = {
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

    label: {
      type: String,
      value: '',  // Allows the hidden$= binding to run without being set.
    },
  },

  listeners: {
    'focus': 'onFocus_',
    'blur': 'cancelRipple_',
    'pointerup': 'cancelRipple_',
  },

  hostAttributes: {
    'aria-disabled': 'false',
    'aria-checked': 'false',
    role: 'radio',
    tabindex: 0,
  },

  keyBindings: {
    // This is mainly for screenreaders, which can perform actions on things
    // that aren't focused (only focused things get synthetic click/tap events).
    'enter:keyup': 'onSelectKeyUp_',
    'space:keyup': 'onSelectKeyUp_',
  },

  /** @private */
  checkedChanged_: function() {
    this.setAttribute('aria-checked', this.checked ? 'true' : 'false');
  },

  /**
   * @param {boolean} current
   * @param {boolean} previous
   * @private
   */
  disabledChanged_: function(current, previous) {
    if (previous === undefined && !this.disabled)
      return;

    this.setAttribute('tabindex', this.disabled ? -1 : 0);
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
  },

  /** @private */
  onFocus_: function() {
    this.ensureRipple();
    this.$$('paper-ripple').holdDown = true;
  },

  /**
   * @param {!{detail: {keyboardEvent: !Event}}} e
   * @private
   */
  onSelectKeyUp_: function(e) {
    // If ripple is disabled, or focus is on a link, don't propagate to the
    // parent paper-radio-group to avoid incorrect selection.
    if (this.disabled || e.detail.keyboardEvent.target.tagName == 'A')
      return;

    this.click();
  },

  /** @private */
  cancelRipple_: function() {
    this.ensureRipple();
    this.$$('paper-ripple').holdDown = false;
  },

  // customize the element's ripple
  _createRipple: function() {
    this._rippleContainer = this.$$('.disc-wrapper');
    let ripple = Polymer.PaperRippleBehavior._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle', 'toggle-ink');
    return ripple;
  },
};


/** @polymerBehavior */
const CrRadioButtonBehavior = [
  Polymer.IronA11yKeysBehavior,
  Polymer.PaperRippleBehavior,
  CrRadioButtonBehaviorImpl,
];