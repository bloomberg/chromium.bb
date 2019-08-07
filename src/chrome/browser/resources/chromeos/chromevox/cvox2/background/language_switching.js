// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides language switching services for ChromeVox.
 */

goog.provide('LanguageSwitching');

/**
 * The current output language.
 * @private {string}
 */
LanguageSwitching.currentLanguage = '';

/**
 * Maps languages to array of country codes that are considered equivalent.
 * @private {!Object<string, !Array<string>>}
 */
LanguageSwitching.equivalentDialects = {
  'en': ['au', 'ca', 'gb', 'ie', 'nz', 'us', '']
  // TODO(akihiroota): Populate this object.
};

/**
 * Updates current language and returns the output language for node.
 * @param {AutomationNode} node
 * @return {string}
 */
LanguageSwitching.updateCurrentLanguageForNode = function(node) {
  if (!node)
    return LanguageSwitching.currentLanguage;
  // Use detected language. If unavailable, fallback on author-provided
  // language.
  var targetLanguage = node.detectedLanguage || node.language;
  // If targetLanguage is still empty, then do not switch languages.
  if (!targetLanguage)
    return LanguageSwitching.currentLanguage;

  targetLanguage = targetLanguage.toLowerCase();
  // Validate targetLanguage.
  // Each language code should be of length 2 or 5 AND each component should
  // be of length 2.
  // Ex: en or en-us
  if (!(targetLanguage.length === 2 || targetLanguage.length === 5))
    return LanguageSwitching.currentLanguage;
  var arr = targetLanguage.split('-');
  for (var i = 0; i < arr.length; ++i) {
    if (!(arr[i].length === 2))
      return LanguageSwitching.currentLanguage;
  }

  // Only switch languages if targetLanguage is different than currentLanguage.
  if (!LanguageSwitching.isEquivalentToCurrentLanguage(targetLanguage))
    LanguageSwitching.currentLanguage = targetLanguage;
  return LanguageSwitching.currentLanguage;
};

/**
 * Returns true if targetLanguage is equivalent to current language.
 * @param {!string} targetLanguage
 * @return {boolean}
 */
LanguageSwitching.isEquivalentToCurrentLanguage = function(targetLanguage) {
  // Language codes are composed of two components: language and country.
  // Ex: en-us represents American English.
  // Note: language codes sometimes come without the country component.

  // Compare the language codes of current and target language.
  if (LanguageSwitching.currentLanguage.substring(0, 2) !==
      targetLanguage.substring(0, 2)) {
    return false;
  }

  // Lookup the equivalent country codes for language.
  var countryCodes =
      LanguageSwitching.equivalentDialects[LanguageSwitching.currentLanguage
                                               .substring(0, 2)];

  // If language not in object, return false.
  if (countryCodes === undefined)
    return false;

  var currentCountryCode = LanguageSwitching.currentLanguage.substring(3);
  var targetCountryCode = targetLanguage.substring(3);

  return countryCodes.includes(currentCountryCode) &&
      countryCodes.includes(targetCountryCode);
};
