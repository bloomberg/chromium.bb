// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `cr-input` is a single-line text field for user input. It is a convenience
 * element wrapping `paper-input`.
 *
 * Example:
 *
 *    <cr-input></cr-input>
 *
 * @group Chrome Elements
 * @element cr-input
 */
Polymer({
  is: 'cr-input',

  properties: {
    /**
     * The label for this input. It normally appears as grey text inside
     * the text input and disappears once the user enters text.
     */
    label: {
      type: String,
      value: ''
    },

    /**
     * Set to true to mark the input as required.
     */
    required: {
      type: Boolean,
      value: false
    },

    /**
     * The current value of the input.
     */
    value: {
      type: String,
      value: '',
    },

    /**
     * The validation pattern for the input.
     */
    pattern: String,

    /**
     * The type of the input (password, date, etc.).
     */
    type: String,

    /**
     * The message to display if the input value fails validation. If this
     * is unset or the empty string, a default message is displayed depending
     * on the type of validation error.
     */
    errorMessage: {
      type: String,
      value: '',
    },
  },

  /**
   * Focuses the 'input' element.
   */
  focus: function() {
    this.$.input.focus();
  },

  /** @override */
  ready: function() {
    this.$.events.forward(this.$.input, ['change']);
  },
});
