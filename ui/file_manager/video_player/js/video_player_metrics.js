// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 */

/**
 * @extends {metricsBase}
 */
var metrics = metricsBase;

/**
 * Values for "VideoPlayer.CastAPIExtensionStaus" metrics.
 * @enum {number}
 */
metrics.CAST_API_EXTENSION_STATUS = {
  // Cast API extension is not loaded since the cast extension, which is requred
  // by the cast API extension, is not installed or load failed.
  SKIPPED: 0,
  // Installation of Cast API extension is failed.
  INSTALLATION_FAILED: 1,
  // Load of Cast API extension is failed.
  LOAD_FAILED: 2,
  // Cast API extension is newly installed and loaded.
  INSTALLED_AND_LOADED: 3,
  // Cast API extension is loaded.
  LOADED: 4,
  // (sentinel)
  MAX_VALUE: 5,
};

/**
 * Values for "VideoPlayer.PlayType" metrics.
 * @enum {number}
 */
metrics.PLAY_TYPE = {
  LOCAL: 0,
  CAST: 1,
  MAX_VALUE: 2,
};

/**
 * Utility function to check if the given value is in the given values.
 * @param {!Object} values
 * @param {*} value
 * @return {boolean} True if one or more elements of the given values hash have
 *     the given value as value. False otherwise.
 */
metrics.hasValue_ = function(values, value) {
  return Object.keys(values).some(function(key) {
    return values[key] === value;
  });
};

/**
 * Record "VideoPlayer.CastAPIExtensionStatsu" metrics.
 * @param {metrics.CAST_API_EXTENSION_STATUS} status Status to be recorded.
 */
metrics.recordCastAPIExtensionStatus = function(status) {
  if (!metrics.hasValue_(metrics.CAST_API_EXTENSION_STATUS, status)) {
    console.error('The given value "' + status + '" is invalid.');
    return;
  }

  metrics.recordEnum('CastAPIExtensionStatus',
                     status,
                     metrics.CAST_API_EXTENSION_STATUS.MAX_VALUE);
};

/**
 * Record "VideoPlayer.NumberOfCastDevices" metrics.
 * @param {number} number Value to be recorded.
 */
metrics.recordNumberOfCastDevices = function(number) {
  metrics.recordSmallCount('NumberOfCastDevices', number);
};

/**
 * Record "VideoPlayer.NumberOfOpenedFile" metrics.
 * @param {number} number Value to be recorded.
 */
metrics.recordNumberOfOpenedFiles = function(number) {
  metrics.recordSmallCount('NumberOfOpenedFiles', number);
};

/**
 * Record "VideoPlayer.CastedVideoLength" metrics.
 * @param {number} seconds Value to be recorded.
 */
metrics.recordCastedVideoLength = function(seconds) {
  metrics.recordMediumCount('CastedVideoLength', seconds);
};

/**
 * Record "VideoPlayer.CastVideoError" metrics.
 */
metrics.recordCastVideoErrorAction = function() {
  metrics.recordUserAction('CastVideoError');
};

/**
 * Record "VideoPlayer.OpenVideoPlayer" action.
 */
metrics.recordOpenVideoPlayerAction = function() {
  metrics.recordUserAction('OpenVideoPlayer');
};

/**
 * Record "VideoPlayer.PlayType" metrics.
 * @param {metrics.PLAY_TYPE} type Value to be recorded.
 */
metrics.recordPlayType = function(type) {
  if (!metrics.hasValue_(metrics.PLAY_TYPE, type)) {
    console.error('The given value "' + type + '" is invalid.');
    return;
  }

  metrics.recordEnum('PlayType',
                     type,
                     metrics.PLAY_TYPE.MAX_VALUE);
};

/**
 * Convert a short metric name to the full format.
 *
 * @param {string} name Short metric name.
 * @return {string} Full metric name.
 * @override
 * @private
 */
metrics.convertName_ = function(name) {
  return 'VideoPlayer.' + name;
};
