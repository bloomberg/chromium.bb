// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The overlay displaying the image.
 *
 * @param {!HTMLElement} container The container element.
 * @param {!Viewport} viewport The viewport.
 * @param {!MetadataModel} metadataModel
 * @constructor
 * @extends {ImageBuffer.Overlay}
 * @struct
 */
function ImageView(container, viewport, metadataModel) {
  ImageBuffer.Overlay.call(this);

  this.container_ = container;

  /**
   * The viewport.
   * @type {!Viewport}
   * @private
   */
  this.viewport_ = viewport;

  this.document_ = assertInstanceof(container.ownerDocument, HTMLDocument);
  this.contentGeneration_ = 0;
  this.displayedContentGeneration_ = 0;

  this.imageLoader_ =
      new ImageUtil.ImageLoader(this.document_, metadataModel);
  // We have a separate image loader for prefetch which does not get cancelled
  // when the selection changes.
  this.prefetchLoader_ =
      new ImageUtil.ImageLoader(this.document_, metadataModel);

  this.contentCallbacks_ = [];

  /**
   * The element displaying the current content.
   * @type {HTMLCanvasElement}
   * @private
   */
  this.screenImage_ = null;

  /**
   * The content canvas element.
   * @type {(HTMLCanvasElement|HTMLImageElement)}
   * @private
   */
  this.contentCanvas_ = null;

  /**
   * True if the image is a preview (not full res).
   * @type {boolean}
   * @private
   */
  this.preview_ = false;

  /**
   * Cached thumbnail image.
   * @type {HTMLCanvasElement}
   * @private
   */
  this.thumbnailCanvas_ = null;

  /**
   * The content revision number.
   * @type {number}
   * @private
   */
  this.contentRevision_ = -1;

  /**
   * The last load time.
   * @type {?number}
   * @private
   */
  this.lastLoadTime_ = null;

  /**
   * Gallery item which is loaded.
   * @type {GalleryItem}
   * @private
   */
  this.contentItem_ = null;

  /**
   * Timer to unload.
   * @type {?number}
   * @private
   */
  this.unloadTimer_ = null;
}

/**
 * Duration of transition between modes in ms.
 * @type {number}
 * @const
 */
ImageView.MODE_TRANSITION_DURATION = 350;

/**
 * If the user flips though images faster than this interval we do not apply
 * the slide-in/slide-out transition.
 * @type {number}
 * @const
 */
ImageView.FAST_SCROLL_INTERVAL = 300;


/**
 * Image load types.
 * @enum {number}
 */
ImageView.LoadType = {
  // Full resolution image loaded from cache.
  CACHED_FULL: 0,
  // Screen resolution preview loaded from cache.
  CACHED_SCREEN: 1,
  // Image read from file.
  IMAGE_FILE: 2,
  // Error occurred.
  ERROR: 3,
  // The file contents is not available offline.
  OFFLINE: 4
};

/**
 * Target of image load.
 * @enum {string}
 */
ImageView.LoadTarget = {
  CACHED_MAIN_IMAGE: 'cachedMainImage',
  CACHED_THUMBNAIL: 'cachedThumbnail',
  THUMBNAIL: 'thumbnail',
  MAIN_IMAGE: 'mainImage'
};

/**
 * Obtains prefered load type from GalleryItem.
 *
 * @param {!GalleryItem} item
 * @param {!ImageView.Effect} effect
 * @return {ImageView.LoadTarget} Load target.
 */
ImageView.getLoadTarget = function(item, effect) {
  if (item.contentImage)
    return ImageView.LoadTarget.CACHED_MAIN_IMAGE;
  if (item.screenImage)
    return ImageView.LoadTarget.CACHED_THUMBNAIL;

  // Only show thumbnails if there is no effect or the effect is Slide.
  var thumbnailLoader = new ThumbnailLoader(
      item.getEntry(),
      ThumbnailLoader.LoaderType.CANVAS,
      item.getThumbnailMetadataItem());
  if ((effect instanceof ImageView.Effect.None ||
       effect instanceof ImageView.Effect.Slide) &&
      thumbnailLoader.getLoadTarget() !==
      ThumbnailLoader.LoadTarget.FILE_ENTRY) {
    return ImageView.LoadTarget.THUMBNAIL;
  }

  return ImageView.LoadTarget.MAIN_IMAGE;
};

