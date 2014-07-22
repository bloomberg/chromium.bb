// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Viewport class controls the way the image is displayed (scale, offset etc).
 * @constructor
 */
function Viewport() {
  /**
   * Size of the full resolution image.
   * @type {Rect}
   * @private
   */
  this.imageBounds_ = new Rect();

  /**
   * Size of the application window.
   * @type {Rect}
   * @private
   */
  this.screenBounds_ = new Rect();

  /**
   * Bounds of the image element on screen without zoom and offset.
   * @type {Rect}
   * @private
   */
  this.imageElementBoundsOnScreen_ = null;

  /**
   * Bounds of the image with zoom and offset.
   * @type {Rect}
   * @private
   */
  this.imageBoundsOnScreen_ = null;

  /**
   * Image bounds that is clipped with the screen bounds.
   * @type {Rect}
   * @private
   */
  this.imageBoundsOnScreenClipped_ = null;

  /**
   * Scale from the full resolution image to the screen displayed image. This is
   * not zoom operated by users.
   * @type {number}
   * @private
   */
  this.scale_ = 1;

  /**
   * Index of zoom ratio. 0 is "zoom ratio = 1".
   * @type {number}
   * @private
   */
  this.zoomIndex_ = 0;

  /**
   * Zoom ratio specified by user operations.
   * @type {number}
   * @private
   */
  this.zoom_ = 1;

  /**
   * Offset specified by user operations.
   * @type {number}
   */
  this.offsetX_ = 0;

  /**
   * Offset specified by user operations.
   * @type {number}
   */
  this.offsetY_ = 0;

  /**
   * Generation of the screen size image cache.
   * This is incremented every time when the size of image cache is changed.
   * @type {number}
   * @private
   */
  this.generation_ = 0;

  this.update_();
  Object.seal(this);
}

/**
 * Zoom ratios.
 *
 * @type {Object.<string, number>}
 * @const
 */
Viewport.ZOOM_RATIOS = Object.freeze({
  '3': 3,
  '2': 2,
  '1': 1.5,
  '0': 1,
  '-1': 0.75,
  '-2': 0.5,
  '-3': 0.25
});

/**
 * @param {number} width Image width.
 * @param {number} height Image height.
 */
Viewport.prototype.setImageSize = function(width, height) {
  this.imageBounds_ = new Rect(width, height);
  this.update_();
};

/**
 * @param {number} width Screen width.
 * @param {number} height Screen height.
 */
Viewport.prototype.setScreenSize = function(width, height) {
  this.screenBounds_ = new Rect(width, height);
  this.update_();
};

/**
 * Sets the new zoom ratio.
 * @param {number} zoomIndex New zoom index.
 */
Viewport.prototype.setZoomIndex = function(zoomIndex) {
  // Ignore the invalid zoomIndex.
  if (!Viewport.ZOOM_RATIOS[zoomIndex.toString()])
    return;
  this.zoomIndex_ = zoomIndex;
  this.zoom_ = Viewport.ZOOM_RATIOS[zoomIndex];
  this.update_();
};

/**
 * Returns the current zoom index.
 * @return {number} Zoon index.
 */
Viewport.prototype.getZoomIndex = function() {
  return this.zoomIndex_;
};

/**
 * Returns the value of zoom.
 * @return {number} Zoom value.
 */
Viewport.prototype.getZoom = function() {
  return this.zoomIndex_;
};

/**
 * Obtains the scale for the specified image size.
 *
 * @param {number} width Width of the full resolution image.
 * @param {number} height Height of the full resolution image.
 * @return {number} The ratio of the fullresotion image size and the calculated
 * displayed image size.
 */
Viewport.prototype.getFittingScaleForImageSize_ = function(width, height) {
  var scaleX = this.screenBounds_.width / width;
  var scaleY = this.screenBounds_.height / height;
  // Scales > (1 / devicePixelRatio) do not look good. Also they are
  // not really useful as we do not have any pixel-level operations.
  return Math.min(1 / window.devicePixelRatio, scaleX, scaleY);
};

