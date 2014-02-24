// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * @constructor
 */
camera.Effect = function() {
};

/**
 * Randomizes the effect values.
 */
camera.Effect.prototype.randomize = function() {
};

/**
 * Filters the passed frame with the effect.
 *
 * @param {HTMLCanvasElement} canvas Canvas element to be filtered.
 * @param {Array.<camera.Tracker.Face>} faces Array of detected faces for this
 *     frame. Requires usesHeadTracker() to return true, otherwise the array is
 *     null.
 */
camera.Effect.prototype.filterFrame = function(canvas, faces) {
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
 * @return {boolean} True if slow, false otherwise.
 */
camera.Effect.prototype.isSlow = function() {
  return false;
};

/**
 * Returns true if the effect uses head tracker, and false otherwise. By default
 * false.
 * @return {boolean} True if uses, false otherwise.
 */
camera.Effect.prototype.usesHeadTracker = function() {
  return false;
};