ImageView.prototype = {__proto__: ImageBuffer.Overlay.prototype};

/**
 * @override
 */
ImageView.prototype.getZIndex = function() { return -1; };

/**
 * @override
 */
ImageView.prototype.draw = function() {
  if (!this.contentCanvas_)  // Do nothing if the image content is not set.
    return;
  this.setTransform_(
      this.contentCanvas_,
      this.viewport_,
      new ImageView.Effect.None(),
      ImageView.Effect.DEFAULT_DURATION);
  if ((this.screenImage_ && this.setupDeviceBuffer(this.screenImage_)) ||
      this.displayedContentGeneration_ !== this.contentGeneration_) {
    this.displayedContentGeneration_ = this.contentGeneration_;
    ImageUtil.trace.resetTimer('paint');
    this.paintDeviceRect(
        this.contentCanvas_, ImageRect.createFromImage(this.contentCanvas_));
    ImageUtil.trace.reportTimer('paint');
  }
};

/**
 * Applies the viewport change that does not affect the screen cache size (zoom
 * change or offset change) with animation.
 */
ImageView.prototype.applyViewportChange = function() {
  var zooming = this.viewport_.getZoom() > 1;
  if (this.contentCanvas_) {
    // Show full resolution image only for zooming.
    this.contentCanvas_.style.opacity = zooming ? '1' : '0';
    this.setTransform_(
        this.contentCanvas_,
        this.viewport_,
        new ImageView.Effect.None(),
        ImageView.Effect.DEFAULT_DURATION);
  }
  if (this.screenImage_) {
      this.setTransform_(
          this.screenImage_,
          this.viewport_,
          new ImageView.Effect.None(),
          ImageView.Effect.DEFAULT_DURATION);
  }
};

/**
 * @return {number} The cache generation.
 */
ImageView.prototype.getCacheGeneration = function() {
  return this.contentGeneration_;
};

/**
 * Invalidates the caches to force redrawing the screen canvas.
 */
ImageView.prototype.invalidateCaches = function() {
  this.contentGeneration_++;
};

/**
 * @return {HTMLCanvasElement} The content canvas element.
 */
ImageView.prototype.getCanvas = function() { return this.contentCanvas_; };

/**
 * @return {boolean} True if the a valid image is currently loaded.
 */
ImageView.prototype.hasValidImage = function() {
  return !!(!this.preview_ && this.contentCanvas_ && this.contentCanvas_.width);
};

/**
 * @return {!HTMLCanvasElement} The cached thumbnail image.
 */
ImageView.prototype.getThumbnail = function() {
  assert(this.thumbnailCanvas_);
  return this.thumbnailCanvas_;
};

/**
 * @return {number} The content revision number.
 */
ImageView.prototype.getContentRevision = function() {
  return this.contentRevision_;
};

/**
 * Copies an image fragment from a full resolution canvas to a device resolution
 * canvas.
 *
 * @param {!HTMLCanvasElement} canvas Canvas containing whole image. The canvas
 *     may not be full resolution (scaled).
 * @param {!ImageRect} imageRect Rectangle region of the canvas to be rendered.
 */
ImageView.prototype.paintDeviceRect = function(canvas, imageRect) {
  // Map the rectangle in full resolution image to the rectangle in the device
  // canvas.
  var deviceBounds = this.viewport_.getDeviceBounds();
  var scaleX = deviceBounds.width / canvas.width;
  var scaleY = deviceBounds.height / canvas.height;
  var deviceRect = new ImageRect(
      imageRect.left * scaleX,
      imageRect.top * scaleY,
      imageRect.width * scaleX,
      imageRect.height * scaleY);

  ImageRect.drawImage(
      this.screenImage_.getContext('2d'), canvas, deviceRect, imageRect);
};

/**
 * Creates an overlay canvas with properties similar to the screen canvas.
 * Useful for showing quick feedback when editing.
 *
 * @return {!HTMLCanvasElement} Overlay canvas.
 */