/**
 * @return {number} X-offset of the viewport.
 */
Viewport.prototype.getOffsetX = function() { return this.offsetX_; };

/**
 * @return {number} Y-offset of the viewport.
 */
Viewport.prototype.getOffsetY = function() { return this.offsetY_; };

/**
 * Set the image offset in the viewport.
 * @param {number} x X-offset.
 * @param {number} y Y-offset.
 */
Viewport.prototype.setOffset = function(x, y) {
  if (this.offsetX_ == x && this.offsetY_ == y)
    return;
  this.offsetX_ = x;
  this.offsetY_ = y;
  this.update_();
};

/**
 * @return {Rect} The image bounds in image coordinates.
 */
Viewport.prototype.getImageBounds = function() { return this.imageBounds_; };

/**
* @return {Rect} The screen bounds in screen coordinates.
*/
Viewport.prototype.getScreenBounds = function() { return this.screenBounds_; };

/**
 * @return {Rect} The size of screen cache canvas.
 */
Viewport.prototype.getDeviceBounds = function() {
  var size = this.getImageElementBoundsOnScreen();
  return new Rect(
      size.width * window.devicePixelRatio,
      size.height * window.devicePixelRatio);
};

/**
 * A counter that is incremented with each viewport state change.
 * Clients that cache anything that depends on the viewport state should keep
 * track of this counter.
 * @return {number} counter.
 */
Viewport.prototype.getCacheGeneration = function() { return this.generation_; };

/**
 * @return {Rect} The image bounds in screen coordinates.
 */
Viewport.prototype.getImageBoundsOnScreen = function() {
  return this.imageBoundsOnScreen_;
};

/**
 * The image bounds in screen coordinates.
 * This returns the bounds of element before applying zoom and offset.
 * @return {Rect}
 */
Viewport.prototype.getImageElementBoundsOnScreen = function() {
  return this.imageElementBoundsOnScreen_;
};

/**
 * The image bounds on screen, which is clipped with the screen size.
 * @return {Rect}
 */
Viewport.prototype.getImageBoundsOnScreenClipped = function() {
  return this.imageBoundsOnScreenClipped_;
};

/**
 * @param {number} size Size in screen coordinates.
 * @return {number} Size in image coordinates.
 */
Viewport.prototype.screenToImageSize = function(size) {
  return size / this.scale_;
};

/**
 * @param {number} x X in screen coordinates.
 * @return {number} X in image coordinates.
 */
Viewport.prototype.screenToImageX = function(x) {
  return Math.round((x - this.imageBoundsOnScreen_.left) / this.scale_);
};

/**
 * @param {number} y Y in screen coordinates.
 * @return {number} Y in image coordinates.
 */
Viewport.prototype.screenToImageY = function(y) {
  return Math.round((y - this.imageBoundsOnScreen_.top) / this.scale_);
};

/**
 * @param {Rect} rect Rectangle in screen coordinates.
 * @return {Rect} Rectangle in image coordinates.
 */
Viewport.prototype.screenToImageRect = function(rect) {
  return new Rect(
      this.screenToImageX(rect.left),
      this.screenToImageY(rect.top),
      this.screenToImageSize(rect.width),
      this.screenToImageSize(rect.height));
};

/**
 * @param {number} size Size in image coordinates.
 * @return {number} Size in screen coordinates.
 */
Viewport.prototype.imageToScreenSize = function(size) {
  return size * this.scale_;
};

/**
 * @param {number} x X in image coordinates.
 * @return {number} X in screen coordinates.
 */
Viewport.prototype.imageToScreenX = function(x) {
  return Math.round(this.imageBoundsOnScreen_.left + x * this.scale_);
};

/**
 * @param {number} y Y in image coordinates.
 * @return {number} Y in screen coordinates.
 */
Viewport.prototype.imageToScreenY = function(y) {
  return Math.round(this.imageBoundsOnScreen_.top + y * this.scale_);
};

/**
 * @param {Rect} rect Rectangle in image coordinates.
 * @return {Rect} Rectangle in screen coordinates.
 */
