// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `cr-toggle-button` provides a switch the user can toggle on or off by
 * tapping or by dragging. Wraps a `paper-toggle-button`.
 *
 * Example:
 *
 *    <cr-toggle-button></cr-toggle-button>
 *
 * @element cr-toggle-button
 */
Polymer({
  is: 'cr-toggle-button',

  properties: {
    /**
     * Gets or sets the state. `true` is checked and `false` is unchecked.
     */
    checked: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true,
    },

    /**
     * If true, the toggle button is disabled.
     */
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  ready: function() {
    this.$.events.forward(this.$.button, ['change']);
  },
});
