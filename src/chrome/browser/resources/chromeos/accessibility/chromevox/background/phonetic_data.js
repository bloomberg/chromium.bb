// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides phonetic disambiguation functionality across multiple
 * languages for ChromeVox.
 *
 */

goog.provide('PhoneticData');

goog.require('JaPhoneticData');

/**
 * Initialization function for PhoneticData.
 */
PhoneticData.init = function() {
  JaPhoneticData.init();
};

/**
 * Returns the phonetic disambiguation for |char| in |locale|.
 * Returns empty string if disambiguation can't be found.
 * @param {string} char
 * @param {string} locale
 * @return {string}
 */
PhoneticData.forCharacter = function(char, locale) {
  const phoneticDictionaries =
      chrome.extension.getBackgroundPage().PhoneticDictionaries;
  if (!phoneticDictionaries || !phoneticDictionaries.phoneticMap_) {
    throw Error('PhoneticDictionaries map must be defined.');
  }

  if (!char || !locale) {
    throw Error('PhoneticData api requires non-empty arguments.');
  }

  char = char.toLowerCase();
  locale = locale.toLowerCase();
  let map = null;
  if (locale === 'ja') {
    map = JaPhoneticData.phoneticMap_;
  } else {
    // Try a lookup using |locale|, but use only the language component if the
    // lookup fails, e.g. "en-us" -> "en" or "zh-hant-hk" -> "zh".
    map = phoneticDictionaries.phoneticMap_[locale] ||
        phoneticDictionaries.phoneticMap_[locale.split('-')[0]];
  }

  if (!map) {
    return '';
  }

  return map[char] || '';
};

/**
 * @param {string} text
 * @param {string} locale
 * @return {string}
 */
PhoneticData.forText = function(text, locale) {
  const result = [];
  const chars = [...text];
  for (const char of chars) {
    const phoneticText = PhoneticData.forCharacter(char, locale);
    result.push(char + phoneticText);
  }
  return result.join(', ');
};
