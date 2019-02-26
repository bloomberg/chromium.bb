/* Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */


/**
 * Alias for document.getElementById.
 * @param {string} id The ID of the element to find.
 * @return {HTMLElement} The found element or null if not found.
 */
function $(id) {
  // eslint-disable-next-line no-restricted-properties
  return document.getElementById(id);
}


/**
 * Contains common functions used in the main NTP page and its iframes.
 */
let utils = {};


/**
 * Disables the focus outline for |element| on mousedown.
 * @param {Element} element The element to remove the focus outline from.
 */
utils.disableOutlineOnMouseClick = function(element) {
  element.addEventListener('mousedown', () => {
    element.classList.add('mouse-navigation');
    element.addEventListener('blur', () => {
      element.classList.remove('mouse-navigation');
    }, {once: true});
  });
};


/**
 * Returns whether the given URL has a known, safe scheme.
 * @param {string} url URL to check.
 */
utils.isSchemeAllowed = function(url) {
  return url.startsWith('http://') || url.startsWith('https://') ||
      url.startsWith('ftp://') || url.startsWith('chrome-extension://');
};
