// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The base class for simple filters that only modify the image content
 * but do not modify the image dimensions.
 * @param {string} name
 * @param {string} title
 * @constructor
 * @struct
 * @extends {ImageEditor.Mode}
 */
ImageEditor.Mode.Adjust = function(name, title) {
  ImageEditor.Mode.call(this, name, title);

  /**
   * @type {boolean}
   * @const
   */
  this.implicitCommit = true;

  /**
   * @type {?string}
   * @private
   */
  this.doneMessage_ = null;

  /**
   * @type {number}
   * @private
   */
  this.viewportGeneration_ = 0;

  /**
   * @type {?string}
   * @private
   */
  this.filter_ = null;

  /**
   * @type {HTMLCanvasElement}
   * @private
   */
  this.canvas_ = null;

  /**
   * @type {ImageData}
   * @private
   */
  this.previewImageData_ = null;

  /**
   * @type {ImageData}
   */
  this.originalImageData = null;
};

ImageEditor.Mode.Adjust.prototype = {__proto__: ImageEditor.Mode.prototype};

/**
 * Gets command to do filter.
 *
 * @return {Command.Filter} Filter command.
 */
ImageEditor.Mode.Adjust.prototype.getCommand = function() {
  if (!this.filter_) return null;

  return new Command.Filter(this.name, this.filter_, this.doneMessage_);
};

/** @override */
ImageEditor.Mode.Adjust.prototype.cleanUpUI = function() {
  ImageEditor.Mode.prototype.cleanUpUI.apply(this, arguments);
  this.hidePreview();
};

/**
 * Hides preview.
 */
ImageEditor.Mode.Adjust.prototype.hidePreview = function() {
  if (this.canvas_) {
    this.canvas_.parentNode.removeChild(this.canvas_);
    this.canvas_ = null;
  }
};

/** @override */
ImageEditor.Mode.Adjust.prototype.cleanUpCaches = function() {
  this.filter_ = null;
  this.previewImageData_ = null;
};

/** @override */
ImageEditor.Mode.Adjust.prototype.reset = function() {
  ImageEditor.Mode.prototype.reset.call(this);
  this.hidePreview();
  this.cleanUpCaches();
};

/** @override */
ImageEditor.Mode.Adjust.prototype.update = function(options) {
  ImageEditor.Mode.prototype.update.apply(this, arguments);

  // We assume filter names are used in the UI directly.
  // This will have to change with i18n.
  this.filter_ = this.createFilter(options);

  this.updatePreviewImage();
  assert(this.previewImageData_);
  assert(this.originalImageData);

  ImageUtil.trace.resetTimer('preview');
  this.filter_(this.previewImageData_, this.originalImageData, 0, 0);
  ImageUtil.trace.reportTimer('preview');

  this.canvas_.getContext('2d').putImageData(
      this.previewImageData_, 0, 0);
};

/**
 * Copy the source image data for the preview.
 * Use the cached copy if the viewport has not changed.
 */
ImageEditor.Mode.Adjust.prototype.updatePreviewImage = function() {
  if (!this.previewImageData_ ||
      this.viewportGeneration_ != this.getViewport().getCacheGeneration()) {
    this.viewportGeneration_ = this.getViewport().getCacheGeneration();

    if (!this.canvas_) {
      this.canvas_ = this.getImageView().createOverlayCanvas();
    }

    this.getImageView().setupDeviceBuffer(this.canvas_);

    this.originalImageData = this.getImageView().copyScreenImageData();
    this.previewImageData_ = this.getImageView().copyScreenImageData();
  }
};

/*
 * Own methods
 */

/**
 * Creates a filter.
 * @param {!Object} options A map of filter-specific options.
 * @return {function(!ImageData,!ImageData,number,number)} Created function.
 */
ImageEditor.Mode.Adjust.prototype.createFilter = function(options) {
  return filter.create(this.name, options);
};

/**
 * A base class for color filters that are scale independent.
 * @constructor
 * @param {string} name The mode name.
 * @param {string} title The mode title.
 * @extends {ImageEditor.Mode.Adjust}
 * @struct
 */
ImageEditor.Mode.ColorFilter = function(name, title) {
  ImageEditor.Mode.Adjust.call(this, name, title);
};

