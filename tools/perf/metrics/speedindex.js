// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview This file provides a JavaScript helper function that
 * determines when the last resource was received.
 */
(function() {

  // Set the Resource Timing interface functions that will be used below
  // to use whatever version is available currently regardless of vendor
  // prefix.
  window.performance.clearResourceTimings =
      (window.performance.clearResourceTimings     ||
       window.performance.mozClearResourceTimings  ||
       window.performance.msClearResourceTimings   ||
       window.performance.oClearResourceTimings    ||
       window.performance.webkitClearResourceTimings);

  window.performance.getEntriesByType =
      (window.performance.getEntriesByType     ||
       window.performance.mozGetEntriesByType  ||
       window.performance.msGetEntriesByType   ||
       window.performance.oGetEntriesByType    ||
       window.performance.webkitGetEntriesByType);

  // This variable will available to the function below and it will be
  // persistent across different function calls. It stores the last
  // entry in the list of PerformanceResourceTiming objects returned by
  // window.performance.getEntriesByType('resource').
  //
  // The reason for doing it this way is because the buffer for
  // PerformanceResourceTiming objects has a limit, and once it's full,
  // new entries are not added. We're only interested in the last entry,
  // so we can clear new entries when they're added.
  var lastEntry = null;

  /**
   * This method uses the Resource Timing interface, which is described at
   * http://www.w3.org/TR/resource-timing/. It provides information about
   * timings for resources such as images and script files, and it includes
   * resources requested via XMLHttpRequest.
   *
   * @return {number} The time since either the load event, or the last
   *   the last resource was received after the load event. If the load
   *   event hasn't yet happened, return 0.
   */
  window.timeSinceLastResponseAfterLoadMs = function() {
    if (window.document.readyState !== 'complete') {
      return 0;
    }

    var resourceTimings = window.performance.getEntriesByType('resource');
    if (resourceTimings.length > 0) {
      lastEntry = resourceTimings.pop();
      window.performance.clearResourceTimings();
    }

    // The times for performance.now() and in the PerformanceResourceTiming
    // objects are all in milliseconds since performance.timing.navigationStart,
    // so we must also get load time in the same terms.
    var timing = window.performance.timing;
    var loadTime = timing.loadEventEnd - timing.navigationStart;

    // If there have been no resource timing entries, or the last entry was
    // before the load event, then return the time since the load event.
    if (!lastEntry || lastEntry.responseEnd < loadTime) {
      return window.performance.now() - loadTime;
    }
    return window.performance.now() - lastEntry.responseEnd;
  }

})();
