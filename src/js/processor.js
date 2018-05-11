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
 * @param {fx.Texture} input Texture with the input frame.
 * @param {Canvas} output Canvas with the output frame.
 * @param {fx.Canvas} fxCanvas Fx canvas to be used for processing.
 * @constructor
 */
camera.Processor = function(input, output, fxCanvas) {
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
   * @type {number}
   * @private
   */
  this.scale_ = 1;

  /**
   * Performance monitors for the processor.
   * @type {camera.util.NamedPerformanceMonitors}
   * @private
   */
  this.performanceMonitors_ = new camera.util.NamedPerformanceMonitors();

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.Processor.prototype = {
  set scale(inScale) {
    this.scale_ = inScale;
  },
  get scale() {
    return this.scale_;
  },
  get output() {
    return this.output_;
  },
  get performanceMonitors() {
    return this.performanceMonitors_;
  }
};

/**
 * Processes an input frame and flushes result to the output canvas.
 */
camera.Processor.prototype.processFrame = function() {
  var width = this.input_._.width;
  var height = this.input_._.height;
  if (!width || !height)
    return;

  var textureWidth = Math.round(width * this.scale_);
  var textureHeight = Math.round(height * this.scale_);

  {
    var finishMeasuring =
        this.performanceMonitors_.startMeasuring('draw-to-fx-canvas');
    this.fxCanvas_.draw(this.input_, textureWidth, textureHeight);
    finishMeasuring();
  }

  {
    var finishMeasuring =
        this.performanceMonitors_.startMeasuring('update-fx-canvas');
    this.fxCanvas_.update();
    finishMeasuring();
  }

  // If the fx canvas does not act as an output, then copy the result from it
  // to the output canvas.
  {
    var finishMeasuring =
        this.performanceMonitors_.startMeasuring('copy-fx-canvas-to-output');
    if (this.output_ != this.fxCanvas_) {
      var context = this.output_.getContext('2d');
      context.drawImage(
          this.fxCanvas_,
          0,
          0,
          this.fxCanvas_.width,
          this.fxCanvas_.height);
    }
    finishMeasuring();
  }
};

