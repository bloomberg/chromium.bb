// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'controlled-button',

  behaviors: [
    CrPolicyPrefBehavior,
    PrefControlBehavior,
  ],

  properties: {
    actionButton: {
      type: Boolean,
      value: false,
    },

    endJustified: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    label: String,

    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @private */
    enforced_: {
      type: Boolean,
      computed: 'isPrefEnforced(pref.*)',
      reflectToAttribute: true,
    },
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorTap_: function(e) {
    // Disallow <controlled-button on-click="..."> when controlled.
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * @param {!boolean} actionButton
   * @return {string} Class of the paper-button.
   * @private
   */
  getClass_: function(actionButton) {
    return actionButton ? "action-button" : "";
  },

  /**
   * @param {!boolean} enforced
   * @param {!boolean} disabled
   * @return {boolean} True if the button should be enabled.
   * @private
   */
  buttonEnabled_(enforced, disabled) {
    return !enforced && !disabled;
  }
});
