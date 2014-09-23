// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Loads a thumbnail using provided url. In CANVAS mode, loaded images
 * are attached as <canvas> element, while in IMAGE mode as <img>.
 * <canvas> renders faster than <img>, however has bigger memory overhead.
 *
 * @param {FileEntry} entry File entry.
 * @param {ThumbnailLoader.LoaderType=} opt_loaderType Canvas or Image loader,
 *     default: IMAGE.
 * @param {Object=} opt_metadata Metadata object.
 * @param {string=} opt_mediaType Media type.
 * @param {ThumbnailLoader.UseEmbedded=} opt_useEmbedded If to use embedded
 *     jpeg thumbnail if available. Default: USE_EMBEDDED.
 * @param {number=} opt_priority Priority, the highest is 0. default: 2.
 * @constructor
 */
function ThumbnailLoader(entry, opt_loaderType, opt_metadata, opt_mediaType,
    opt_useEmbedded, opt_priority) {
  opt_useEmbedded = opt_useEmbedded || ThumbnailLoader.UseEmbedded.USE_EMBEDDED;

  this.mediaType_ = opt_mediaType || FileType.getMediaType(entry);
  this.loaderType_ = opt_loaderType || ThumbnailLoader.LoaderType.IMAGE;
  this.metadata_ = opt_metadata;
  this.priority_ = (opt_priority !== undefined) ? opt_priority : 2;
  this.transform_ = null;

  if (!opt_metadata) {
    this.thumbnailUrl_ = entry.toURL();  // Use the URL directly.
    return;
  }

  this.fallbackUrl_ = null;
  this.thumbnailUrl_ = null;
  if (opt_metadata.external && opt_metadata.external.customIconUrl)
    this.fallbackUrl_ = opt_metadata.external.customIconUrl;

  // Fetch the rotation from the external properties (if available).
  var externalTransform;
  if (opt_metadata.external &&
      opt_metadata.external.imageRotation !== undefined) {
    externalTransform = {
      scaleX: 1,
      scaleY: 1,
      rotate90: opt_metadata.external.imageRotation / 90
    };
  }

  if (((opt_metadata.thumbnail && opt_metadata.thumbnail.url) ||
       (opt_metadata.external && opt_metadata.external.thumbnailUrl)) &&
      opt_useEmbedded === ThumbnailLoader.UseEmbedded.USE_EMBEDDED) {
    // If the thumbnail generated from the local cache (metadata.thumbnail.url)
    // is available, use it. If not, use the one passed from the external
    // provider (metadata.external.thumbnailUrl).
    this.thumbnailUrl_ =
        (opt_metadata.thumbnail && opt_metadata.thumbnail.url) ||
        (opt_metadata.external && opt_metadata.external.thumbnailUrl);
    this.transform_ = externalTransform !== undefined ? externalTransform :
        (opt_metadata.thumbnail && opt_metadata.thumbnail.transform);
  } else if (FileType.isImage(entry)) {
    this.thumbnailUrl_ = entry.toURL();
    this.transform_ = externalTransform !== undefined ? externalTransform :
        opt_metadata.media && opt_metadata.media.imageTransform;
  } else if (this.fallbackUrl_) {
    // Use fallback as the primary thumbnail.
    this.thumbnailUrl_ = this.fallbackUrl_;
    this.fallbackUrl_ = null;
  } // else the generic thumbnail based on the media type will be used.
}

/**
 * In percents (0.0 - 1.0), how much area can be cropped to fill an image
 * in a container, when loading a thumbnail in FillMode.AUTO mode.
 * The specified 30% value allows to fill 16:9, 3:2 pictures in 4:3 element.
 * @type {number}
 */
ThumbnailLoader.AUTO_FILL_THRESHOLD = 0.3;

/**
 * Type of displaying a thumbnail within a box.
 * @enum {number}
 */
ThumbnailLoader.FillMode = {
  FILL: 0,  // Fill whole box. Image may be cropped.
  FIT: 1,   // Keep aspect ratio, do not crop.
  OVER_FILL: 2,  // Fill whole box with possible stretching.
  AUTO: 3   // Try to fill, but if incompatible aspect ratio, then fit.
};

/**
 * Optimization mode for downloading thumbnails.
 * @enum {number}
 */
ThumbnailLoader.OptimizationMode = {
  NEVER_DISCARD: 0,    // Never discards downloading. No optimization.
  DISCARD_DETACHED: 1  // Canceled if the container is not attached anymore.
};

/**
 * Type of element to store the image.
 * @enum {number}
 */
ThumbnailLoader.LoaderType = {
  IMAGE: 0,
  CANVAS: 1
};

/**
 * Whether to use the embedded thumbnail, or not. The embedded thumbnail may
 * be small.
 * @enum {number}
 */
ThumbnailLoader.UseEmbedded = {
  USE_EMBEDDED: 0,
  NO_EMBEDDED: 1
};

/**
 * Maximum thumbnail's width when generating from the full resolution image.
 * @const
 * @type {number}
 */
ThumbnailLoader.THUMBNAIL_MAX_WIDTH = 500;