Viewport.prototype.imageToScreenRect = function(rect) {
  return new Rect(
      this.imageToScreenX(rect.left),
      this.imageToScreenY(rect.top),
      Math.round(this.imageToScreenSize(rect.width)),
      Math.round(this.imageToScreenSize(rect.height)));
};

/**
 * @return {boolean} True if some part of the image is clipped by the screen.
 */
Viewport.prototype.isClipped = function() {
  return this.getMarginX_() < 0 || this.getMarginY_() < 0;
};

/**
 * @return {number} Horizontal margin.
 *   Negative if the image is clipped horizontally.
 * @private
 */
Viewport.prototype.getMarginX_ = function() {
  return Math.round(
    (this.screenBounds_.width - this.imageBounds_.width * this.scale_) / 2);
};

/**
 * @return {number} Vertical margin.
 *   Negative if the image is clipped vertically.
 * @private
 */
Viewport.prototype.getMarginY_ = function() {
  return Math.round(
    (this.screenBounds_.height - this.imageBounds_.height * this.scale_) / 2);
};

/**
 * @param {number} x X-offset.
 * @return {number} X-offset clamped to the valid range.
 * @private
 */
Viewport.prototype.clampOffsetX_ = function(x) {
  var limit = Math.round(Math.max(0, -this.getMarginX_() / this.scale_));
  return ImageUtil.clamp(-limit, x, limit);
};

/**
 * @param {number} y Y-offset.
 * @return {number} Y-offset clamped to the valid range.
 * @private
 */
Viewport.prototype.clampOffsetY_ = function(y) {
  var limit = Math.round(Math.max(0, -this.getMarginY_() / this.scale_));
  return ImageUtil.clamp(-limit, y, limit);
};

/**
 * @private
 */
Viewport.prototype.getCenteredRect_ = function(
    width, height, offsetX, offsetY) {
  return new Rect(
      ~~((this.screenBounds_.width - width) / 2) + offsetX,
      ~~((this.screenBounds_.height - height) / 2) + offsetY,
      width,
      height);
};

/**
 * Resets zoom and offset.
 */
Viewport.prototype.resetView = function() {
  this.zoomIndex_ = 0;
  this.zoom_ = 1;
  this.offsetX_ = 0;
  this.offsetY_ = 0;
  this.update_();
};

/**
 * Recalculate the viewport parameters.
 * @private
 */
Viewport.prototype.update_ = function() {
  // Update scale.
  this.scale_ = this.getFittingScaleForImageSize_(
      this.imageBounds_.width, this.imageBounds_.height);

  // Limit offset values.
  var zoomedWidht = ~~(this.imageBounds_.width * this.scale_ * this.zoom_);
  var zoomedHeight = ~~(this.imageBounds_.height * this.scale_ * this.zoom_);
  var dx = Math.max(zoomedWidht - this.screenBounds_.width, 0) / 2;
  var dy = Math.max(zoomedHeight - this.screenBounds_.height, 0) /2;
  this.offsetX_ = ImageUtil.clamp(-dx, this.offsetX_, dx);
  this.offsetY_ = ImageUtil.clamp(-dy, this.offsetY_, dy);

  // Image bounds on screen.
  this.imageBoundsOnScreen_ = this.getCenteredRect_(
      zoomedWidht, zoomedHeight, this.offsetX_, this.offsetY_);

  // Image bounds of element (that is not applied zoom and offset) on screen.
  var oldBounds = this.imageElementBoundsOnScreen_;
  this.imageElementBoundsOnScreen_ = this.getCenteredRect_(
      ~~(this.imageBounds_.width * this.scale_),
      ~~(this.imageBounds_.height * this.scale_),
      0,
      0);
  if (!oldBounds ||
      this.imageElementBoundsOnScreen_.width != oldBounds.width ||
      this.imageElementBoundsOnScreen_.height != oldBounds.height) {
    this.generation_++;
  }

  // Image bounds on screen cliped with the screen bounds.
  var left = Math.max(this.imageBoundsOnScreen_.left, 0);
  var top = Math.max(this.imageBoundsOnScreen_.top, 0);
  var right = Math.min(
      this.imageBoundsOnScreen_.right, this.screenBounds_.width);
  var bottom = Math.min(
      this.imageBoundsOnScreen_.bottom, this.screenBounds_.height);
  this.imageBoundsOnScreenClipped_ = new Rect(
      left, top, right - left, bottom - top);
};

