// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-input' is a component similar to native input.
 */
Polymer({
  is: 'cr-input',

  properties: {
    autofocus: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'disabledChanged_',
    },

    errorMessage: {
      type: String,
      value: '',
    },

    invalid: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    label: {
      type: String,
      value: '',
    },

    placeholder: {
      type: String,
      observer: 'placeholderChanged_',
    },

    tabIndex: String,

    value: {
      type: String,
      value: '',
      notify: true,
    },
  },

  hostAttributes: {
    'aria-disabled': 'false',
    role: 'textbox',
  },

  listeners: {
    'input.change': 'onInputChange_',
  },

  /** @private */
  disabledChanged_: function() {
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
    this.$.input.blur();  // In case input was focused when disabled changes.
  },

  /**
   * This is necessary instead of doing <input placeholder="[[placeholder]]">
   * because if this.placeholder is set to a truthy value then removed, it
   * would show "null" as placeholder.
   * @private
   */
  placeholderChanged_: function() {
    if (this.placeholder || this.placeholder == '')
      this.$.input.setAttribute('placeholder', this.placeholder);
    else
      this.$.input.removeAttribute('placeholder');
  },

  /**
   * 'change' event fires when <input> value changes and user presses 'Enter'.
   * This function helps propagate it to host since change events don't
   * propagate across Shadow DOM boundary by default.
   * @param {!Event} e
   * @private
   */
  onInputChange_: function(e) {
    this.fire('change', {sourceEvent: e});
  }
});