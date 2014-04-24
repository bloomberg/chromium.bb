// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * This variable is checked in SelectFileDialogExtensionBrowserTest.
 * @type {number}
 */
window.JSErrorCount = 0;

/**
 * Count uncaught exceptions.
 */
window.onerror = function() { window.JSErrorCount++; };

// Overrides console.error() to count errors.
/**
 * @param {...Object} var_args Message to be logged.
 */
console.error = (function() {
  var orig = console.error;
  return function() {
    window.JSErrorCount++;
    return orig.apply(this, arguments);
  };
})();

// Overrides console.assert() to count errors.
/**
 * @param {boolean} condition If false, log a message and stack trace.
 * @param {...Object} var_args Objects to.
 */
console.assert = (function() {
  var orig = console.assert;
  return function(condition) {
    if (!condition)
      window.JSErrorCount++;
    return orig.apply(this, arguments);
  };
})();
