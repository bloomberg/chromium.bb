// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'I18nBehavior' is a behavior to mix in loading of
 * internationalization strings.
 *
 * Example:
 *   behaviors: [
 *     I18nBehavior,
 *   ],
 *
 * @group Chrome UI Behavior
 */

/** @polymerBehavior */
var I18nBehavior = {
  /**
   * @param {string} id The ID of the string to translate.
   * @param {...string} var_args Placeholders required by the string ($1-9).
   * @return {string} A translated, substituted string.
   */
  i18n: function(id, var_args) {
    return loadTimeData.getStringF.apply(loadTimeData, arguments);
  },
};
