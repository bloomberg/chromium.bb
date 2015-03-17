// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-input' is a single-line text field for user input. It is a convenience
 * element composed of a 'paper-input-decorator' and a 'input is="core-input"'.
 *
 * Example:
 *
 *    <cr-input></cr-input>
 *
 * @group Chrome Elements
 * @element cr-input
 */
Polymer('cr-input', {
  publish: {
    /**
     * The label for this input. It normally appears as grey text inside
     * the text input and disappears once the user enters text.
     *
     * @attribute label
     * @type string
     * @default ''
     */
    label: '',

    /**
     * If true, the label will "float" above the text input once the
     * user enters text instead of disappearing.
     *
     * @attribute floatingLabel
     * @type boolean
     * @default true
     */
    floatingLabel: true,

    /**
     * Set to true to style the element as disabled.
     *
     * @attribute disabled
     * @type boolean
     * @default false
     */
    disabled: {value: false, reflect: true},

    /**
     * Set to true to make the input read-only.
     *
     * @attribute readonly
     * @type boolean
     * @default false
     */
    readonly: {value: false, reflect: true},

    /**
     * Set to true to mark the input as required.
     *
     * @attribute required
     * @type boolean
     * @default false
     */
    required: {value: false, reflect: true},

    /**
     * The current value of the input.
     *
     * @attribute value
     * @type string
     * @default ''
     */
    value: '',

    /**
     * The validation pattern for the input.
     *
     * @attribute pattern
     * @type string
     * @default ''
     */
    pattern: '',

    /**
     * The type of the input (password, date, etc.).
     *
     * @attribute type
     * @type string
     * @default 'text'
     */
    type: 'text',

    /**
     * The message to display if the input value fails validation. If this
     * is unset or the empty string, a default message is displayed depending
     * on the type of validation error.
     *
     * @attribute error
     * @type string
     * @default ''
     */
    error: '',

    /**
     * The most recently committed value of the input.
     *
     * @attribute committedValue
     * @type string
     * @default ''
     */
    committedValue: ''
  },

  /**
   * Focuses the 'input' element.
   */
  focus: function() {
    this.$.input.focus();
  },

  valueChanged: function() {
    this.$.decorator.updateLabelVisibility(this.value);
  },

  patternChanged: function() {
    if (this.pattern)
      this.$.input.pattern = this.pattern;
    else
      this.$.input.removeAttribute('pattern');
  },

  /** @override */
  ready: function() {
    this.$.events.forward(this.$.input, ['change']);
    this.patternChanged();
  },
});
