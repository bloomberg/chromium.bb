/*
 * Copyright 2009, Google Inc.
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
 * @fileoverview This file contains sample code for controlling the camera
 * (ie view matrix) using the mouse and keyboard.
 */

o3djs.provide('o3djs.cameracontroller');

o3djs.require('o3djs.math');

/**
 * A Module for user control of the camera / view matrix.
 * @namespace
 */
o3djs.cameracontroller = o3djs.cameracontroller || {};

/**
 * Creates a CameraController.
 * @param {!o3djs.math.Vector3} centerPos The position that the camera is
 *     looking at and rotating around; or if backpedal is zero, the location
 *     of the camera. In world space.
 * @param {number} backpedal The distance the camera moves back from the
 *     centerPos.
 * @param {number} heightAngle The angle the camera rotates up or down
 *     (about the x axis that passes through the centerPos). In radians.
 * @param {number} rotationAngle The angle the camera rotates left or right
 *     (about the y axis that passes through the centerPos). In radians.
 * @param {function(!o3djs.cameracontroller.CameraController): void}
 *     opt_onchange Pointer to a callback to call when the camera changes.
 * @return {!o3djs.cameracontroller.CameraController} The created
 *     CameraController.
 */
o3djs.cameracontroller.createCameraController = function(centerPos,
                                                         backpedal,
                                                         heightAngle,
                                                         rotationAngle,
                                                         opt_onchange) {
  return new o3djs.cameracontroller.CameraController(centerPos,
                                                     backpedal,
                                                     heightAngle,
                                                     rotationAngle,
                                                     opt_onchange);
};

/**
 * Class to hold user-controlled camera information and handle user events.
 * @constructor
 * @param {!o3djs.math.Vector3} centerPos The position that the camera is
 *     looking at and rotating around; or if backpedal is zero, the location
 *     of the camera. In world space.
 * @param {number} backpedal The distance the camera moves back from the
 *     centerPos.
 * @param {number} heightAngle The angle the camera rotates up or down
 *     (about the x axis that passes through the centerPos). In radians.
 * @param {number} rotationAngle The angle the camera rotates left or right
 *     (about the y axis that passes through the centerPos). In radians.
 * @param {function(!o3djs.cameracontroller.CameraController): void}
 *     opt_onchange Pointer to a callback to call when the camera changes.
 */
o3djs.cameracontroller.CameraController = function(centerPos,
                                                   backpedal,
                                                   heightAngle,
                                                   rotationAngle,
                                                   opt_onchange) {
  /**
   * The position that the camera is looking at and rotating around.
   * Or if backpedal is zero, the location of the camera. In world space.
   * @type {!o3djs.math.Vector3}
   */
  this.centerPos = centerPos;

  /**
   * The distance the camera moves back from the centerPos.
   * @type {number}
   */
  this.backpedal = backpedal;

  /**
   * The angle the camera rotates up or down.
   * @type {number}
   */
  this.heightAngle = heightAngle;

  /**
   * The angle the camera rotates left or right.
   * @type {number}
   */
  this.rotationAngle = rotationAngle;


  /**
   * Points to a callback to call when the camera changes.
   * @type {function(!o3djs.cameracontroller.CameraController): void}
   */
  this.onchange = opt_onchange || null;

  /**
   * Whether the mouse is currently being dragged to control the camera.
   * @private
   * @type {boolean}
   */
  this.dragging_ = false;

  /**
   * The last X coordinate of the mouse.
   * @private
   * @type {number}
   */
  this.mouseX_ = 0;

  /**
   * The last Y coordinate of the mouse.
   * @private
   * @type {number}
   */
  this.mouseY_ = 0;

  /**
   * Controls how quickly the mouse moves the camera.
   * @type {number}
   */
  this.dragScaleFactor = 300.0;
};

/**
 * Calculates the view matrix for this camera.
 * @return {!o3djs.math.Matrix4} The view matrix.
 */
o3djs.cameracontroller.CameraController.prototype.calculateViewMatrix = function() {
  var matrix4 = o3djs.math.matrix4;
  var view = matrix4.translation(o3djs.math.negativeVector(this.centerPos));
  view = matrix4.mul(view, matrix4.rotationY(this.rotationAngle));
  view = matrix4.mul(view, matrix4.rotationX(this.heightAngle));
  view = matrix4.mul(view, matrix4.translation([0, 0, -this.backpedal]));
  return view;
};

/**
 * Method which should be called by end user code upon receiving a
 * mouse-down event.
 * @param {!o3d.Event} ev The event.
 */
o3djs.cameracontroller.CameraController.prototype.mousedown = function(ev) {
  this.dragging_ = true;
  this.mouseX_ = ev.x;
  this.mouseY_ = ev.y;
};

/**
 * Method which should be called by end user code upon receiving a
 * mouse-up event.
 * @param {!o3d.Event} ev The event.
 */
o3djs.cameracontroller.CameraController.prototype.mouseup = function(ev) {
  this.dragging_ = false;
};

/**
 * Method which should be called by end user code upon receiving a
 * mouse-move event.
 * @param {!o3d.Event} ev The event.
 */
o3djs.cameracontroller.CameraController.prototype.mousemove = function(ev) {
  if (this.dragging_) {
    var curX = ev.x;
    var curY = ev.y;
    var deltaX = (curX - this.mouseX_) / this.dragScaleFactor;
    var deltaY = (curY - this.mouseY_) / this.dragScaleFactor;
    this.mouseX_ = curX;
    this.mouseY_ = curY;
    this.rotationAngle += deltaX;
    this.heightAngle += deltaY;
    if (this.onchange != null) {
      this.onchange(this);
    }
  }
};
