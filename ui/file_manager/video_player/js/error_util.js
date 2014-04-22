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
 * Counts uncaught exceptions.
 */
window.onerror = function() { window.JSErrorCount++; };

/**
 * Wraps the function to use it as a callback.
 * This does:
 *  - Capture the stack trace in case of error.
 *  - Bind this object
 *
 * @param {Object} thisObject Object to be used as this.
 * @return {function} Wapped function.
 */
Function.prototype.wrap = function(thisObject) {
  var func = this;
  var liveStack = (new Error('Stack trace before async call')).stack;
  if (thisObject === undefined)
    thisObject = null;

  return function wrappedCallback() {
    try {
      return func.apply(thisObject, arguments);
    } catch (e) {
      console.error('Exception happens in callback.', liveStack);

      window.JSErrorCount++;
      throw e;
    }
  }
};
