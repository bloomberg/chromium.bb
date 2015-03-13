// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(wnwen): Move most of these functions to their own page rather than
//     common, which should be shared with content script.

/**
 * TODO(wnwen): Remove this and use actual web API.
 */
function $(id) {
  return document.getElementById(id);
}


/**
 * TODO(wnwen): Remove this, it's terrible.
 */
function siteFromUrl(url) {
  var a = document.createElement('a');
  a.href = url;
  return a.hostname;
}


/**
 * The filter should not apply to these URLs.
 *
 * @param {string} url The URL to check.
 */
function isDisallowedUrl(url) {
  return url.indexOf('chrome') == 0 || url.indexOf('about') == 0;
}


/**
 * Whether extension is loaded unpacked or from Chrome Webstore.
 */
function isDevMode = function() {
  return !('update_url' in chrome.runtime.getManifest());
}


/**
 * Easily turn on/off console logs.
 *
 * @param {*} message The message to potentially pass to {@code console.log}.
 */
var debugPrint;
if (isDevMode()) {
  debugPrint = console.log;
} else {
  debugPrint = function() {};
}
