// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class representing image orientation.
 * The constructor takes 2x2 matrix value that cancels the image orientation:
 * |a, c|
 * |b, d|
 * @param {number} a
 * @param {number} b
 * @param {number} c
 * @param {number} d
 * @struct
 * @constructor
 */
function ImageOrientation(a, b, c, d) {
  /**
   * @public {number}
   * @const
   */
  this.a = a;

  /**
   * @public {number}
   * @const
   */
  this.b = b;

  /**
   * @public {number}
   * @const
   */
  this.c = c;

  /**
   * @public {number}
   * @const
   */
  this.d = d;
}

/**
 * @param {number} orientation 1-based orientation number defined by EXIF.
 * @return {!ImageOrientation}
 */
ImageOrientation.fromExifOrientation = function(orientation) {
  switch (~~orientation) {
    case 1:
      return new ImageOrientation(1, 0, 0, 1);
    case 2:
      return new ImageOrientation(-1, 0, 0, 1);
    case 3:
      return new ImageOrientation(-1, 0, 0, -1);
    case 4:
      return new ImageOrientation(1, 0, 0, -1);
    case 5:
      return new ImageOrientation(0, 1, 1, 0);
    case 6:
      return new ImageOrientation(0, 1, -1, 0);
    case 7:
      return new ImageOrientation(0, -1, -1, 0);
    case 8:
      return new ImageOrientation(0, -1, 1, 0);
    default:
      console.error('Invalid orientation number.');
      return new ImageOrientation(1, 0, 0, 1);
  }
};

/**
 * @param {number} rotation90 Clockwise degrees / 90.
 * @return {!ImageOrientation}
 */
ImageOrientation.fromDriveOrientation = function(rotation90) {
  switch (~~(rotation90 % 4)) {
    case 0:
      return new ImageOrientation(1, 0, 0, 1);
    case 1:
      return new ImageOrientation(0, -1, 1, 0);
    case 2:
      return new ImageOrientation(-1, 0, 0, -1);
    case 3:
      return new ImageOrientation(0, 1, -1, 0);
    default:
      console.error('Invalid orientation number.');
      return new ImageOrientation(1, 0, 0, 1);
  }
};

/**
 * Obtains the image size after cancelling its orientation.
 * @param {number} imageWidth
 * @param {number} imageHeight
 * @return {{width:number, height:number}}
 */
ImageOrientation.prototype.getSizeAfterCancelling = function(
    imageWidth, imageHeight) {
  var projectedX = this.a * imageWidth + this.c * imageHeight;
  var projectedY = this.b * imageWidth + this.d * imageHeight;
  return {
    width: Math.abs(projectedX),
    height: Math.abs(projectedY)
  };
};

/**
 * Applies the transformation that cancels the image orientation to the given
 * context.
 * @param {!CanvasRenderingContext2D} context
 * @param {number} imageWidth
 * @param {number} imageHeight
 */
ImageOrientation.prototype.cancelImageOrientation = function(
    context, imageWidth, imageHeight) {
  // Calculate where to project the point of (imageWidth, imageHeight).
  var projectedX = this.a * imageWidth + this.c * imageHeight;
  var projectedY = this.b * imageWidth + this.d * imageHeight;

  // If the projected point coordinates are negative, add offset to cancel it.
  var offsetX = projectedX < 0 ? -projectedX : 0;
  var offsetY = projectedY < 0 ? -projectedY : 0;

  // Apply the transform.
  context.setTransform(this.a, this.b, this.c, this.d, offsetX, offsetY);
};

/**
 * Checks if the orientation represents identity transformation or not.
 * @return {boolean}
 */
ImageOrientation.prototype.isIdentity = function() {
  return this.a === 1 && this.b === 0 && this.c === 0 && this.d === 1;
};
