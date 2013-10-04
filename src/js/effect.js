// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * @param {camera.Tracker} tracker Head tracker object.
 * @constructor
 */
camera.Effect = function(tracker) {
  /**
   * @type {camera.Tracker}
   * @private
   */
  this.tracker_ = tracker;
};

/**
 * Randomizes the effect values.
 */
camera.Effect.prototype.randomize = function() {
};

/**
 * Filters the passed frame with the effect.
 * @param {HTMLCanvasElement} canvas Canvas element to be filtered.
 */
camera.Effect.prototype.filterFrame = function(canvas) {
};

/**
 * Provides title of the effect.
 * @return {string} Title.
 */
camera.Effect.prototype.getTitle = function() {
  return '(Effect)';
};

/**
 * Returns true if the effect is slow and preview should be done in lower
 * resolution to keep acceptable FPS.
 *
 * @return {boolean} True if slow, false otherwise.
 */
camera.Effect.prototype.isSlow = function() {
  return false;
};

