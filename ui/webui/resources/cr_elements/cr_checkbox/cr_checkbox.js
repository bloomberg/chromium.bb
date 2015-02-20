/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview
 * `cr-checkbox` is a button that can be either checked or unchecked. User
 * can tap the checkbox to check or uncheck it. Usually you use checkboxes
 * to allow user to select multiple options from a set. If you have a single
 * ON/OFF option, avoid using a single checkbox and use `cr-toggle-button`
 * instead.
 *
 * Example:
 *     <cr-checkbox></cr-checkbox>
 *     <cr-checkbox checked></cr-checkbox>
 *
 * @element cr-checkbox
 */
Polymer({
  publish: {
    /**
     * Gets or sets the state. `true` is checked and `false` is unchecked.
     *
     * @attribute checked
     * @type boolean
     * @default false
     */
    checked: {value: false, reflect: true},

    /**
     * The label for the checkbox.
     *
     * @attribute label
     * @type string
     * @default ''
     */
    label: '',

    /**
     * If true, the user cannot interact with this element.
     *
     * @attribute disabled
     * @type boolean
     * @default false
     */
    disabled: {value: false, reflect: true},
  },

  toggle: function() {
    this.$.checkbox.toggle();
  },

  ready: function() {
    this.$.events.forward(this.$.checkbox, ['change']);
  },
});