/**
 * Obtains CSS transformation for the screen image.
 * @return {string} Transformation description.
 */
Viewport.prototype.getTransformation = function() {
  return 'translate(' + this.offsetX_ + 'px, ' + this.offsetY_ + 'px) ' +
      'scale(' + this.zoom_ + ')';
};

/**
 * Obtains shift CSS transformation for the screen image.
 * @param {number} dx Amount of shift.
 * @return {string} Transformation description.
 */
Viewport.prototype.getShiftTransformation = function(dx) {
  return 'translateX(' + dx + 'px) ' + this.getTransformation();
};

/**
 * Obtains CSS transformation that makes the rotated image fit the original
 * image. The new rotated image that the transformation is applied to looks the
 * same with original image.
 *
 * @param {boolean} orientation Orientation of the rotation from the original
 *     image to the rotated image. True is for clockwise and false is for
 *     counterclockwise.
 * @return {string} Transformation description.
 */
Viewport.prototype.getInverseTransformForRotatedImage = function(orientation) {
  var previousImageWidth = this.imageBounds_.height;
  var previousImageHeight = this.imageBounds_.width;
  var oldScale = this.getFittingScaleForImageSize_(
      previousImageWidth, previousImageHeight);
  var scaleRatio = oldScale / this.scale_;
  var degree = orientation ? '-90deg' : '90deg';
  return [
    'scale(' + scaleRatio + ')',
    'rotate(' + degree + ')',
    this.getTransformation()
  ].join(' ');
};

/**
 * Obtains CSS transformation that makes the cropped image fit the original
 * image. The new cropped image that the transformaton is applied to fits to the
 * the cropped rectangle in the original image.
 *
 * @param {number} imageWidth Width of the original image.
 * @param {number} imageHeight Height of the origianl image.
 * @param {Rect} imageCropRect Crop rectangle in the image's coordinate system.
 * @return {string} Transformation description.
 */
Viewport.prototype.getInverseTransformForCroppedImage =
    function(imageWidth, imageHeight, imageCropRect) {
  var wholeScale = this.getFittingScaleForImageSize_(
      imageWidth, imageHeight);
  var croppedScale = this.getFittingScaleForImageSize_(
      imageCropRect.width, imageCropRect.height);
  var dx =
      (imageCropRect.left + imageCropRect.width / 2 - imageWidth / 2) *
      wholeScale;
  var dy =
      (imageCropRect.top + imageCropRect.height / 2 - imageHeight / 2) *
      wholeScale;
  return [
    'translate(' + dx + 'px,' + dy + 'px)',
    'scale(' + wholeScale / croppedScale + ')',
    this.getTransformation()
  ].join(' ');
};

/**
 * Obtains CSS transformaton that makes the image fit to the screen rectangle.
 *
 * @param {Rect} screenRect Screen rectangle.
 * @return {string} Transformation description.
 */
Viewport.prototype.getScreenRectTransformForImage = function(screenRect) {
  var imageBounds = this.getImageElementBoundsOnScreen();
  var scaleX = screenRect.width / imageBounds.width;
  var scaleY = screenRect.height / imageBounds.height;
  var screenWidth = this.screenBounds_.width;
  var screenHeight = this.screenBounds_.height;
  var dx = screenRect.left + screenRect.width / 2 - screenWidth / 2;
  var dy = screenRect.top + screenRect.height / 2 - screenHeight / 2;
  return [
    'translate(' + dx + 'px,' + dy + 'px)',
    'scale(' + scaleX + ',' + scaleY + ')',
    this.getTransformation()
  ].join(' ');
};
