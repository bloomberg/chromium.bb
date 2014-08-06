// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JS helpers used on login.
 */

cr.define('cr.ui.LoginUITools', function() {
  return {
    /**
     * Computes max-height for an element so that it doesn't overlap shelf.
     * @param {element} DOM element
     */
    getMaxHeightBeforeShelfOverlapping : function(element) {
      var maxAllowedHeight =
          $('outer-container').offsetHeight -
          element.getBoundingClientRect().top -
          parseInt(window.getComputedStyle(element).marginTop) -
          parseInt(window.getComputedStyle(element).marginBottom);
      return maxAllowedHeight;
    }
  }
});
