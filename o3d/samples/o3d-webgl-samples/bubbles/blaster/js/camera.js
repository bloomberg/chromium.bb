/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * A camera represents the view of the player. This class has a dependency on
 * g_math.
 *
 * @param {number} fov
 * @param {number} distance
 * @param {o3d.DrawContext} context
 */
var Camera = function(fov, distance, context) {
  this.eye_ = [0, 0, distance, 1];
  this.fov_ = fov;
  this.target_ = [0, 0, 0, 1];
  this.up_ = [0, 1, 0, 0];
  this.context_ = context;
};

/**
 * The camera's eye (position).
 * @type {!Array.<number>}
 * @private
 */
Camera.prototype.eye_ = [0, 0, 0, 1];

/**
 * The camera's field of vision, in degrees.
 * @type {number}
 * @private
 */
Camera.prototype.fov_ = 45;

/**
 * The camera's target (lookAt).
 * @type {!Array.<number>}
 * @private
 */
Camera.prototype.target_ = [0, 0, 0, 1];

/**
 * The up vector.
 * @type {!Array.<number>}
 * @private
 */
Camera.prototype.up_ = [0, 0, 0, 0];

/**
 * The near plane.
 * @type {number}
 * @private
 */
Camera.prototype.nearPlane_ = 0.1;

/**
 * The far plane.
 * @type {number}
 * @private
 */
Camera.prototype.farPlane_ = 5000;

/**
 * The drawContext which this camera's position and perspective affects.
 * @type {o3d.DrawContext}
 * @private
 */
Camera.prototype.context_ = null;

/**
 * Returns the eye position of the camera as a 3-vector.
 * @type {!Array.<number>}
 */
Camera.prototype.__defineGetter__('eye',
    function() {
      return [this.eye_[0], this.eye_[1], this.eye_[2]];
    }
);

/**
 * Returns the up vector of the camera as a 3-vector.
 * @type {!Array.<number>}
 */
Camera.prototype.__defineGetter__('up',
    function() {
      return [this.up_[0], this.up_[1], this.up_[2]];
    }
);

/**
 * Returns the target/focal point of the camera as a 3-vector.
 * @type {!Array.<number>}
 */
Camera.prototype.__defineGetter__('target',
    function() {
      return [this.target_[0], this.target_[1], this.target_[2]];
    }
);

/**
 * Camera.Dir
 * UP,
 * DOWN,
 * LEFT,
 * RIGHT,
 */
Camera.UP = 0;
Camera.DOWN = 1;
Camera.LEFT = 2;
Camera.RIGHT = 3;

/**
 * Updates the projection matrix to correspond to the new width and height of
 * the canvas.
 * @param {number} width The width of the screen/canvas.
 * @param {number} height The height of the screen/canvas.
 */
Camera.prototype.updateProjection = function(width, height) {
  this.context_.projection = g_math.matrix4.perspective(
      g_math.degToRad(this.fov_),
      width / height,
      this.nearPlane_,
      this.farPlane_);
};


/**
 * Updates the view matrix using this Camera's eye, target, and up vectors.
 */
Camera.prototype.updateView = function() {
  this.context_.view = g_math.matrix4.lookAt(this.eye, this.target, this.up);
};

/**
 * Rotates the Camera by delta radians in the given direction and updates the
 * view.
 *
 * @param {Camera.Dir} direction
 * @param {number} delta
 */
Camera.prototype.rotate = function(direction, delta) {
  var axis;
  switch(direction) {
    case Camera.LEFT:
    case Camera.RIGHT:
      axis = this.up_;
      break;
    case Camera.UP:
    case Camera.DOWN:
      axis = g_math.cross(this.up_, g_math.negativeVector(this.eye));
      break;
  }
  switch(direction) {
    case Camera.LEFT:
    case Camera.DOWN:
      delta *= -1;
      break;
  }
  var matrix = g_math.matrix4.axisRotation(axis, delta);
  this.up_ = g_math.rowMajor.mulMatrixVector(matrix, this.up_);
  this.eye_ = g_math.rowMajor.mulMatrixVector(matrix, this.eye_);
  this.updateView();
};