ImageView.prototype.createOverlayCanvas = function() {
  var canvas = assertInstanceof(this.document_.createElement('canvas'),
      HTMLCanvasElement);
  canvas.className = 'image';
  this.container_.appendChild(canvas);
  return canvas;
};

/**
 * Sets up the canvas as a buffer in the device resolution.
 *
 * @param {!HTMLCanvasElement} canvas The buffer canvas.
 * @return {boolean} True if the canvas needs to be rendered.
 */
ImageView.prototype.setupDeviceBuffer = function(canvas) {
  // Set the canvas position and size in device pixels.
  var deviceRect = this.viewport_.getDeviceBounds();
  var needRepaint = false;
  if (canvas.width !== deviceRect.width) {
    canvas.width = deviceRect.width;
    needRepaint = true;
  }
  if (canvas.height !== deviceRect.height) {
    canvas.height = deviceRect.height;
    needRepaint = true;
  }
  this.setTransform_(canvas, this.viewport_);
  return needRepaint;
};

/**
 * Gets screen image data with specified size.
 * @param {number} width
 * @param {number} height
 * @return {!ImageData} A new ImageData object.
 */
ImageView.prototype.getScreenImageDataWith = function(width, height) {
  // If specified size is same with current screen image size, just return it.
  if (width === this.screenImage_.width &&
      height === this.screenImage_.height) {
    return this.screenImage_.getContext('2d').getImageData(
        0, 0, this.screenImage_.width, this.screenImage_.height);
  }

  // Resize if these sizes are different.
  var resizeCanvas = document.createElement('canvas');
  resizeCanvas.width = width;
  resizeCanvas.height = height;

  var context = resizeCanvas.getContext('2d');
  context.drawImage(this.screenImage_,
      0, 0, this.screenImage_.width, this.screenImage_.height,
      0, 0, resizeCanvas.width, resizeCanvas.height);
  return context.getImageData(0, 0, resizeCanvas.width, resizeCanvas.height);
};

/**
 * @return {boolean} True if the image is currently being loaded.
 */
ImageView.prototype.isLoading = function() {
  return this.imageLoader_.isBusy();
};

/**
 * Cancels the current image loading operation. The callbacks will be ignored.
 */
ImageView.prototype.cancelLoad = function() {
  this.imageLoader_.cancel();
};

/**
 * Loads and display a new image.
 *
 * Loads the thumbnail first, then replaces it with the main image.
 * Takes into account the image orientation encoded in the metadata.
 *
 * @param {!GalleryItem} item Gallery item to be loaded.
 * @param {!ImageView.Effect} effect Transition effect object.
 * @param {function()} displayCallback Called when the image is displayed
 *     (possibly as a preview).
 * @param {function(!ImageView.LoadType, number, *=)} loadCallback Called when
 *     the image is fully loaded. The first parameter is the load type.
 */
