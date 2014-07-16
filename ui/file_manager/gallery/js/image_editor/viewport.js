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

  this.offsetX_ = 0;
  this.offsetY_ = 0;

  this.generation_ = 0;

  this.update();
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
  this.invalidateCaches();
};

/**
 * @param {number} width Screen width.
 * @param {number} height Screen height.
 */
Viewport.prototype.setScreenSize = function(width, height) {
  this.screenBounds_ = new Rect(width, height);
  this.invalidateCaches();
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
};

/**
 * Returns the current zoom index.
 * @return {number} Zoon index.
 */
Viewport.prototype.getZoomIndex = function() {
  return this.zoomIndex_;
};

/**
 * Set the size by an HTML element.
 *
 * @param {HTMLElement} frame The element acting as the "screen".
 */
Viewport.prototype.sizeByFrame = function(frame) {
  this.setScreenSize(frame.clientWidth, frame.clientHeight);
};

/**
 * Set the size and scale to fit an HTML element.
 *
 * @param {HTMLElement} frame The element acting as the "screen".
 */
Viewport.prototype.sizeByFrameAndFit = function(frame) {
  var wasFitting = this.getScale() == this.getFittingScale();
  this.sizeByFrame(frame);
  var minScale = this.getFittingScale();
  if (wasFitting || (this.getScale() < minScale)) {
    this.setScale(minScale, true);
  }
};

/**
 * @return {number} Scale.
 */
Viewport.prototype.getScale = function() { return this.scale_; };

/**
 * @param {number} scale The new scale.
 * @param {boolean} notify True if the change should be reflected in the UI.
 */
Viewport.prototype.setScale = function(scale, notify) {
  if (this.scale_ == scale) return;
  this.scale_ = scale;
  this.invalidateCaches();
};

/**
 * @return {number} Best scale to fit the current image into the current screen.
 */
