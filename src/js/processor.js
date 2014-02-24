// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Creates a processor object, which takes the camera stream and processes it.
 * Flushes the result to a canvas.
 *
 * @param {camera.Tracker} tracker Head tracker object.
 * @param {fx.Texture} input Texture with the input frame.
 * @param {Canvas} output Canvas with the output frame.
 * @param {fx.Canvas} fxCanvas Fx canvas to be used for processing.
 * @param {camera.Processor.Mode=} opt_mode Optional mode of the processor.
 *     Default is the high quality mode.
 * @constructor
 */
camera.Processor = function(tracker, input, output, fxCanvas, opt_mode) {
  /**
   * @type {camera.Tracker}
   * @private
   */
  this.tracker_ = tracker;

  /**
   * @type {fx.Texture}
   * @private
   */
  this.input_ = input;

  /**
   * @type {Canvas}
   * @private
   */
  this.output_ = output;

  /**
   * @type {fx.Canvas}
   * @private
   */
  this.fxCanvas_ = fxCanvas;

  /**
   * @type {camera.Processor.Mode}
   * @private
   */
  this.mode_ = opt_mode || camera.Processor.Mode.DEFAULT;

  /**
   * @type {camera.Effect}
   * @private
   */
  this.effect_ = null;

  // End of properties. Seal the object.
  Object.seal(this);
};

/**
 * Type of the processor. For FAST it uses lowered resolution. DEFAULT is high
 * quality.
 * @enum {number}
 */
camera.Processor.Mode = Object.freeze({
  DEFAULT: 0,
  FAST: 1
});

camera.Processor.prototype = {
  set effect(inEffect) {
    this.effect_ = inEffect;
  },
  get effect() {
    return this.effect_;
  },
  get output() {
    return this.output_;
  }
};

/**
 * Processes an input frame, applies effects and flushes result to the output
 * canvas.
 */
camera.Processor.prototype.processFrame = function() {
  var width = this.input_._.width;
  var height = this.input_._.height;
  if (!width || !height)
    return;

  var textureWidth = null;
  var textureHeight = null;

  switch (this.mode_) {
    case camera.Processor.Mode.FAST:
      textureWidth = Math.round(width / 2);
      textureHeight = Math.round(height / 2);
      break;
    case camera.Processor.Mode.DEFAULT:
      textureWidth = width;
      textureHeight = height;
      break;
  }

  this.fxCanvas_.draw(this.input_, textureWidth, textureHeight);
  if (this.effect_) {
    this.effect_.filterFrame(
        this.fxCanvas_,
        this.effect_.usesHeadTracker() ?
            this.tracker_.getFacesForCanvas(this.fxCanvas_) : null);
  }
  this.fxCanvas_.update();

  // If the fx canvas does not act as an output, then copy the result from it
  // to the output canvas.
  if (this.output_ != this.fxCanvas_) {
    var context = this.output_.getContext('2d');
    context.drawImage(
        this.fxCanvas_,
        0,
        0,
        this.fxCanvas_.width,
        this.fxCanvas_.height);
  }
};