/**
 * Maximum thumbnail's height when generating from the full resolution image.
 * @const
 * @type {number}
 */
ThumbnailLoader.THUMBNAIL_MAX_HEIGHT = 500;

/**
 * Loads and attaches an image.
 *
 * @param {HTMLElement} box Container element.
 * @param {ThumbnailLoader.FillMode} fillMode Fill mode.
 * @param {ThumbnailLoader.OptimizationMode=} opt_optimizationMode Optimization
 *     for downloading thumbnails. By default optimizations are disabled.
 * @param {function(Image, Object)=} opt_onSuccess Success callback,
 *     accepts the image and the transform.
 * @param {function()=} opt_onError Error callback.
 * @param {function()=} opt_onGeneric Callback for generic image used.
 */
ThumbnailLoader.prototype.load = function(box, fillMode, opt_optimizationMode,
    opt_onSuccess, opt_onError, opt_onGeneric) {
  opt_optimizationMode = opt_optimizationMode ||
      ThumbnailLoader.OptimizationMode.NEVER_DISCARD;

  if (!this.thumbnailUrl_) {
    // Relevant CSS rules are in file_types.css.
    box.setAttribute('generic-thumbnail', this.mediaType_);
    if (opt_onGeneric) opt_onGeneric();
    return;
  }

  this.cancel();
  this.canvasUpToDate_ = false;
  this.image_ = new Image();
  this.image_.onload = function() {
    this.attachImage(box, fillMode);
    if (opt_onSuccess)
      opt_onSuccess(this.image_, this.transform_);
  }.bind(this);
  this.image_.onerror = function() {
    if (opt_onError)
      opt_onError();
    if (this.fallbackUrl_) {
      this.thumbnailUrl_ = this.fallbackUrl_;
      this.fallbackUrl_ = null;
      this.load(box, fillMode, opt_optimizationMode, opt_onSuccess);
    } else {
      box.setAttribute('generic-thumbnail', this.mediaType_);
    }
  }.bind(this);

  if (this.image_.src) {
    console.warn('Thumbnail already loaded: ' + this.thumbnailUrl_);
    return;
  }

  // TODO(mtomasz): Smarter calculation of the requested size.
  var wasAttached = box.ownerDocument.contains(box);
  var modificationTime = this.metadata_ &&
                         this.metadata_.filesystem &&
                         this.metadata_.filesystem.modificationTime &&
                         this.metadata_.filesystem.modificationTime.getTime();
  this.taskId_ = util.loadImage(
      this.image_,
      this.thumbnailUrl_,
      { maxWidth: ThumbnailLoader.THUMBNAIL_MAX_WIDTH,
        maxHeight: ThumbnailLoader.THUMBNAIL_MAX_HEIGHT,
        cache: true,
        priority: this.priority_,
        timestamp: modificationTime },
      function() {
        if (opt_optimizationMode ==
            ThumbnailLoader.OptimizationMode.DISCARD_DETACHED &&
            !box.ownerDocument.contains(box)) {
          // If the container is not attached, then invalidate the download.
          return false;
        }
        return true;
      });
};

/**
 * Cancels loading the current image.
 */
ThumbnailLoader.prototype.cancel = function() {
  if (this.taskId_) {
    this.image_.onload = function() {};
    this.image_.onerror = function() {};
    util.cancelLoadImage(this.taskId_);
    this.taskId_ = null;
  }
};

/**
 * @return {boolean} True if a valid image is loaded.
 */
ThumbnailLoader.prototype.hasValidImage = function() {
  return !!(this.image_ && this.image_.width && this.image_.height);
};

/**
 * @return {boolean} True if the image is rotated 90 degrees left or right.
 * @private
 */
ThumbnailLoader.prototype.isRotated_ = function() {
  return this.transform_ && (this.transform_.rotate90 % 2 === 1);
};

/**
 * @return {number} Image width (corrected for rotation).
 */
ThumbnailLoader.prototype.getWidth = function() {
  return this.isRotated_() ? this.image_.height : this.image_.width;
};

/**
 * @return {number} Image height (corrected for rotation).
 */
ThumbnailLoader.prototype.getHeight = function() {
  return this.isRotated_() ? this.image_.width : this.image_.height;
};

/**
 * Load an image but do not attach it.
 *
 * @param {function(boolean)} callback Callback, parameter is true if the image
 *     has loaded successfully or a stock icon has been used.
 */
ThumbnailLoader.prototype.loadDetachedImage = function(callback) {
  if (!this.thumbnailUrl_) {
    callback(true);
    return;
  }

  this.cancel();
  this.canvasUpToDate_ = false;
  this.image_ = new Image();
  this.image_.onload = callback.bind(null, true);
  this.image_.onerror = callback.bind(null, false);

  // TODO(mtomasz): Smarter calculation of the requested size.
  var modificationTime = this.metadata_ &&
                         this.metadata_.filesystem &&
                         this.metadata_.filesystem.modificationTime &&
                         this.metadata_.filesystem.modificationTime.getTime();
  this.taskId_ = util.loadImage(
      this.image_,
      this.thumbnailUrl_,
      { maxWidth: ThumbnailLoader.THUMBNAIL_MAX_WIDTH,
        maxHeight: ThumbnailLoader.THUMBNAIL_MAX_HEIGHT,
        cache: true,
        priority: this.priority_,
        timestamp: modificationTime });
};

