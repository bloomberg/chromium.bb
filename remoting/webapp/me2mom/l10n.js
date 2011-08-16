/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var l10n = l10n || {};

/**
 * Localize an element by setting its innerText according to its i18n-content
 * attribute, and an optional set of substitutions.
 * @param {Element} element The element to localize.
 * @param {string=} opt_substitutions An optional set of substitution strings
 *     corresponding to the "placeholders"attributes in messages.json.
 * @return {boolean} True if the localization was successful; false otherwise.
 */
l10n.localizeElement = function(element, opt_substitutions) {
  var tag = element.getAttribute('i18n-content');
  var translation = chrome.i18n.getMessage(tag, opt_substitutions);
  if (translation) {
    element.innerText = translation;
  } else {
    console.error('Missing translation for "' + tag + '":', element);
  }
  return translation != null;
};

/**
 * Localize all tags with the i18n-content attribute.
 */
l10n.localize = function() {
  var elements = document.querySelectorAll('[i18n-content]');
  for (var i = 0; i < elements.length; ++i) {
    var element = elements[i];
    l10n.localizeElement(element);
  }
};