ImageView.prototype.load =
    function(item, effect, displayCallback, loadCallback) {
  var entry = item.getEntry();

  if (!(effect instanceof ImageView.Effect.None)) {
    // Skip effects when reloading repeatedly very quickly.
    var time = Date.now();
    if (this.lastLoadTime_ &&
        (time - this.lastLoadTime_) < ImageView.FAST_SCROLL_INTERVAL) {
      effect = new ImageView.Effect.None();
    }
    this.lastLoadTime_ = time;
  }

  ImageUtil.metrics.startInterval(ImageUtil.getMetricName('DisplayTime'));

  var self = this;

  this.contentItem_ = item;
  this.contentRevision_ = -1;

  switch (ImageView.getLoadTarget(item, effect)) {
    case ImageView.LoadTarget.CACHED_MAIN_IMAGE:
      displayMainImage(
          ImageView.LoadType.CACHED_FULL,
          false /* no preview */,
          assert(item.contentImage));
      break;

    case ImageView.LoadTarget.CACHED_THUMBNAIL:
      // We have a cached screen-scale canvas, use it instead of a thumbnail.
      displayThumbnail(ImageView.LoadType.CACHED_SCREEN, item.screenImage);
      // As far as the user can tell the image is loaded. We still need to load
      // the full res image to make editing possible, but we can report now.
      ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('DisplayTime'));
      break;

    case ImageView.LoadTarget.THUMBNAIL:
      var thumbnailLoader = new ThumbnailLoader(
          entry,
          ThumbnailLoader.LoaderType.CANVAS,
          item.getThumbnailMetadataItem());
      thumbnailLoader.loadDetachedImage(function(success) {
        displayThumbnail(
            ImageView.LoadType.IMAGE_FILE,
            success ? thumbnailLoader.getImage() : null);
      });
      break;

    case ImageView.LoadTarget.MAIN_IMAGE:
      loadMainImage(
          ImageView.LoadType.IMAGE_FILE,
          entry,
          false /* no preview*/,
          0 /* delay */);
      break;

    default:
      assertNotReached();
  }

  /**
   * @param {!ImageView.LoadType} loadType A load type.
   * @param {(HTMLCanvasElement|HTMLImageElement)} canvas A canvas.
   */
  function displayThumbnail(loadType, canvas) {
    if (canvas) {
      var width = item.getMetadataItem().imageWidth;
      var height = item.getMetadataItem().imageHeight;
      self.replace(
          canvas,
          effect,
          width,
          height,
          true /* preview */);
      if (displayCallback)
        displayCallback();
    }
    loadMainImage(loadType, entry, !!canvas,
        (effect && canvas) ? effect.getSafeInterval() : 0);
  }

  /**
   * @param {!ImageView.LoadType} loadType Load type.
   * @param {Entry} contentEntry A content entry.
   * @param {boolean} previewShown A preview is shown or not.
   * @param {number} delay Load delay.
   */
  function loadMainImage(loadType, contentEntry, previewShown, delay) {
    if (self.prefetchLoader_.isLoading(contentEntry)) {
      // The image we need is already being prefetched. Initiating another load
      // would be a waste. Hijack the load instead by overriding the callback.
      self.prefetchLoader_.setCallback(
          displayMainImage.bind(null, loadType, previewShown));

      // Swap the loaders so that the self.isLoading works correctly.
      var temp = self.prefetchLoader_;
      self.prefetchLoader_ = self.imageLoader_;
      self.imageLoader_ = temp;
      return;
    }
    self.prefetchLoader_.cancel();  // The prefetch was doing something useless.

    self.imageLoader_.load(
        item,
        displayMainImage.bind(null, loadType, previewShown),
        delay);
  }

  /**
   * @param {!ImageView.LoadType} loadType Load type.
   * @param {boolean} previewShown A preview is shown or not.
   * @param {!(HTMLCanvasElement|HTMLImageElement)} content A content.
   * @param {string=} opt_error Error message.
   */
  function displayMainImage(loadType, previewShown, content, opt_error) {
    if (opt_error)
      loadType = ImageView.LoadType.ERROR;

    // If we already displayed the preview we should not replace the content if
    // the full content failed to load.
    var animationDuration = 0;
    if (!(previewShown && loadType === ImageView.LoadType.ERROR)) {
      var replaceEffect = previewShown ? null : effect;
      animationDuration = replaceEffect ? replaceEffect.getSafeInterval() : 0;
      self.replace(content, replaceEffect);
      if (!previewShown && displayCallback) displayCallback();
    }

    if (loadType !== ImageView.LoadType.ERROR &&
        loadType !== ImageView.LoadType.CACHED_SCREEN) {
      ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('DisplayTime'));
    }
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('LoadMode'),
        loadType, Object.keys(ImageView.LoadType).length);

    if (loadType === ImageView.LoadType.ERROR &&
        !navigator.onLine && !item.getMetadataItem().present) {
      loadType = ImageView.LoadType.OFFLINE;
    }
    if (loadCallback) loadCallback(loadType, animationDuration, opt_error);
  }
};

/**
 * Prefetches an image.
 * @param {!GalleryItem} item The image item.
 * @param {number=} opt_delay Image load delay in ms.
 */
ImageView.prototype.prefetch = function(item, opt_delay) {
  if (item.contentImage || this.prefetchLoader_.isLoading(item.getEntry()))
    return;
  this.prefetchLoader_.load(item, function(canvas) {
    if (canvas.width && canvas.height && !item.contentImage)
      item.contentImage = canvas;
  }, opt_delay);
};

