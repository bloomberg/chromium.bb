// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Returns full Chrome version.
 * @return {string?}
 */
remoting.getChromeVersion = function() {
  var match = new RegExp('Chrome/([0-9.]*)').exec(navigator.userAgent);
  if (match && (match.length >= 2)) {
    return match[1];
  }
  return null;
};

/**
 * Returns Chrome major version.
 * @return {number}
 */
remoting.getChromeMajorVersion = function() {
  var match = new RegExp('Chrome/([0-9]+)\.').exec(navigator.userAgent);
  if (match && (match.length >= 2)) {
    return parseInt(match[1], 10);
  }
  return 0;
};

/**
 * Tests whether we are running on Mac.
 *
 * @return {boolean} True if the platform is Mac.
 */
remoting.platformIsMac = function() {
  return navigator.platform.indexOf('Mac') != -1;
}

/**
 * Tests whether we are running on Windows.
 *
 * @return {boolean} True if the platform is Windows.
 */
remoting.platformIsWindows = function() {
  return (navigator.platform.indexOf('Win32') != -1) ||
         (navigator.platform.indexOf('Win64') != -1);
}

/**
 * Tests whether we are running on Linux.
 *
 * @return {boolean} True if the platform is Linux.
 */
remoting.platformIsLinux = function() {
  return (navigator.platform.indexOf('Linux') != -1) &&
         !remoting.platformIsChromeOS();
}

/**
 * Tests whether we are running on ChromeOS.
 *
 * @return {boolean} True if the platform is ChromeOS.
 */
remoting.platformIsChromeOS = function() {
  return navigator.userAgent.match(/\bCrOS\b/) != null;
}