ImageEditor.Mode.ColorFilter.prototype =
    {__proto__: ImageEditor.Mode.Adjust.prototype};

/**
 * Gets a histogram from a thumbnail.
 * @return {{r: !Array.<number>, g: !Array.<number>, b: !Array.<number>}}
 *    histogram.
 */
ImageEditor.Mode.ColorFilter.prototype.getHistogram = function() {
  return filter.getHistogram(this.getImageView().getThumbnail());
};

/**
 * Exposure/contrast filter.
 * @constructor
 * @extends {ImageEditor.Mode.ColorFilter}
 * @struct
 */
ImageEditor.Mode.Exposure = function() {
  ImageEditor.Mode.ColorFilter.call(this, 'exposure', 'GALLERY_EXPOSURE');
};

ImageEditor.Mode.Exposure.prototype =
    {__proto__: ImageEditor.Mode.ColorFilter.prototype};

/** @override */
ImageEditor.Mode.Exposure.prototype.createTools = function(toolbar) {
  toolbar.addRange('brightness', 'GALLERY_BRIGHTNESS', -1, 0, 1, 100);
  toolbar.addRange('contrast', 'GALLERY_CONTRAST', -1, 0, 1, 100);
};

/**
 * Autofix.
 * @constructor
 * @struct
 * @extends {ImageEditor.Mode.ColorFilter}
 */
ImageEditor.Mode.Autofix = function() {
  ImageEditor.Mode.ColorFilter.call(this, 'autofix', 'GALLERY_AUTOFIX');
  this.doneMessage_ = 'GALLERY_FIXED';
};

ImageEditor.Mode.Autofix.prototype =
    {__proto__: ImageEditor.Mode.ColorFilter.prototype};

/** @override */
ImageEditor.Mode.Autofix.prototype.createTools = function(toolbar) {
  var self = this;
  toolbar.addButton('Apply', 'Apply', this.apply.bind(this));
};

/** @override */
ImageEditor.Mode.Autofix.prototype.isApplicable = function() {
  return this.getImageView().hasValidImage() &&
      filter.autofix.isApplicable(this.getHistogram());
};

/**
 * Applies autofix.
 */
ImageEditor.Mode.Autofix.prototype.apply = function() {
  this.update({histogram: this.getHistogram()});
};

/**
 * Instant Autofix.
 * @constructor
 * @extends {ImageEditor.Mode.Autofix}
 * @struct
 */
ImageEditor.Mode.InstantAutofix = function() {
  ImageEditor.Mode.Autofix.call(this);
  this.instant = true;
};

ImageEditor.Mode.InstantAutofix.prototype =
    {__proto__: ImageEditor.Mode.Autofix.prototype};

/** @override */
ImageEditor.Mode.InstantAutofix.prototype.setUp = function() {
  ImageEditor.Mode.Autofix.prototype.setUp.apply(this, arguments);
  this.apply();
};

/**
 * Blur filter.
 * @constructor
 * @extends {ImageEditor.Mode.Adjust}
 * @struct
 */
ImageEditor.Mode.Blur = function() {
  ImageEditor.Mode.Adjust.call(this, 'blur', 'blur');
};

ImageEditor.Mode.Blur.prototype =
    {__proto__: ImageEditor.Mode.Adjust.prototype};

/** @override */
ImageEditor.Mode.Blur.prototype.createTools = function(toolbar) {
  toolbar.addRange('strength', 'GALLERY_STRENGTH', 0, 0, 1, 100);
  toolbar.addRange('radius', 'GALLERY_RADIUS', 1, 1, 3);
};

/**
 * Sharpen filter.
 * @constructor
 * @extends {ImageEditor.Mode.Adjust}
 * @struct
 */
ImageEditor.Mode.Sharpen = function() {
  ImageEditor.Mode.Adjust.call(this, 'sharpen', 'sharpen');
};

ImageEditor.Mode.Sharpen.prototype =
    {__proto__: ImageEditor.Mode.Adjust.prototype};

/** @override */
ImageEditor.Mode.Sharpen.prototype.createTools = function(toolbar) {
  toolbar.addRange('strength', 'GALLERY_STRENGTH', 0, 0, 1, 100);
  toolbar.addRange('radius', 'GALLERY_RADIUS', 1, 1, 3);
};
