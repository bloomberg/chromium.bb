// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Simple utility functions for Chromoting.
 */

/**
 * @param {string} classes A space-separated list of classes.
 * @param {string} cls The class to check for.
 * @return {boolean} True if |cls| is found within |classes|.
 */
function hasClass(classes, cls) {
  return classes.match(new RegExp('(\\s|^)' + cls + '(\\s|$)')) != null;
}

/**
 * @param {Element} element The element to which to add the class.
 * @param {string} cls The new class.
 * @return {void} Nothing.
 */
function addClass(element, cls) {
  if (!hasClass(element.className, cls)) {
    var padded = element.className == '' ? '' : element.className + ' ';
    element.className = padded + cls;
  }
}

/**
 * @param {Element} element The element from which to remove the class.
 * @param {string} cls The new class.
 * @return {void} Nothing.
 */
function removeClass(element, cls) {
  element.className =
      element.className.replace(new RegExp('\\b' + cls + '\\b', 'g'), '')
                       .replace('  ', ' ');
}

/**
 * @return {Object.<string, string>} The URL parameters.
 */
function getUrlParameters() {
  var result = {};
  var parts = window.location.search.substring(1).split('&');
  for (var i = 0; i < parts.length; i++) {
    var pair = parts[i].split('=');
    result[pair[0]] = decodeURIComponent(pair[1]);
  }
  return result;
}