/**
 * Unloads content.
 * @param {ImageRect=} opt_zoomToRect Target rectangle for zoom-out-effect.
 */
ImageView.prototype.unload = function(opt_zoomToRect) {
  if (this.unloadTimer_) {
    clearTimeout(this.unloadTimer_);
    this.unloadTimer_ = null;
  }
  if (opt_zoomToRect && this.screenImage_) {
    var effect = this.createZoomEffect(opt_zoomToRect);
    this.setTransform_(this.screenImage_, this.viewport_, effect);
    this.screenImage_.setAttribute('fade', true);
    this.unloadTimer_ = setTimeout(function() {
      this.unloadTimer_ = null;
      this.unload(null /* force unload */);
    }.bind(this), effect.getSafeInterval());
    return;
  }
  this.container_.textContent = '';
  this.contentCanvas_ = null;
  this.screenImage_ = null;
};

/**
 * @param {!(HTMLCanvasElement|HTMLImageElement)} content The image element.
 * @param {number=} opt_width Image width.
 * @param {number=} opt_height Image height.
 * @param {boolean=} opt_preview True if the image is a preview (not full res).
 * @private
 */
ImageView.prototype.replaceContent_ = function(
    content, opt_width, opt_height, opt_preview) {

  if (this.contentCanvas_ && this.contentCanvas_.parentNode === this.container_)
    this.container_.removeChild(this.contentCanvas_);

  this.screenImage_ = assertInstanceof(this.document_.createElement('canvas'),
      HTMLCanvasElement);
  this.screenImage_.className = 'image';

  this.contentCanvas_ = content;
  this.invalidateCaches();
  this.viewport_.setImageSize(
      opt_width || this.contentCanvas_.width,
      opt_height || this.contentCanvas_.height);
  this.draw();

  this.container_.appendChild(this.screenImage_);

  this.preview_ = opt_preview || false;
  // If this is not a thumbnail, cache the content and the screen-scale image.
  if (this.hasValidImage()) {
    // Insert the full resolution canvas into DOM so that it can be printed.
    this.container_.appendChild(this.contentCanvas_);
    this.contentCanvas_.classList.add('fullres');
    this.setTransform_(
        this.contentCanvas_, this.viewport_, null, 0);

    this.contentItem_.contentImage = this.contentCanvas_;
    this.contentItem_.screenImage = this.screenImage_;

    // TODO(kaznacheev): It is better to pass screenImage_ as it is usually
    // much smaller than contentCanvas_ and still contains the entire image.
    // Once we implement zoom/pan we should pass contentCanvas_ instead.
    this.updateThumbnail_(this.screenImage_);

    this.contentRevision_++;
    for (var i = 0; i !== this.contentCallbacks_.length; i++) {
      try {
        this.contentCallbacks_[i]();
      } catch (e) {
        console.error(e);
      }
    }
  }
};

/**
 * Adds a listener for content changes.
 * @param {function()} callback Callback.
 */
ImageView.prototype.addContentCallback = function(callback) {
  this.contentCallbacks_.push(callback);
};

/**
 * Updates the cached thumbnail image.
 *
 * @param {!HTMLCanvasElement} canvas The source canvas.
 * @private
 */
ImageView.prototype.updateThumbnail_ = function(canvas) {
  ImageUtil.trace.resetTimer('thumb');
  var pixelCount = 10000;
  var downScale =
      Math.max(1, Math.sqrt(canvas.width * canvas.height / pixelCount));

  this.thumbnailCanvas_ = canvas.ownerDocument.createElement('canvas');
  this.thumbnailCanvas_.width = Math.round(canvas.width / downScale);
  this.thumbnailCanvas_.height = Math.round(canvas.height / downScale);
  ImageRect.drawImage(this.thumbnailCanvas_.getContext('2d'), canvas);
  ImageUtil.trace.reportTimer('thumb');
};

/**
 * Replaces the displayed image, possibly with slide-in animation.
 *
 * @param {!(HTMLCanvasElement|HTMLImageElement)} content The image element.
 * @param {ImageView.Effect=} opt_effect Transition effect object.
 * @param {number=} opt_width Image width.
 * @param {number=} opt_height Image height.
 * @param {boolean=} opt_preview True if the image is a preview (not full res).
 */
