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
camera.effects.Colorize = function() {
  camera.Effect.call(this);

  /**
   * @type {number}
   * @private
   */
  this.hue_ = 0.5;

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.effects.Colorize.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Colorize.prototype.randomize = function() {
  this.hue_ = Math.random() * 2.0 - 1.0;
};

/**
 * @override
 */
camera.effects.Colorize.prototype.filterFrame = function(canvas, faces) {
  canvas.hueSaturation(this.hue_, 0);
};

/**
 * @override
 */
camera.effects.Colorize.prototype.getTitle = function() {
  return chrome.i18n.getMessage('colorizeEffect');
};

