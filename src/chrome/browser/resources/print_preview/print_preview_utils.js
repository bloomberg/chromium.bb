// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns true if the contents of the two page ranges are equal.
 * @param {!Array<{ to: number, from: number }>} array1 The first array.
 * @param {!Array<{ to: number, from: number }>} array2 The second array.
 * @return {boolean} true if the arrays are equal.
 */
function areRangesEqual(array1, array2) {
  if (array1.length != array2.length) {
    return false;
  }
  for (let i = 0; i < array1.length; i++) {
    if (array1[i].from != array2[i].from || array1[i].to != array2[i].to) {
      return false;
    }
  }
  return true;
}

/**
 * @param {!Array<!{locale: string, value: string}>} localizedStrings An array
 *     of strings with corresponding locales.
 * @param {string} locale Locale to look the string up for.
 * @return {string} A string for the requested {@code locale}. An empty string
 *     if there's no string for the specified locale found.
 */
function getStringForLocale(localizedStrings, locale) {
  locale = locale.toLowerCase();
  for (let i = 0; i < localizedStrings.length; i++) {
    if (localizedStrings[i].locale.toLowerCase() == locale) {
      return localizedStrings[i].value;
    }
  }
  return '';
}

/**
 * @param {!Array<!{locale: string, value: string}>} localizedStrings An array
 *     of strings with corresponding locales.
 * @return {string} A string for the current locale. An empty string if there's
 *     no string for the current locale found.
 */
function getStringForCurrentLocale(localizedStrings) {
  // First try to find an exact match and then look for the language only.
  return getStringForLocale(localizedStrings, navigator.language) ||
      getStringForLocale(localizedStrings, navigator.language.split('-')[0]);
}

/**
 * @param {!Array<*>} args The arguments for the observer.
 * @return {boolean} Whether all arguments are defined.
 */
function observerDepsDefined(args) {
  return args.every(arg => arg !== undefined);
}
