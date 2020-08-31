// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ImageLoader loads an image from a given Entry into a canvas in two steps:
 * 1. Loads the image into an HTMLImageElement.
 * 2. Copies pixels from HTMLImageElement to HTMLCanvasElement. This is done
 *    stripe-by-stripe to avoid freezing up the UI. The transform is taken into
 *    account.
 *
 * @param {!HTMLDocument} document Owner document.
 * @param {!MetadataModel} metadataModel
 * @constructor
 * @struct
 */
ImageUtil.ImageLoader = function(document, metadataModel) {
  this.document_ = document;

  /**
   * @private {!MetadataModel}
   * @const
   */
  this.metadataModel_ = metadataModel;

  this.generation_ = 0;

  /**
   * @type {number}
   * @private
   */
  this.timeout_ = 0;

  /**
   * Cleanup function for Blink EXIF changes in m81. See comments in
   * convertImage_() and https://crbug.com/1043561.
   *
   * @type {function()}
   */
  this.onCancelForExifWorkaround_ = () => {};

  /**
   * @type {?function(!HTMLCanvasElement, string=)}
   * @private
   */
  this.callback_ = null;

  /**
   * @type {FileEntry}
   * @private
   */
  this.entry_ = null;
};

/**
 * Loads media.
 * TODO(mtomasz): Simplify, or even get rid of this class and merge with the
 * ThumbnaiLoader class.
 *
 * @param {!GalleryItem} item Item representing the media to be loaded.
 * @param {function(!HTMLCanvasElement, string=)} callback Callback to be
 *     called when loaded. The second optional argument is an error identifier.
 * @param {number=} opt_delay Load delay in milliseconds, useful to let the
 *     animations play out before the computation heavy media loading starts.
 */
ImageUtil.ImageLoader.prototype.load = function(item, callback, opt_delay) {
  var entry = item.getEntry();

  this.cancel();
  this.entry_ = entry;
  this.callback_ = callback;

  // The transform fetcher is not cancellable so we need a generation counter.
  var generation = ++this.generation_;

  if (FileType.isVideo(entry)) {
    var targetVideo = assertInstanceof(
        this.document_.createElement('video'), HTMLVideoElement);
    targetVideo.controls = true;
    targetVideo.controlsList = 'nodownload';
    targetVideo.classList.add('video');
  } else {
    var targetImage =
        assertInstanceof(this.document_.createElement('img'), HTMLImageElement);

    /**
     * @param {!HTMLImageElement} image Image to be transformed.
     * @param {Object=} opt_transform Transformation.
     */
    var onTransform = function(image, opt_transform) {
      if (generation === this.generation_) {
        this.convertImage_(image, opt_transform);
      }
    };
    onTransform = onTransform.bind(this);
  }

  /**
   * @param {string=} opt_error Error.
   */
  var onError = function(opt_error) {
    targetImage.onerror = null;
    targetImage.onload = null;
    var tmpCallback = this.callback_;
    this.callback_ = null;
    var emptyCanvas = assertInstanceof(this.document_.createElement('canvas'),
        HTMLCanvasElement);
    emptyCanvas.width = 0;
    emptyCanvas.height = 0;
    tmpCallback(emptyCanvas, opt_error);
  };
  onError = onError.bind(this);

  var loadImage = function(url) {
    if (generation !== this.generation_) {
      return;
    }

    metrics.startInterval(ImageUtil.getMetricName('LoadTime'));
    this.timeout_ = 0;

    targetImage.onload = function() {
      targetImage.onerror = null;
      targetImage.onload = null;
      if (generation !== this.generation_) {
        return;
      }
      this.metadataModel_.get([entry], ['contentImageTransform']).then(
          function(metadataItems) {
            onTransform(targetImage, metadataItems[0].contentImageTransform);
          }.bind(this));
    }.bind(this);

    // The error callback has an optional error argument, which in case of a
    // general error should not be specified
    targetImage.onerror = onError.bind(null, 'GALLERY_IMAGE_ERROR');

    targetImage.src = url;
  }.bind(this);

  var loadVideo = function(url) {
    if (generation !== this.generation_) {
      return;
    }

    metrics.startInterval(ImageUtil.getMetricName('LoadTime'));
    this.timeout_ = 0;

    var source = assertInstanceof(
        this.document_.createElement('source'), HTMLSourceElement);
    source.src = url;
    targetVideo.appendChild(source);

    // Start the <video> element loading immediately.
    setTimeout(this.callback_, 0, targetVideo);
    this.callback_ = null;

    // Start a task to set the poster property using the thumbnail so that there
    // is an image visible before the user clicks play. Ignore errors - the
    // poster just won't be set. If the video load also fails, the standard
    // controls show a broken play icon.
    var thumbnailMetadata = item.getThumbnailMetadataItem();
    if (!thumbnailMetadata) {
      return;
    }

    var posterLoader = new ThumbnailLoader(
        entry, undefined /* opt_loaderType */, thumbnailMetadata);
    posterLoader.loadAsDataUrl(ThumbnailLoader.FillMode.FIT)
        .then(function(result) {
          targetVideo.poster = result.data;
        }.bind(this))
        .catch(function(error) {}.bind(this));
  }.bind(this);

  // Loads the media. If already loaded, then forces a reload.
  var startLoad = function() {
    if (generation !== this.generation_) {
      return;
    }

    if (FileType.isVideo(entry)) {
      loadVideo(entry.toURL() + '?nocache=' + Date.now());
      return;
    }

    // Obtain target URL.
    if (FileType.isRaw(entry)) {
      var timestamp =
          item.getMetadataItem() &&
          item.getMetadataItem().modificationTime &&
          item.getMetadataItem().modificationTime.getTime();
      let request = LoadImageRequest.createFullImageRequest({
        url: entry.toURL(),
        cache: true,
        timestamp: timestamp,
        priority: 0  // Use highest priority to show main image.
      });
      ImageLoaderClient.getInstance().load(request, function(result) {
        if (generation !== this.generation_) {
          return;
        }
        if (result.status === 'success') {
          loadImage(result.data);
        } else {
          onError('GALLERY_IMAGE_ERROR');
        }
      }.bind(this));
      return;
    }

    // Load the image directly. The query parameter is workaround for
    // crbug.com/379678, which force to update the contents of the image.
    loadImage(entry.toURL() + '?nocache=' + Date.now());
  }.bind(this);

  if (opt_delay) {
    this.timeout_ = setTimeout(startLoad, opt_delay);
  } else {
    startLoad();
  }
};