Viewport.prototype.getFittingScale = function() {
  return this.getFittingScaleForImageSize_(
      this.imageBounds_.width, this.imageBounds_.height);
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
 * Set the scale to fit the image into the screen.
 */
Viewport.prototype.fitImage = function() {
  var scale = this.getFittingScale();
  this.setScale(scale, true);
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
 * @param {boolean} ignoreClipping True if no clipping should be applied.
 */
Viewport.prototype.setOffset = function(x, y, ignoreClipping) {
  if (!ignoreClipping) {
    x = this.clampOffsetX_(x);
    y = this.clampOffsetY_(y);
  }
  if (this.offsetX_ == x && this.offsetY_ == y) return;
  this.offsetX_ = x;
  this.offsetY_ = y;
  this.invalidateCaches();
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
 * @return {Rect} The visible part of the image, in image coordinates.
 */
Viewport.prototype.getImageClipped = function() { return this.imageClipped_; };

/**
 * @return {Rect} The visible part of the image, in screen coordinates.
 */
Viewport.prototype.getScreenClipped = function() {
  return this.screenClipped_;
};

/**
 * A counter that is incremented with each viewport state change.
 * Clients that cache anything that depends on the viewport state should keep
 * track of this counter.
 * @return {number} counter.
 */
Viewport.prototype.getCacheGeneration = function() { return this.generation_; };

/**
 * Called on event view port state change.
 */
Viewport.prototype.invalidateCaches = function() { this.generation_++; };

/**
 * @return {Rect} The image bounds in screen coordinates.
 */
Viewport.prototype.getImageBoundsOnScreen = function() {
  return this.imageOnScreen_;
};

/**
 * @param {number} size Size in screen coordinates.
 * @return {number} Size in image coordinates.
 */
Viewport.prototype.screenToImageSize = function(size) {
  return size / this.getScale();
};

/**
 * @param {number} x X in screen coordinates.
 * @return {number} X in image coordinates.
 */
Viewport.prototype.screenToImageX = function(x) {
  return Math.round((x - this.imageOnScreen_.left) / this.getScale());
};

/**
 * @param {number} y Y in screen coordinates.
 * @return {number} Y in image coordinates.
 */
Viewport.prototype.screenToImageY = function(y) {
  return Math.round((y - this.imageOnScreen_.top) / this.getScale());
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
  return size * this.getScale();
};

/**
 * @param {number} x X in image coordinates.
 * @return {number} X in screen coordinates.
 */
Viewport.prototype.imageToScreenX = function(x) {
  return Math.round(this.imageOnScreen_.left + x * this.getScale());
};

/**
 * @param {number} y Y in image coordinates.
 * @return {number} Y in screen coordinates.
 */
Viewport.prototype.imageToScreenY = function(y) {
  return Math.round(this.imageOnScreen_.top + y * this.getScale());
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
 * Convert a rectangle from screen coordinates to 'device' coordinates.
 *
 * This conversion enlarges the original rectangle devicePixelRatio times
 * with the screen center as a fixed point.
 *
 * @param {Rect} rect Rectangle in screen coordinates.
 * @return {Rect} Rectangle in device coordinates.
 */
Viewport.prototype.screenToDeviceRect = function(rect) {
  var ratio = window.devicePixelRatio;
  var screenCenterX = Math.round(
      this.screenBounds_.left + this.screenBounds_.width / 2);
  var screenCenterY = Math.round(
      this.screenBounds_.top + this.screenBounds_.height / 2);
  return new Rect(screenCenterX + (rect.left - screenCenterX) * ratio,
                  screenCenterY + (rect.top - screenCenterY) * ratio,
                  rect.width * ratio,
                  rect.height * ratio);
};

/**
 * @return {Rect} The visible part of the image, in device coordinates.
 */
Viewport.prototype.getDeviceClipped = function() {
  return this.screenToDeviceRect(this.getScreenClipped());
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
  var limit = Math.round(Math.max(0, -this.getMarginX_() / this.getScale()));
  return ImageUtil.clamp(-limit, x, limit);
};

/**
 * @param {number} y Y-offset.
 * @return {number} Y-offset clamped to the valid range.
 * @private
 */
Viewport.prototype.clampOffsetY_ = function(y) {
  var limit = Math.round(Math.max(0, -this.getMarginY_() / this.getScale()));
  return ImageUtil.clamp(-limit, y, limit);
};

/**
 * Recalculate the viewport parameters.
 */
Viewport.prototype.update = function() {
  var scale = this.getScale();

  // Image bounds in screen coordinates.
  this.imageOnScreen_ = new Rect(
      this.getMarginX_(),
      this.getMarginY_(),
      Math.round(this.imageBounds_.width * scale),
      Math.round(this.imageBounds_.height * scale));

  // A visible part of the image in image coordinates.
  this.imageClipped_ = new Rect(this.imageBounds_);

  // A visible part of the image in screen coordinates.
  this.screenClipped_ = new Rect(this.screenBounds_);

  // Adjust for the offset.
  if (this.imageOnScreen_.left < 0) {
    this.imageOnScreen_.left +=
        Math.round(this.clampOffsetX_(this.offsetX_) * scale);
    this.imageClipped_.left = Math.round(-this.imageOnScreen_.left / scale);
    this.imageClipped_.width = Math.round(this.screenBounds_.width / scale);
  } else {
    this.screenClipped_.left = this.imageOnScreen_.left;
    this.screenClipped_.width = this.imageOnScreen_.width;
  }

  if (this.imageOnScreen_.top < 0) {
    this.imageOnScreen_.top +=
        Math.round(this.clampOffsetY_(this.offsetY_) * scale);
    this.imageClipped_.top = Math.round(-this.imageOnScreen_.top / scale);
    this.imageClipped_.height = Math.round(this.screenBounds_.height / scale);
  } else {
    this.screenClipped_.top = this.imageOnScreen_.top;
    this.screenClipped_.height = this.imageOnScreen_.height;
  }
};

/**
 * Obtains CSS transformation for the screen image.
 * @return {string} Transformation description.
 */
Viewport.prototype.getTransformation = function() {
  return 'scale(' + (1 / window.devicePixelRatio * this.zoom_) + ')';
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
  var scaleRatio = oldScale / this.getScale();
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
  var screenImageWidth = this.imageBounds_.width * this.getScale();
  var screenImageHeight = this.imageBounds_.height * this.getScale();
  var scaleX = screenRect.width / screenImageWidth;
  var scaleY = screenRect.height / screenImageHeight;
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
