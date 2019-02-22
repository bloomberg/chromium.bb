// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'multidevice-radio-button',

  behaviors: [
    CrRadioButtonBehavior,
  ],

  properties: {
    disabled: {
      type: Boolean,
      reflectToAttribute: true,
      observer: 'disabledChanged_',
    },

    name: {
      type: String,
      notify: true,
    },
  },

  /**
   * Updates attributes of the control to reflect its disabled state.
   * @private
   */
  disabledChanged_: function() {
    this.setAttribute('tabindex', this.disabled ? -1 : 0);
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
  },

  /**
   * Prevents on-click handles on the control from being activated when the
   * indicator is clicked.
   * @param {!Event} e The click event.
   * @private
   */
  onIndicatorTap_: function(e) {
    e.preventDefault();
    e.stopPropagation();
  },
});