/**
 * @return {boolean} True if an image is loading.
 */
ImageUtil.ImageLoader.prototype.isBusy = function() {
  return !!this.callback_;
};

/**
 * @param {Entry} entry Image entry.
 * @return {boolean} True if loader loads this image.
 */
ImageUtil.ImageLoader.prototype.isLoading = function(entry) {
  return this.isBusy() && util.isSameEntry(this.entry_, entry);
};

/**
 * @param {function(!HTMLCanvasElement, string=)} callback To be called when the
 *     image loaded.
 */
ImageUtil.ImageLoader.prototype.setCallback = function(callback) {
  this.callback_ = callback;
};

/**
 * Stops loading image.
 */
ImageUtil.ImageLoader.prototype.cancel = function() {
  if (!this.callback_) {
    return;
  }
  this.callback_ = null;
  if (this.timeout_) {
    this.onCancelForExifWorkaround_();
    clearTimeout(this.timeout_);
    this.timeout_ = 0;
  }
  this.generation_++;  // Silence the transform fetcher if it is in progress.
};

/**
 * @param {!HTMLImageElement} image Image to be transformed.
 * @param {!Object} transform transformation description to apply to the image.
 * @private
 */
ImageUtil.ImageLoader.prototype.convertImage_ = function(image, transform) {
  if (!transform ||
        (transform.rotate90 === 0 &&
         transform.scaleX === 1 &&
         transform.scaleY === 1)) {
    setTimeout(this.callback_, 0, image);
    this.callback_ = null;
    return;
  }

  // Helper to suppress JSC_USELESS_CODE below. Like goog.reflect.sinkValue().
  function sinkValue(value) {}

  // If the <img> is not in the DOM (and style calculated for it). Chrome m81+
  // provides width/height that already accounts for EXIF orientations.
  image.style.imageOrientation = 'none';
  image.style.visibility = 'hidden';
  document.body.appendChild(image);
  sinkValue(image.clientWidth);  // Ensure style resolves.

  var canvas = this.document_.createElement('canvas');

  if (transform.rotate90 & 1) {  // Rotated +/-90deg, swap the dimensions.
    canvas.width = image.height;
    canvas.height = image.width;
  } else {
    canvas.width = image.width;
    canvas.height = image.height;
  }

  // Before getting context, apply style to ignore EXIF orientation from source
  // pixel data. The <canvas> also needs to be in the DOM when getContext() is
  // called for this to take effect.
  canvas.style.imageOrientation = 'none';
  canvas.style.visibility = 'hidden';
  document.body.appendChild(canvas);
  sinkValue(canvas.clientWidth);  // Ensure style resolves.

  // Remove image and canvas from the DOM once the content is drawn. Note the
  // canvas may be added back later. Doing this earlier causes the elements to
  // "forget' their image-orientation style attribute.
  this.onCancelForExifWorkaround_ = () => {
    image.remove();
    canvas.remove();
    canvas.style.visibility = 'unset';
    this.onCancelForExifWorkaround_ = () => {};
  };

  var context = canvas.getContext('2d');
  context.save();
  context.translate(canvas.width / 2, canvas.height / 2);
  context.rotate(transform.rotate90 * Math.PI / 2);
  context.scale(transform.scaleX, transform.scaleY);

  var stripCount = Math.ceil(image.width * image.height / (1 << 21));
  var step = Math.max(16, Math.ceil(image.height / stripCount)) & 0xFFFFF0;

  this.copyStrip_(context, image, 0, step);
};

/**
 * @param {!CanvasRenderingContext2D} context Context to draw.
 * @param {!HTMLImageElement} image Image to draw.
 * @param {number} firstRow Number of the first pixel row to draw.
 * @param {number} rowCount Count of pixel rows to draw.
 * @private
 */
ImageUtil.ImageLoader.prototype.copyStrip_ = function(
    context, image, firstRow, rowCount) {
  var lastRow = Math.min(firstRow + rowCount, image.height);

  context.drawImage(
      image, 0, firstRow, image.width, lastRow - firstRow,
      -image.width / 2, firstRow - image.height / 2,
      image.width, lastRow - firstRow);

  if (lastRow === image.height) {
    context.restore();
    if (this.entry_.toURL().substr(0, 5) !== 'data:') {  // Ignore data urls.
      metrics.recordInterval(ImageUtil.getMetricName('LoadTime'));
    }
    this.onCancelForExifWorkaround_();
    setTimeout(this.callback_, 0, context.canvas);
    this.callback_ = null;
  } else {
    var self = this;
    this.timeout_ = setTimeout(
        function() {
          self.timeout_ = 0;
          self.copyStrip_(context, image, lastRow, rowCount);
        }, 0);
  }
};

