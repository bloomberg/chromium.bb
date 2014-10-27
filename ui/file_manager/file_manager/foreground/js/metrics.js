// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 */

var metrics = {};

/**
 * A map from interval name to interval start timestamp.
 */
metrics.intervals = {};

/**
 * Start the named time interval.
 * Should be followed by a call to recordInterval with the same name.
 *
 * @param {string} name Unique interval name.
 */
metrics.startInterval = function(name) {
  metrics.intervals[name] = Date.now();
};

metrics.startInterval('Load.Total');
metrics.startInterval('Load.Script');

/**
 * Convert a short metric name to the full format.
 *
 * @param {string} name Short metric name.
 * @return {string} Full metric name.
 * @private
 */
metrics.convertName_ = function(name) {
  return 'FileBrowser.' + name;
};

/**
 * Wrapper method for calling chrome.fileManagerPrivate safely.
 * @param {string} methodName Method name.
 * @param {Array.<Object>} args Arguments.
 * @private
 */
metrics.call_ = function(methodName, args) {
  try {
    chrome.metricsPrivate[methodName].apply(chrome.metricsPrivate, args);
  } catch (e) {
    console.error(e.stack);
  }
  if (metrics.log)
    console.log('chrome.metricsPrivate.' + methodName, args);
};

/**
 * Records a value than can range from 1 to 10,000.
 * @param {string} name Short metric name.
 * @param {number} value Value to be recorded.
 */
metrics.recordMediumCount = function(name, value) {
  metrics.call_('recordMediumCount', [metrics.convertName_(name), value]);
};

/**
 * Records a value than can range from 1 to 100.
 * @param {string} name Short metric name.
 * @param {number} value Value to be recorded.
 */
metrics.recordSmallCount = function(name, value) {
  metrics.call_('recordSmallCount', [metrics.convertName_(name), value]);
};

/**
 * Records an elapsed time of no more than 10 seconds.
 * @param {string} name Short metric name.
 * @param {number} time Time to be recorded in milliseconds.
 */
metrics.recordTime = function(name, time) {
  metrics.call_('recordTime', [metrics.convertName_(name), time]);
};

/**
 * Records an action performed by the user.
 * @param {string} name Short metric name.
 */
metrics.recordUserAction = function(name) {
  metrics.call_('recordUserAction', [metrics.convertName_(name)]);
};

/**
 * Complete the time interval recording.
 *
 * Should be preceded by a call to startInterval with the same name. *
 *
 * @param {string} name Unique interval name.
 */
metrics.recordInterval = function(name) {
  if (name in metrics.intervals) {
    metrics.recordTime(name, Date.now() - metrics.intervals[name]);
  } else {
    console.error('Unknown interval: ' + name);
  }
};

/**
 * Record an enum value.
 *
 * @param {string} name Metric name.
 * @param {*} value Enum value.
 * @param {Array.<*>|number} validValues Array of valid values
 *                                            or a boundary number value.
 */
metrics.recordEnum = function(name, value, validValues) {
  var boundaryValue;
  var index;
  if (validValues.constructor.name == 'Array') {
    index = validValues.indexOf(value);
    boundaryValue = validValues.length;
  } else {
    index = /** @type {number} */ (value);
    boundaryValue = validValues;
  }
  // Collect invalid values in the overflow bucket at the end.
  if (index < 0 || index > boundaryValue)
    index = boundaryValue;

  // Setting min to 1 looks strange but this is exactly the recommended way
  // of using histograms for enum-like types. Bucket #0 works as a regular
  // bucket AND the underflow bucket.
  // (Source: UMA_HISTOGRAM_ENUMERATION definition in base/metrics/histogram.h)
  var metricDescr = {
    'metricName': metrics.convertName_(name),
    'type': 'histogram-linear',
    'min': 1,
    'max': boundaryValue,
    'buckets': boundaryValue + 1
  };
  metrics.call_('recordValue', [metricDescr, index]);
};
