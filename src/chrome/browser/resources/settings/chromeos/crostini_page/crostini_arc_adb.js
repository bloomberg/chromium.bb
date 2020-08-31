// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-arc-adb' is the ARC adb sideloading subpage for Crostini.
 */

Polymer({
  is: 'settings-crostini-arc-adb',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** @private {boolean} */
    arcAdbEnabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the device requires a powerwash first (to define nvram for boot
     * lockbox). This happens to devices initialized through OOBE flow before
     * M74.
     * @private {boolean}
     */
    arcAdbNeedPowerwash_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    isOwnerProfile_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isOwnerProfile');
      },
    },

    /** @private {boolean} */
    isEnterpriseManaged_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isEnterpriseManaged');
      },
    },

    /** @private {boolean} */
    canChangeAdbSideloading_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('canChangeAdbSideloading');
      },
    },

    /** @private {boolean} */
    showConfirmationDialog_: {
      type: Boolean,
      value: false,
    },
  },

  attached() {
    this.addWebUIListener(
        'crostini-arc-adb-sideload-status-changed',
        (enabled, need_powerwash) => {
          this.arcAdbEnabled_ = enabled;
          this.arcAdbNeedPowerwash_ = need_powerwash;
        });
    settings.CrostiniBrowserProxyImpl.getInstance()
        .requestArcAdbSideloadStatus();
  },

  /**
   * Returns whether the toggle is changeable by the user. See
   * CrostiniFeatures::CanChangeAdbSideloading(). Note that the actual
   * guard should be in the browser, otherwise a user may bypass this check by
   * inspecting Settings with developer tools.
   * @return {boolean} Whether the control should be disabled.
   * @private
   */
  shouldDisable_() {
    return !this.canChangeAdbSideloading_ || this.arcAdbNeedPowerwash_;
  },

  /**
   * @return {CrPolicyIndicatorType} Which policy indicator to show (if any).
   * @private
   */
  getPolicyIndicatorType_() {
    if (this.isEnterpriseManaged_) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    } else if (!this.isOwnerProfile_) {
      return CrPolicyIndicatorType.OWNER;
    } else {
      return CrPolicyIndicatorType.NONE;
    }
  },

  /**
   * @return {string} Which action to perform when the toggle is changed.
   * @private
   */
  getToggleAction_() {
    return this.arcAdbEnabled_ ? 'disable' : 'enable';
  },

  /** @private */
  onArcAdbToggleChanged_() {
    this.showConfirmationDialog_ = true;
  },

  /** @private */
  onConfirmationDialogClose_() {
    this.showConfirmationDialog_ = false;
    this.$.arcAdbEnabledButton.checked = this.arcAdbEnabled_;
  },
});