ImageView.prototype.replace = function(
    content, opt_effect, opt_width, opt_height, opt_preview) {
  var oldScreenImage = this.screenImage_;
  var oldViewport = this.viewport_.clone();
  this.replaceContent_(content, opt_width, opt_height, opt_preview);
  if (!opt_effect) {
    if (oldScreenImage)
      oldScreenImage.parentNode.removeChild(oldScreenImage);
    return;
  }

  assert(this.screenImage_);
  var newScreenImage = this.screenImage_;
  this.viewport_.resetView();

  if (oldScreenImage)
    ImageUtil.setAttribute(newScreenImage, 'fade', true);
  this.setTransform_(
      newScreenImage, this.viewport_, opt_effect, 0 /* instant */);
  this.setTransform_(
      content, this.viewport_, opt_effect, 0 /* instant */);

  // We need to call requestAnimationFrame twice here. The first call is for
  // commiting the styles of beggining of transition that are assigned above.
  // The second call is for assigning and commiting the styles of end of
  // transition, which triggers transition animation.
  requestAnimationFrame(function() {
    requestAnimationFrame(function() {
      this.setTransform_(
          newScreenImage,
          this.viewport_,
          null,
          opt_effect ? opt_effect.getDuration() : undefined);
      this.setTransform_(
          content,
          this.viewport_,
          null,
          opt_effect ? opt_effect.getDuration() : undefined);
      if (oldScreenImage) {
        ImageUtil.setAttribute(newScreenImage, 'fade', false);
        ImageUtil.setAttribute(oldScreenImage, 'fade', true);
        var reverse = opt_effect.getReverse();
        if (reverse) {
          this.setTransform_(oldScreenImage, oldViewport, reverse);
          setTimeout(function() {
            if (oldScreenImage.parentNode)
              oldScreenImage.parentNode.removeChild(oldScreenImage);
          }, reverse.getSafeInterval());
        } else {
            if (oldScreenImage.parentNode)
              oldScreenImage.parentNode.removeChild(oldScreenImage);
        }
      }
    }.bind(this));
  }.bind(this));
};

/**
 * @param {!HTMLCanvasElement|!HTMLImageElement} element The element to
 *     transform.
 * @param {!Viewport} viewport Viewport to be used for calculating
 *     transformation.
 * @param {ImageView.Effect=} opt_effect The effect to apply.
 * @param {number=} opt_duration Transition duration.
 * @private
 */
ImageView.prototype.setTransform_ = function(
    element, viewport, opt_effect, opt_duration) {
  if (!opt_effect)
    opt_effect = new ImageView.Effect.None();
  if (typeof opt_duration !== 'number')
    opt_duration = opt_effect.getDuration();
  element.style.transitionDuration = opt_duration + 'ms';
  element.style.transitionTimingFunction = opt_effect.getTiming();
  element.style.transform = opt_effect.transform(element, viewport);
};

/**
 * Creates zoom effect object.
 * @param {!ImageRect} screenRect Target rectangle in screen coordinates.
 * @return {!ImageView.Effect} Zoom effect object.
 */
ImageView.prototype.createZoomEffect = function(screenRect) {
  return new ImageView.Effect.ZoomToScreen(
      screenRect,
      ImageView.MODE_TRANSITION_DURATION);
};

/**
 * Visualizes crop or rotate operation. Hide the old image instantly, animate
 * the new image to visualize the operation.
 *
 * @param {!HTMLCanvasElement} canvas New content canvas.
 * @param {ImageRect} imageCropRect The crop rectangle in image coordinates.
 *     Null for rotation operations.
 * @param {number} rotate90 Rotation angle in 90 degree increments.
 * @return {number} Animation duration.
 */
