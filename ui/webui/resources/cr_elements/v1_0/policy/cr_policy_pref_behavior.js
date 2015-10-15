// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled settings prefs.
 */

/** @polymerBehavior */
var CrPolicyPrefBehavior = {
  /**
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean} True if the pref is controlled by an enforced policy.
   */
  isPrefPolicyControlled: function(pref) {
    return pref.policyEnforcement ==
           chrome.settingsPrivate.PolicyEnforcement.ENFORCED;
  },
};
