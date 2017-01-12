// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled indicators.
 */

/**
 * Strings required for policy indicators. These must be set at runtime.
 * Chrome OS only strings may be undefined.
 * @type {{
 *   controlledSettingPolicy: string,
 *   controlledSettingRecommendedMatches: string,
 *   controlledSettingRecommendedDiffers: string,
 *   controlledSettingShared: (string|undefined),
 *   controlledSettingOwner: (string|undefined),
 * }}
 */
var CrPolicyStrings;

/** @enum {string} */
var CrPolicyIndicatorType = {
  DEVICE_POLICY: 'devicePolicy',
  EXTENSION: 'extension',
  NONE: 'none',
  OWNER: 'owner',
  PRIMARY_USER: 'primary_user',
  RECOMMENDED: 'recommended',
  USER_POLICY: 'userPolicy',
};

/** @polymerBehavior */
var CrPolicyIndicatorBehavior = {
  /**
   * @param {CrPolicyIndicatorType} type
   * @return {boolean} True if the indicator should be shown.
   * @private
   */
  isIndicatorVisible: function(type) {
    return type != CrPolicyIndicatorType.NONE &&
        type != CrPolicyIndicatorType.EXTENSION;
  },

  /**
   * @param {CrPolicyIndicatorType} type
   * @return {string} The iron-icon icon name.
   * @private
   */
  getPolicyIndicatorIcon: function(type) {
    var icon = '';
    switch (type) {
      case CrPolicyIndicatorType.EXTENSION:
      case CrPolicyIndicatorType.NONE:
        return icon;
      case CrPolicyIndicatorType.PRIMARY_USER:
        icon = 'group';
        break;
      case CrPolicyIndicatorType.OWNER:
        icon = 'person';
        break;
      case CrPolicyIndicatorType.USER_POLICY:
      case CrPolicyIndicatorType.DEVICE_POLICY:
      case CrPolicyIndicatorType.RECOMMENDED:
        icon = 'domain';
        break;
      default:
        assertNotReached();
    }
    return 'cr:' + icon;
  },

  /**
   * @param {CrPolicyIndicatorType} type
   * @param {string} name The name associated with the indicator. See
   *     chrome.settingsPrivate.PrefObject.controlledByName
   * @param {boolean=} opt_matches For RECOMMENDED only, whether the indicator
   *     value matches the recommended value.
   * @return {string} The tooltip text for |type|.
   */
  getPolicyIndicatorTooltip: function(type, name, opt_matches) {
    if (!CrPolicyStrings)
      return '';  // Tooltips may not be defined, e.g. in OOBE.
    switch (type) {
      case CrPolicyIndicatorType.PRIMARY_USER:
        return CrPolicyStrings.controlledSettingShared.replace('$1', name);
      case CrPolicyIndicatorType.OWNER:
        return CrPolicyStrings.controlledSettingOwner.replace('$1', name);
      case CrPolicyIndicatorType.USER_POLICY:
      case CrPolicyIndicatorType.DEVICE_POLICY:
        return CrPolicyStrings.controlledSettingPolicy;
      case CrPolicyIndicatorType.RECOMMENDED:
        return opt_matches ?
            CrPolicyStrings.controlledSettingRecommendedMatches :
            CrPolicyStrings.controlledSettingRecommendedDiffers;
    }
    return '';
  },
};
