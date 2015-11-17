// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for effects.
 */
camera.effects = camera.effects || {};

/**
 * @constructor
 * @extends {camera.Effect}
 */
camera.effects.Beauty = function() {
  camera.Effect.call(this);

  /**
   * @type {number}
   * @private
   */
  this.brightness_ = 0.25;

  /**
   * @type {number}
   * @private
   */
  this.contrast_ = 0.25;

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.effects.Beauty.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Beauty.prototype.randomize = function() {
  this.brightness_ = Math.random() * 0.4;
  this.contrast_ = Math.random() * 0.6 - 0.2;
};

/**
 * @override
 */
camera.effects.Beauty.prototype.filterFrame = function(canvas, faces) {
  var face = faces[0];
  var factor = 720 / canvas.width * 2;
  var exponent = 50 * (1 - face.width) * factor;
  canvas.denoise(exponent);
  canvas.brightnessContrast(this.brightness_, this.contrast_);
};

/**
 * @override
 */
camera.effects.Beauty.prototype.getTitle = function() {
  return chrome.i18n.getMessage('beautyEffect');
};

/**
 * @override
 */
camera.effects.Beauty.prototype.isSlow = function() {
  return true;
};

/**
 * @override
 */
camera.effects.Beauty.prototype.usesHeadTracker = function() {
  return true;
};

