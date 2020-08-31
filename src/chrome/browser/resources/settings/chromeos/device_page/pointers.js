// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-pointers' is the settings subpage with mouse and touchpad settings.
 */
Polymer({
  is: 'settings-pointers',

  behaviors: [
    PrefsBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    hasMouse: Boolean,

    hasTouchpad: Boolean,

    /**
     * TODO(michaelpg): settings-slider should optionally take a min and max so
     * we don't have to generate a simple range of natural numbers ourselves.
     * @type {!Array<number>}
     * @private
     */
    sensitivityValues_: {
      type: Array,
      value: [1, 2, 3, 4, 5],
      readOnly: true,
    },

    /**
     * TODO(zentaro): Remove this conditional once the feature is launched.
     * @private
     */
    allowDisableAcceleration_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('allowDisableMouseAcceleration');
      },
    },

    /**
     * TODO(khorimoto): Remove this conditional once the feature is launched.
     * @private
     */
    allowScrollSettings_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('allowScrollSettings');
      },
    },
  },

  // Used to correctly identify when the mouse button has been released.
  // crbug.com/686949.
  receivedMouseSwapButtonsDown_: false,

  /**
   * Mouse and touchpad sections are only subsections if they are both present.
   * @param {boolean} hasMouse
   * @param {boolean} hasTouchpad
   * @return {string}
   * @private
   */
  getSubsectionClass_(hasMouse, hasTouchpad) {
    return hasMouse && hasTouchpad ? 'subsection' : '';
  },

  /** @private */
  onMouseSwapButtonsDown_() {
    this.receivedMouseSwapButtonsDown_ = true;
  },

  /** @private */
  onMouseSwapButtonsUp_() {
    this.receivedMouseSwapButtonsDown_ = false;
    /** @type {!SettingsToggleButtonElement} */ (this.$.mouseSwapButton)
        .sendPrefChange();
  },

  /** @private */
  onMouseSwapButtonsChange_() {
    if (!this.receivedMouseSwapButtonsDown_) {
      /** @type {!SettingsToggleButtonElement} */ (this.$.mouseSwapButton)
          .sendPrefChange();
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onLearnMoreLinkClicked_: function(event) {
    if (!Array.isArray(event.path) || !event.path.length) {
      return;
    }

    if (event.path[0].tagName == 'A') {
      // Do not toggle reverse scrolling if the contained link is clicked.
      event.stopPropagation();
    }
  },

  /** @private */
  onReverseScrollRowClicked_: function() {
    this.setPrefValue(
        'settings.touchpad.natural_scroll',
        !this.getPref('settings.touchpad.natural_scroll').value);
  },
});
