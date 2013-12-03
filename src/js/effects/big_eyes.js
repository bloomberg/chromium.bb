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
 * @param {camera.Tracker} tracker Head tracker object.
 * @constructor
 * @extends {camera.Effect}
 */
camera.effects.BigEyes = function(tracker) {
  camera.Effect.call(this, tracker);

  /**
   * @type {number}
   * @private
   */
  this.amount_ = 0.3

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.effects.BigEyes.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.BigEyes.prototype.randomize = function() {
  this.amount_ = Math.random() * 0.5 + 0.1;
};

/**
 * @override
 */
camera.effects.BigEyes.prototype.filterFrame = function(canvas) {
  var face = this.tracker_.getFaceForCanvas(canvas);
  var x = canvas.width * (face.x + (face.width / 2));
  var y = canvas.height * face.y;

  // Left eye.
  canvas.bulgePinch(x - (face.width * canvas.width * -0.2),
                    y + (face.height * canvas.height * 0.35),
                    face.height * canvas.height * 0.3,
                    this.amount_);

  // Right eye.
  canvas.bulgePinch(x - (face.width * canvas.width * 0.2),
                    y + (face.height * canvas.height * 0.35),
                    face.height * canvas.height * 0.3,
                    this.amount_);
};

/**
 * @override
 */
camera.effects.BigEyes.prototype.getTitle = function() {
  return chrome.i18n.getMessage('bigEyesEffect');
};