/**
 * Renders the thumbnail into either canvas or an image element.
 * @private
 */
ThumbnailLoader.prototype.renderMedia_ = function() {
  if (this.loaderType_ !== ThumbnailLoader.LoaderType.CANVAS)
    return;

  if (!this.canvas_)
    this.canvas_ = document.createElement('canvas');

  // Copy the image to a canvas if the canvas is outdated.
  if (!this.canvasUpToDate_) {
    this.canvas_.width = this.image_.width;
    this.canvas_.height = this.image_.height;
    var context = this.canvas_.getContext('2d');
    context.drawImage(this.image_, 0, 0);
    this.canvasUpToDate_ = true;
  }
};

/**
 * Attach the image to a given element.
 * @param {Element} container Parent element.
 * @param {ThumbnailLoader.FillMode} fillMode Fill mode.
 */
ThumbnailLoader.prototype.attachImage = function(container, fillMode) {
  if (!this.hasValidImage()) {
    container.setAttribute('generic-thumbnail', this.mediaType_);
    return;
  }

  this.renderMedia_();
  util.applyTransform(container, this.transform_);
  var attachableMedia = this.loaderType_ === ThumbnailLoader.LoaderType.CANVAS ?
      this.canvas_ : this.image_;

  ThumbnailLoader.centerImage_(
      container, attachableMedia, fillMode, this.isRotated_());

  if (attachableMedia.parentNode !== container) {
    container.textContent = '';
    container.appendChild(attachableMedia);
  }

  if (!this.taskId_)
    attachableMedia.classList.add('cached');
};

/**
 * Gets the loaded image.
 * TODO(mtomasz): Apply transformations.
 *
 * @return {Image|HTMLCanvasElement} Either image or a canvas object.
 */
ThumbnailLoader.prototype.getImage = function() {
  this.renderMedia_();
  return this.loaderType_ === ThumbnailLoader.LoaderType.CANVAS ? this.canvas_ :
      this.image_;
};

/**
 * Update the image style to fit/fill the container.
 *
 * Using webkit center packing does not align the image properly, so we need
 * to wait until the image loads and its dimensions are known, then manually
 * position it at the center.
 *
 * @param {HTMLElement} box Containing element.
 * @param {Image|HTMLCanvasElement} img Element containing an image.
 * @param {ThumbnailLoader.FillMode} fillMode Fill mode.
 * @param {boolean} rotate True if the image should be rotated 90 degrees.
 * @private
 */
ThumbnailLoader.centerImage_ = function(box, img, fillMode, rotate) {
  var imageWidth = img.width;
  var imageHeight = img.height;

  var fractionX;
  var fractionY;

  var boxWidth = box.clientWidth;
  var boxHeight = box.clientHeight;

  var fill;
  switch (fillMode) {
    case ThumbnailLoader.FillMode.FILL:
    case ThumbnailLoader.FillMode.OVER_FILL:
      fill = true;
      break;
    case ThumbnailLoader.FillMode.FIT:
      fill = false;
      break;
    case ThumbnailLoader.FillMode.AUTO:
      var imageRatio = imageWidth / imageHeight;
      var boxRatio = 1.0;
      if (boxWidth && boxHeight)
        boxRatio = boxWidth / boxHeight;
      // Cropped area in percents.
      var ratioFactor = boxRatio / imageRatio;
      fill = (ratioFactor >= 1.0 - ThumbnailLoader.AUTO_FILL_THRESHOLD) &&
             (ratioFactor <= 1.0 + ThumbnailLoader.AUTO_FILL_THRESHOLD);
      break;
  }

  if (boxWidth && boxHeight) {
    // When we know the box size we can position the image correctly even
    // in a non-square box.
    var fitScaleX = (rotate ? boxHeight : boxWidth) / imageWidth;
    var fitScaleY = (rotate ? boxWidth : boxHeight) / imageHeight;

    var scale = fill ?
        Math.max(fitScaleX, fitScaleY) :
        Math.min(fitScaleX, fitScaleY);

    if (fillMode !== ThumbnailLoader.FillMode.OVER_FILL)
      scale = Math.min(scale, 1);  // Never overscale.

    fractionX = imageWidth * scale / boxWidth;
    fractionY = imageHeight * scale / boxHeight;
  } else {
    // We do not know the box size so we assume it is square.
    // Compute the image position based only on the image dimensions.
    // First try vertical fit or horizontal fill.
    fractionX = imageWidth / imageHeight;
    fractionY = 1;
    if ((fractionX < 1) === !!fill) {  // Vertical fill or horizontal fit.
      fractionY = 1 / fractionX;
      fractionX = 1;
    }
  }

  function percent(fraction) {
    return (fraction * 100).toFixed(2) + '%';
  }

  img.style.width = percent(fractionX);
  img.style.height = percent(fractionY);
  img.style.left = percent((1 - fractionX) / 2);
  img.style.top = percent((1 - fractionY) / 2);
};
