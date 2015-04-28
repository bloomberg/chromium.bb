// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  is: 'cr-checkbox',

  properties: {
    /**
     * Gets or sets the state. `true` is checked and `false` is unchecked.
     *
     * @attribute checked
     * @type {boolean}
     * @default false
     */
    checked: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },

    /**
     * If true, the user cannot interact with this element.
     *
     * @attribute disabled
     * @type {boolean}
     * @default false
     */
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },
  },

  toggle: function() {
    this.$.checkbox.toggle();
  },
});