ImageView.prototype.replaceAndAnimate = function(
    canvas, imageCropRect, rotate90) {
  assert(this.screenImage_);

  var oldImageBounds = {
    width: this.viewport_.getImageBounds().width,
    height: this.viewport_.getImageBounds().height
  };
  var oldScreenImage = this.screenImage_;
  this.replaceContent_(canvas);
  var newScreenImage = this.screenImage_;
  var effect = rotate90 ?
      new ImageView.Effect.Rotate(rotate90 > 0) :
      new ImageView.Effect.Zoom(
          oldImageBounds.width, oldImageBounds.height, assert(imageCropRect));

  this.setTransform_(newScreenImage, this.viewport_, effect, 0 /* instant */);

  oldScreenImage.parentNode.appendChild(newScreenImage);
  oldScreenImage.parentNode.removeChild(oldScreenImage);

  // Let the layout fire, then animate back to non-transformed state.
  setTimeout(
      this.setTransform_.bind(
          this, newScreenImage, this.viewport_, null, effect.getDuration()),
      0);

  return effect.getSafeInterval();
};

/**
 * Visualizes "undo crop". Shrink the current image to the given crop rectangle
 * while fading in the new image.
 *
 * @param {!HTMLCanvasElement} canvas New content canvas.
 * @param {!ImageRect} imageCropRect The crop rectangle in image coordinates.
 * @return {number} Animation duration.
 */
ImageView.prototype.animateAndReplace = function(canvas, imageCropRect) {
  var oldScreenImage = this.screenImage_;
  this.replaceContent_(canvas);
  var newScreenImage = this.screenImage_;
  var setFade = ImageUtil.setAttribute.bind(null, assert(newScreenImage),
      'fade');
  setFade(true);
  oldScreenImage.parentNode.insertBefore(newScreenImage, oldScreenImage);
  var effect = new ImageView.Effect.Zoom(
      this.viewport_.getImageBounds().width,
      this.viewport_.getImageBounds().height,
      imageCropRect);

  // Animate to the transformed state.
  this.setTransform_(oldScreenImage, this.viewport_, effect);
  setTimeout(setFade.bind(null, false), 0);
  setTimeout(function() {
    if (oldScreenImage.parentNode)
      oldScreenImage.parentNode.removeChild(oldScreenImage);
  }, effect.getSafeInterval());

  return effect.getSafeInterval();
};

/* Transition effects */

/**
 * Base class for effects.
 *
 * @param {number} duration Duration in ms.
 * @param {string=} opt_timing CSS transition timing function name.
 * @constructor
 * @struct
 */
ImageView.Effect = function(duration, opt_timing) {
  this.duration_ = duration;
  this.timing_ = opt_timing || 'linear';
};

/**
 * Default duration of an effect.
 * @type {number}
 * @const
 */
ImageView.Effect.DEFAULT_DURATION = 180;

/**
 * Effect margin.
 * @type {number}
 * @const
 */
ImageView.Effect.MARGIN = 100;

/**
 * @return {number} Effect duration in ms.
 */
ImageView.Effect.prototype.getDuration = function() { return this.duration_; };

/**
 * @return {number} Delay in ms since the beginning of the animation after which
 * it is safe to perform CPU-heavy operations without disrupting the animation.
 */
ImageView.Effect.prototype.getSafeInterval = function() {
  return this.getDuration() + ImageView.Effect.MARGIN;
};

/**
 * Reverses the effect.
 * @return {ImageView.Effect} Reversed effect. Null is returned if this
 *     is not supported in the effect.
 */
ImageView.Effect.prototype.getReverse = function() {
  return null;
};

/**
 * @return {string} CSS transition timing function name.
 */
ImageView.Effect.prototype.getTiming = function() { return this.timing_; };

/**
 * Obtains the CSS transformation string of the effect.
 * @param {!HTMLCanvasElement|!HTMLImageElement} element Canvas element to be
 *     applied the transformation.
 * @param {!Viewport} viewport Current viewport.
 * @return {string} CSS transformation description.
 */
ImageView.Effect.prototype.transform = function(element, viewport) {
  throw new Error('Not implemented.');
};

/**
 * Default effect.
 *
 * @constructor
 * @extends {ImageView.Effect}
 * @struct
 */
ImageView.Effect.None = function() {
  ImageView.Effect.call(this, 0, 'easy-out');
};

/**
 * Inherits from ImageView.Effect.
 */
ImageView.Effect.None.prototype = { __proto__: ImageView.Effect.prototype };

