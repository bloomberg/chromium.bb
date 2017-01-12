// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies that apply to an
 * element controlling a settings preference.
 */
Polymer({
  is: 'cr-policy-pref-indicator',

  behaviors: [CrPolicyIndicatorBehavior, CrPolicyPrefBehavior],

  properties: {
    /**
     * Optional preference object associated with the indicator. Initialized to
     * null so that computed functions will get called if this is never set.
     * @type {!chrome.settingsPrivate.PrefObject|undefined}
     */
    pref: Object,

    /**
     * Which indicator type to show (or NONE).
     * @type {CrPolicyIndicatorType}
     */
    indicatorType: {
      type: String,
      value: CrPolicyIndicatorType.NONE,
      computed: 'getIndicatorType(pref.controlledBy, pref.enforcement)',
    },
  },

  /**
   * @param {CrPolicyIndicatorType} type
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {string} The tooltip text for |type|.
   * @private
   */
  getTooltip_: function(type, pref) {
    var matches = pref && pref.value == pref.recommendedValue;
    return this.getPolicyIndicatorTooltip(
        type, pref.controlledByName || '', matches);
  }
});
