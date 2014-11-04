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
