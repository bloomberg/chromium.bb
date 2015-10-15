// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies that apply to an
 * element controlling a settings preference.
 */

var CrPolicyIndicator = {
  /** @enum {string} */
  Type: {
    DEVICE_POLICY: 'devicePolicy',
    EXTENSION: 'extension',
    NONE: 'none',
    OWNER: 'owner',
    PRIMARY_USER: 'primary_user',
    RECOMMENDED: 'recommended',
    USER_POLICY: 'userPolicy',
  },
};

(function() {

/** @element cr-policy-pref-indicator */
Polymer({
  is: 'cr-policy-pref-indicator',

  properties: {
    /**
     * Optional preference object associated with the indicator. Initialized to
     * null so that computed functions will get called if this is never set.
     * @type {?chrome.settingsPrivate.PrefObject}
     */
    pref: {type: Object, value: null},

    /**
     * Optional email of the user controlling the setting when the setting does
     * not correspond to a pref (Chrome OS only). Only used when pref is null.
     * Initialized to '' so that computed functions will get called if this is
     * never set.
     */
    controllingUser: {type: String, value: ''},

    /**
     * Which indicator type to show (or NONE).
     * @type {CrPolicyIndicator.Type}
     */
    indicatorType: {type: String, value: CrPolicyIndicator.Type.NONE},
  },

  observers: ['prefPolicyChanged_(pref.policySource, pref.policyEnforcement)'],

  /**
   * Polymer observer for pref.
   * @param {chrome.settingsPrivate.PolicySource} source
   * @param {chrome.settingsPrivate.PolicyEnforcement} enforcement
   * @private
   */
  prefPolicyChanged_: function(source, enforcement) {
    var type = CrPolicyIndicator.Type.NONE;
    if (enforcement == chrome.settingsPrivate.PolicyEnforcement.ENFORCED) {
      if (source == chrome.settingsPrivate.PolicySource.PRIMARY_USER)
        type = CrPolicyIndicator.Type.PRIMARY_USER;
      else if (source == chrome.settingsPrivate.PolicySource.OWNER)
        type = CrPolicyIndicator.Type.OWNER;
      else if (source == chrome.settingsPrivate.PolicySource.USER_POLICY)
        type = CrPolicyIndicator.Type.USER_POLICY;
      else if (source == chrome.settingsPrivate.PolicySource.DEVICE_POLICY)
        type = CrPolicyIndicator.Type.DEVICE_POLICY;
      else if (source == chrome.settingsPrivate.PolicySource.EXTENSION)
        type = CrPolicyIndicator.Type.EXTENSION;
    } else if (enforcement ==
               chrome.settingsPrivate.PolicyEnforcement.RECOMMENDED) {
      type = CrPolicyIndicator.Type.RECOMMENDED;
    }
    this.indicatorType = type;
  },

  /**
   * @param {CrPolicyIndicator.Type} type
   * @return {boolean} True if the indicator should be shown.
   * @private
   */
  isIndicatorVisible_: function(type) {
    return type != CrPolicyIndicator.Type.NONE;
  },

  /**
   * @param {CrPolicyIndicator.Type} type
   * @return {string} The iron-icons icon name.
   * @private
   */
  getIcon_: function(type) {
    switch (type) {
      case CrPolicyIndicator.Type.NONE:
        return '';
      case CrPolicyIndicator.Type.PRIMARY_USER:
        return 'social:group';
      case CrPolicyIndicator.Type.OWNER:
        return 'social:person';
      case CrPolicyIndicator.Type.USER_POLICY:
      case CrPolicyIndicator.Type.DEVICE_POLICY:
        return 'social:domain';
      case CrPolicyIndicator.Type.EXTENSION:
        return 'extension';
      case CrPolicyIndicator.Type.RECOMMENDED:
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
   * @param {CrPolicyIndicator.Type} type The type of indicator.
   * @param {?chrome.settingsPrivate.PrefObject} pref
   * @param {string} controllingUser The user controlling the setting, if |pref|
   *     is null.
   * @return {string} The tooltip text for |type|.
   * @private
   */
  getTooltipText_: function(type, pref, controllingUser) {
    var name = pref ? pref.policySourceName : controllingUser;

    switch (type) {
      case CrPolicyIndicator.Type.PRIMARY_USER:
        return this.i18n_('controlledSettingShared', name);
      case CrPolicyIndicator.Type.OWNER:
        return this.i18n_('controlledSettingOwner', name);
      case CrPolicyIndicator.Type.USER_POLICY:
      case CrPolicyIndicator.Type.DEVICE_POLICY:
        return this.i18n_('controlledSettingPolicy');
      case CrPolicyIndicator.Type.EXTENSION:
        return this.i18n_('controlledSettingExtension', name);
      case CrPolicyIndicator.Type.RECOMMENDED:
        if (pref && pref.value == pref.recommendedValue)
          return this.i18n_('controlledSettingRecommendedMatches');
        return this.i18n_('controlledSettingRecommendedDiffers');
    }
    return '';
  }
});
})();
