// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Simple utility functions for Chromoting.
 */

/**
 * TODO(garykac): Remove this once host_list.js and host_table_entry.js have
 * been updated to use classList directly.
 *
 * @param {Element} element The element to which to add the class.
 * @param {string} cls The new class.
 * @return {void} Nothing.
 */
function addClass(element, cls) {
  var helem = /** @type {HTMLElement} */ element;
  helem.classList.add(cls);
}

/**
 * TODO(garykac): Remove this once host_list.js and host_table_entry.js have
 * been updated to use classList directly.
 *
 * @param {Element} element The element from which to remove the class.
 * @param {string} cls The new class.
 * @return {void} Nothing.
 */
function removeClass(element, cls) {
  var helem = /** @type {HTMLElement} */ element;
  helem.classList.remove(cls);
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

// This function can be called from the Javascript console to show all the UI,
// for example prior to auditing the CSS. It is not useful otherwise.
function unhideAll() {
  var hidden = document.querySelectorAll('[hidden]');
  for (var i in hidden) {
    hidden[i].hidden = false;
  }
}
