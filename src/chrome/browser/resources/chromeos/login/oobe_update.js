// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Update screen.
 */

Polymer({
  is: 'oobe-update-md',

  behaviors: [OobeDialogHostBehavior],

  properties: {
    /**
     * Shows "Checking for update ..." section and hides "Updating..." section.
     */
    checkingForUpdate: {
      type: Boolean,
      value: true,
    },

    /**
     * Progress bar percent.
     */
    progressValue: {
      type: Number,
      value: 0,
    },

    /**
     * Message "3 minutes left".
     */
    estimatedTimeLeft: {
      type: String,
    },

    /**
     * Shows estimatedTimeLeft.
     */
    estimatedTimeLeftShown: {
      type: Boolean,
    },

    /**
     * Message "33 percent done".
     */
    progressMessage: {
      type: String,
    },

    /**
     * Shows progressMessage.
     */
    progressMessageShown: {
      type: Boolean,
    },

    /**
     * True if update is fully completed and, probably manual action is
     * required.
     */
    updateCompleted: {
      type: Boolean,
      value: false,
    },

    /**
     * If update cancellation is allowed.
     */
    cancelAllowed: {
      type: Boolean,
      value: false,
    },
  },

  onBeforeShow: function() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
    // 'indeterminate' paper-progress will recalculate styles on every frame
    // wnen OOBE is loaded (even when another screen is open).
    // So we make it 'indeterminate' only right before screen is shown, and
    // make it hidden when its container dialog is hidden.
    this.$['checking-progress'].indeterminate = true;
  },

  /**
   * This updates "Cancel Update" message.
   */
  setCancelHint: function(message) {
    this.$.checkingForUpdateCancelHint.textContent = message;
    this.$.updatingCancelHint.textContent = message;
  },

  /**
   * Calculates visibility of UI element. Returns true if element is hidden.
   * @param {Boolean} isAllowed Element flag that marks it visible.
   * @param {Boolean} updateCompleted If update is completed and all
   * intermediate status elements are hidden.
   */
  isNotAllowedOrUpdateCompleted_: function(isAllowed, updateCompleted) {
    return !isAllowed || updateCompleted;
  },
});
