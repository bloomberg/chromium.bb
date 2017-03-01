// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled settings prefs.
 */

/** @polymerBehavior */
var CrPolicyPrefBehavior = {
  /**
   * @return {boolean} True if |this.pref| is controlled by an enforced policy.
   */
  isPrefPolicyControlled: function() {
    return (
        this.pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED &&
        this.pref.controlledBy !=
            chrome.settingsPrivate.ControlledBy.EXTENSION);
  },

  /**
   * @return {boolean} True if |this.pref| has a recommended or enforced policy.
   */
  hasPrefPolicyIndicator: function() {
    return this.isPrefPolicyControlled() ||
        this.pref.enforcement == chrome.settingsPrivate.Enforcement.RECOMMENDED;
  },
};
