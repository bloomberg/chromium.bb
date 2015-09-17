// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @element cr-policy-indicator
 */

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

  observers: [
    'prefPolicyChanged_(pref.policySource, pref.policyEnforcement)'
  ],

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
    return '';
  }
});
