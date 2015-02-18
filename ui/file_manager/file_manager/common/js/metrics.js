// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 */

var metrics = metrics || metricsBase;

/**
 * Analytics tracking ID for Files app.
 * @const {string}
 */
metrics.TRACKING_ID = 'UA-38248358-9';

/**
 * Convert a short metric name to the full format.
 *
 * @param {string} name Short metric name.
 * @return {string} Full metric name.
 * @override
 * @private
 */
metrics.convertName_ = function(name) {
  return 'FileBrowser.' + name;
};

/** @private {analytics.Tracker} */
metrics.tracker_ = null;

/** @private {boolean} */
metrics.enabled_ = false;

/** @return {!analytics.Tracker} */
metrics.getTracker = function() {
  if (!metrics.tracker_) {
    metrics.createTracker_();
  }
  return /** @type {!analytics.Tracker} */ (metrics.tracker_);
};

/**
 * Creates a new analytics tracker.
 * @private
 */
metrics.createTracker_ = function() {
  var analyticsService = analytics.getService('Files app');

  // Create a tracker, add a filter that only enables analytics when UMA is
  // enabled.
  metrics.tracker_ = analyticsService.getTracker(metrics.TRACKING_ID);
  metrics.tracker_.addFilter(metrics.umaEnabledFilter_);
};

/**
 * Queries the chrome UMA enabled setting, and filters hits based on that.
 * @param {!analytics.Tracker.Hit} hit
 * @private
 */
metrics.umaEnabledFilter_ = function(hit) {
  chrome.fileManagerPrivate.isUMAEnabled(
      /** @param {boolean} enabled */
      function(enabled) {
        metrics.enabled_ = enabled;
      });
  if (!metrics.enabled_) {
    hit.cancel();
  }
};
