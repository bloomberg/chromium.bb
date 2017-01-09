// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies based on network
 * properties.
 */

Polymer({
  is: 'cr-policy-network-indicator',

  behaviors: [CrPolicyIndicatorBehavior, CrPolicyNetworkBehavior],

  properties: {
    /**
     * Network property associated with the indicator.
     * @type {!CrOnc.ManagedProperty|undefined}
     */
    property: {type: Object, observer: 'propertyChanged_'},

    /**
     * Which indicator type to show (or NONE).
     * @type {!CrPolicyIndicatorType}
     */
    indicatorType: {type: String, value: CrPolicyIndicatorType.NONE},

    /**
     * Recommended value for non enforced properties.
     * @private {!CrOnc.NetworkPropertyType|undefined}
     */
    recommended_: Object,
  },

  /**
   * @param {!CrOnc.ManagedProperty} property Always defined property value.
   * @private
   */
  propertyChanged_: function(property) {
    if (!this.isControlled(property)) {
      this.indicatorType = CrPolicyIndicatorType.NONE;
      return;
    }
    var effective = property.Effective;
    var active = property.Active;
    if (active == undefined)
      active = property[effective];

    if (property.UserEditable === true &&
        property.hasOwnProperty('UserPolicy')) {
      // We ignore UserEditable unless there is a UserPolicy.
      this.recommended_ =
          /** @type {!CrOnc.NetworkPropertyType} */ (property.UserPolicy);
      this.indicatorType = CrPolicyIndicatorType.RECOMMENDED;
    } else if (
        property.DeviceEditable === true &&
        property.hasOwnProperty('DevicePolicy')) {
      // We ignore DeviceEditable unless there is a DevicePolicy.
      this.recommended_ =
          /** @type {!CrOnc.NetworkPropertyType} */ (property.DevicePolicy);
      this.indicatorType = CrPolicyIndicatorType.RECOMMENDED;
    } else if (effective == 'UserPolicy') {
      this.indicatorType = CrPolicyIndicatorType.USER_POLICY;
    } else if (effective == 'DevicePolicy') {
      this.indicatorType = CrPolicyIndicatorType.DEVICE_POLICY;
    } else if (effective == 'ActiveExtension') {
      this.indicatorType = CrPolicyIndicatorType.EXTENSION;
    } else {
      this.indicatorType = CrPolicyIndicatorType.NONE;
    }
  },

  /**
   * @return {string} The tooltip text for |type|.
   * @private
   */
  getTooltip_: function() {
    if (this.indicatorType == CrPolicyIndicatorType.NONE)
      return '';
    if (this.indicatorType == CrPolicyIndicatorType.RECOMMENDED) {
      if (!this.property)
        return '';
      var value = this.property.Active;
      if (value == undefined && this.property.Effective)
        value = this.property[this.property.Effective];
      if (value == this.recommended_)
        return this.i18n_('controlledSettingRecommendedMatches');
      return this.i18n_('controlledSettingRecommendedDiffers');
    }
    return this.getPolicyIndicatorTooltip(this.indicatorType, '');
  }
});
