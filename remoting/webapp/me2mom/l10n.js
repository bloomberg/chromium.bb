/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Localize the document by replacing innerText of nodes with an i18n-content
 * attribute with the corresponding localized string.
 */

var l10n = l10n || {};

l10n.localize = function() {
  var elements = document.querySelectorAll('[i18n-content]');
  for (var i = 0; i < elements.length; ++i) {
    var element = elements[i];
    var tag = element.getAttribute('i18n-content');
    var translation = chrome.i18n.getMessage(tag);
    if (translation) {
      element.innerText = translation;
    } else {
      console.error('Missing translation for "' + tag +'":', element);
    }
  }
}
