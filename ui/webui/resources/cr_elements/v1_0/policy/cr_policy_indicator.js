// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies that apply to an
 * element controlling a settings preference.
 */

(function() {

/** @enum {string} */
var IndicatorType = {
  DEVICE_POLICY: 'devicePolicy',
  EXTENSION: 'extension',
  NONE: 'none',
  OWNER: 'owner',
  PRIMARY_USER: 'primary_user',
  RECOMMENDED: 'recommended',
  USER_POLICY: 'userPolicy',
};

/** @element cr-policy-indicator */
Polymer({
  is: 'cr-policy-indicator',

  properties: {
    /**
     * The preference object to show an indicator for.
     * @type {chrome.settingsPrivate.PrefObject|undefined}
     */
    pref: {type: Object},

    /**
     * Which indicator type to show (or NONE).
     * @type {IndicatorType}
     */
    indicatorType: {type: String, value: IndicatorType.NONE},
  },

  observers: ['prefPolicyChanged_(pref.policySource, pref.policyEnforcement)'],

  /**
   * Polymer observer for pref.
   * @param {chrome.settingsPrivate.PolicySource} source
   * @param {chrome.settingsPrivate.PolicyEnforcement} enforcement
   * @private
   */
  prefPolicyChanged_: function(source, enforcement) {
    var type = IndicatorType.NONE;
    if (enforcement == chrome.settingsPrivate.PolicyEnforcement.ENFORCED) {
      if (source == chrome.settingsPrivate.PolicySource.PRIMARY_USER)
        type = IndicatorType.PRIMARY_USER;
      else if (source == chrome.settingsPrivate.PolicySource.OWNER)
        type = IndicatorType.OWNER;
      else if (source == chrome.settingsPrivate.PolicySource.USER_POLICY)
        type = IndicatorType.USER_POLICY;
      else if (source == chrome.settingsPrivate.PolicySource.DEVICE_POLICY)
        type = IndicatorType.DEVICE_POLICY;
      else if (source == chrome.settingsPrivate.PolicySource.EXTENSION)
        type = IndicatorType.EXTENSION;
    } else if (enforcement ==
               chrome.settingsPrivate.PolicyEnforcement.RECOMMENDED) {
      type = IndicatorType.RECOMMENDED;
    }
    this.indicatorType = type;
  },

  /**
   * @param {IndicatorType} type
   * @return {boolean} True if the indicator should be shown.
   * @private
   */
  isIndicatorVisible_: function(type) { return type != IndicatorType.NONE; },

  /**
   * @param {IndicatorType} type
   * @return {string} The iron-icons icon name.
   * @private
   */
  getIcon_: function(type) {
    switch (type) {
      case IndicatorType.NONE:
        return '';
      case IndicatorType.PRIMARY_USER:
        return 'social:group';
      case IndicatorType.OWNER:
        return 'social:person';
      case IndicatorType.USER_POLICY:
      case IndicatorType.DEVICE_POLICY:
        return 'social:domain';
      case IndicatorType.EXTENSION:
        return 'extension';
      case IndicatorType.RECOMMENDED:
        return 'social:domain';
    }
    assertNotReached();
  },

  /**
   * @param {string} id The id of the string to translate.
   * @param {string=} opt_name An optional name argument.
   * @return The translated string.
   */
  i18n_: function (id, opt_name) {
    return loadTimeData.getStringF(id, opt_name);
  },

  /**
   * @param {IndicatorType} type The type of indicator.
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {string} The tooltip text for |type|.
   * @private
   */
  getTooltipText_: function(type, pref) {
    var name = pref.policySourceName || '';
    switch (type) {
      case IndicatorType.PRIMARY_USER:
        return this.i18n_('controlledSettingShared', name);
      case IndicatorType.OWNER:
        return this.i18n_('controlledSettingOwner', name);
      case IndicatorType.USER_POLICY:
      case IndicatorType.DEVICE_POLICY:
        return this.i18n_('controlledSettingPolicy');
      case IndicatorType.EXTENSION:
        return this.i18n_('controlledSettingExtension', name);
      case IndicatorType.RECOMMENDED:
        if (pref.value == pref.recommendedValue)
          return this.i18n_('controlledSettingRecommendedMatches');
        return this.i18n_('controlledSettingRecommendedDiffers');
    }
    return '';
  }
});
})();
