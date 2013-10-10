// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Detects and tracks faces on the input stream.
 *
 * @param {Canvas} input Input canvas.
 * @constructor
 */
camera.Tracker = function(input) {
  /**
   * @type {Canvas}
   * @private
   */
  this.input_ = input;

  /**
   * @type {camera.Tracker.Face}
   * @private
   */
  this.face_ = new camera.Tracker.Face();

  /**
   * @type {boolean}
   * @private
   */
  this.busy_ = false;

  // End of properties. Seal the object.
  Object.seal(this);
};

/**
 * Represents a detected face.
 * @constructor
 */
camera.Tracker.Face = function() {
  /**
   * @type {number}
   * @private
   */
  this.x_ = 0;

  /**
   * @type {number}
   * @private
   */
  this.y_ = 0;

  /**
   * @type {number}
   * @private
   */
  this.targetX_ = 0;

  /**
   * @type {number}
   * @private
   */
  this.targetY_ = 0;

  /**
   * @type {number}
   * @private
   */
  this.width_ = 0.3;

  /**
   * @type {number}
   * @private
   */
  this.height_ = 0.3;

  /**
   * @type {number}
   * @private
   */
  this.targetWidth_ = 0.3;

  /**
   * @type {number}
   * @private
   */
  this.targetHeight_ = 0.3;

  /**
   * @type {number}
   * @private
   */
  this.confidence_ = 0;

  /**
   * @type {number}
   * @private
   */
  this.targetConfidence_ = 0;

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.Tracker.Face.prototype = {
  /**
   * Returns the current interpolated x coordinate of the head.
   * @return {number} Position as fraction (0-1) of the width.
   */
  get x() {
    return this.x_;
  },

  /**
   * Returns the current interpolated y coordinate of the head.
   * @return {number} Position as fraction (0-1) of the width.
   */
  get y() {
    return this.y_;
  },

  /**
   * Returns the current interpolated width of the head.
   * @return {number} Width as fraction (0-1) of the image's width.
   */
  get width() {
    return this.width_;
  },

  /**
   * Returns the current interpolated height of the head.
   * @return {number} Height as fraction (0-1) of the image's height.
   */
  get height() {
    return this.height_;
  },

  /**
   * Returns the current interpolated confidence of detection.
   * @return {number} Confidence level.
   */
  get confidence() {
    return this.confidence_;
  },

  /**
   * Sets the target x coordinate of the face.
   * @param {number} x Position as a fraction of the width (0-1).
   */
  set targetX(x) {
    this.targetX_ = x;
  },

  /**
   * Sets the target y coordinate of the face.
   * @param {number} y Position as a fraction of the height (0-1).
   */
  set targetY(y) {
    this.targetY_ = y;
  },

  /**
   * Sets the target width of the face.
   * @param {number} width Width as a fraction of the image's width (0-1).
   */
  set targetWidth(width) {
    this.targetWidth_ = width;
  },

  /**
   * Sets the target height of the face.
   * @param {number} height Height as a fraction of the image's height (0-1).
   */
  set targetHeight(height) {
    this.targetHeight_ = height;
  },

  /**
   * Sets the target confidence level.
   * @param {number} Confidence level.
   */
  set targetConfidence(confidence) {
    this.targetConfidence_ = confidence;
  }
};

/**
 * Updates the detected face by applying some interpolation.
 */
camera.Tracker.Face.prototype.update = function() {
  var step = 0.3;
  this.x_ += (this.targetX_ - this.x_) * step;
  this.y_ += (this.targetY_ - this.y_) * step;
  this.width_ += (this.targetWidth_ - this.width_) * step;
  this.height_ += (this.targetHeight_ - this.height_) * step;
  this.confidence_ += (this.targetConfidence_ - this.confidence_) * step;
};

camera.Tracker.prototype = {
 /**
  * Returns detected faces by the last call of update().
  * @return {camera.Tracker.Face} Detected face object.
  */
  get face() {
    return this.face_;
  }
};

/**
 * Requests face detection on the current frame.
 */
camera.Tracker.prototype.detect = function() {
  if (this.busy_)
    return;
  this.busy_ = true;

  var result = ccv.detect_objects({
    canvas: this.input_,
    interval: 5,
    min_neighbors: 1,
    worker: 1,
    async: true
  })(function(result) {
    if (result.length) {
      result.sort(function(a, b) {
        return a.confidence < b.confidence;
      });

      this.face_.targetX = result[0].x / this.input_.width;
      this.face_.targetY = result[0].y / this.input_.height;
      this.face_.targetWidth = result[0].width / this.input_.width;
      this.face_.targetHeight = result[0].height / this.input_.height;
      this.face_.targetConfidence = 1;
    } else {
      this.face_.targetConfidence = 0;
    }
    this.busy_ = false;
  }.bind(this));
};

/**
 * Updates the face by applying some interpolation.
 */
camera.Tracker.prototype.update = function() {
  this.face_.update();
};