/**
 * @override
 */
ImageView.Effect.None.prototype.transform = function(element, viewport) {
  return viewport.getTransformation(element.width, element.height);
};

/**
 * Slide effect.
 *
 * @param {number} direction -1 for left, 1 for right.
 * @param {boolean=} opt_slow True if slow (as in slideshow).
 * @constructor
 * @extends {ImageView.Effect}
 * @struct
 */
ImageView.Effect.Slide = function Slide(direction, opt_slow) {
  ImageView.Effect.call(this,
      opt_slow ? 800 : ImageView.Effect.DEFAULT_DURATION, 'ease-out');
  this.direction_ = direction;
  this.slow_ = opt_slow;
  this.shift_ = opt_slow ? 100 : 40;
  if (this.direction_ < 0) this.shift_ = -this.shift_;
};

ImageView.Effect.Slide.prototype = { __proto__: ImageView.Effect.prototype };

/**
 * @override
 */
ImageView.Effect.Slide.prototype.getReverse = function() {
  return new ImageView.Effect.Slide(-this.direction_, this.slow_);
};

/**
 * @override
 */
ImageView.Effect.Slide.prototype.transform = function(element, viewport) {
  return viewport.getTransformation(
      element.width, element.height, this.shift_);
};

/**
 * Zoom effect.
 *
 * Animates the original rectangle to the target rectangle.
 *
 * @param {number} previousImageWidth Width of the full resolution image.
 * @param {number} previousImageHeight Height of the full resolution image.
 * @param {!ImageRect} imageCropRect Crop rectangle in the full resolution
 *     image.
 * @param {number=} opt_duration Duration of the effect.
 * @constructor
 * @extends {ImageView.Effect}
 * @struct
 */
ImageView.Effect.Zoom = function(
    previousImageWidth, previousImageHeight, imageCropRect, opt_duration) {
  ImageView.Effect.call(this,
      opt_duration || ImageView.Effect.DEFAULT_DURATION, 'ease-out');
  this.previousImageWidth_ = previousImageWidth;
  this.previousImageHeight_ = previousImageHeight;
  this.imageCropRect_ = imageCropRect;
};

ImageView.Effect.Zoom.prototype = { __proto__: ImageView.Effect.prototype };

/**
 * @override
 */
ImageView.Effect.Zoom.prototype.transform = function(element, viewport) {
  return viewport.getCroppingTransformation(
      element.width,
      element.height,
      this.previousImageWidth_,
      this.previousImageHeight_,
      this.imageCropRect_);
};

/**
 * Effect to zoom to a screen rectangle.
 *
 * @param {!ImageRect} screenRect Rectangle in the application window's
 *     coordinate.
 * @param {number=} opt_duration Duration of effect.
 * @constructor
 * @extends {ImageView.Effect}
 * @struct
 */
ImageView.Effect.ZoomToScreen = function(screenRect, opt_duration) {
  ImageView.Effect.call(this, opt_duration ||
      ImageView.Effect.DEFAULT_DURATION);
  this.screenRect_ = screenRect;
};

ImageView.Effect.ZoomToScreen.prototype = {
  __proto__: ImageView.Effect.prototype
};

/**
 * @override
 */
ImageView.Effect.ZoomToScreen.prototype.transform = function(
    element, viewport) {
  return viewport.getScreenRectTransformation(
      element.width,
      element.height,
      this.screenRect_);
};

/**
 * Rotation effect.
 *
 * @param {boolean} orientation Orientation of rotation. True is for clockwise
 *     and false is for counterclockwise.
 * @constructor
 * @extends {ImageView.Effect}
 * @struct
 */
ImageView.Effect.Rotate = function(orientation) {
  ImageView.Effect.call(this, ImageView.Effect.DEFAULT_DURATION);
  this.orientation_ = orientation;
};

ImageView.Effect.Rotate.prototype = { __proto__: ImageView.Effect.prototype };

/**
 * @override
 */
ImageView.Effect.Rotate.prototype.transform = function(element, viewport) {
  return viewport.getRotatingTransformation(
      element.width, element.height, this.orientation_ ? -1 : 1);
};
