// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace object for the utilities.
var ImageUtil = {};

/**
 * Performance trace.
 */
ImageUtil.trace = (function() {
  /**
   * Performance trace.
   * @constructor
   * @struct
   */
  function PerformanceTrace() {
    this.lines_ = {};
    this.timers_ = {};
    this.container_ = null;
  }

  PerformanceTrace.prototype.bindToDOM = function(container) {
    this.container_ = container;
  };

  PerformanceTrace.prototype.report = function(key, value) {
    if (!(key in this.lines_)) {
      if (this.container_) {
        var div = this.lines_[key] = document.createElement('div');
        this.container_.appendChild(div);
      } else {
        this.lines_[key] = {};
      }
    }
    this.lines_[key].textContent = key + ': ' + value;
    if (ImageUtil.trace.log) this.dumpLine(key);
  };

  PerformanceTrace.prototype.resetTimer = function(key) {
    this.timers_[key] = Date.now();
  };

  PerformanceTrace.prototype.reportTimer = function(key) {
    this.report(key, (Date.now() - this.timers_[key]) + 'ms');
  };

  PerformanceTrace.prototype.dump = function() {
    for (var key in this.lines_)
      this.dumpLine(key);
  };

  PerformanceTrace.prototype.dumpLine = function(key) {
    console.log('trace.' + this.lines_[key].textContent);
  };

  return new PerformanceTrace();
})();

/**
 * @param {number} min Minimum value.
 * @param {number} value Value to adjust.
 * @param {number} max Maximum value.
 * @return {number} The closest to the |value| number in span [min, max].
 */
ImageUtil.clamp = function(min, value, max) {
  return Math.max(min, Math.min(max, value));
};

/**
 * @param {number} min Minimum value.
 * @param {number} value Value to check.
 * @param {number} max Maximum value.
 * @return {boolean} True if value is between.
 */
ImageUtil.between = function(min, value, max) {
  return (value - min) * (value - max) <= 0;
};

/**
 * Rectangle class.
 *
 * @param {number} left Left.
 * @param {number} top Top.
 * @param {number} width Width.
 * @param {number} height Height.
 * @constructor
 * @struct
 */
function ImageRect(left, top, width, height) {
  this.left = left;
  this.top = top;
  this.width = width;
  this.height = height;
}

/**
 * Creates an image rect with an image or a canvas.
 * @param {!(HTMLImageElement|HTMLCanvasElement)} image An image or a canvas.
 * @return {!ImageRect}
 */
ImageRect.createFromImage = function(image) {
  return new ImageRect(0, 0, image.width, image.height);
};

/**
 * Clone an image rect.
 * @param {!ImageRect} imageRect An image rect.
 * @return {!ImageRect}
 */
ImageRect.clone = function(imageRect) {
  return new ImageRect(imageRect.left, imageRect.top, imageRect.width,
      imageRect.height);
};

/**
 * Creates an image rect with a bound.
 * @param {{left: number, top: number, right: number, bottom: number}} bound
 *     A bound.
 * @return {!ImageRect}
 */
ImageRect.createFromBounds = function(bound) {
  return new ImageRect(bound.left, bound.top,
      bound.right - bound.left, bound.bottom - bound.top);
};

/**
 * Creates an image rect with width and height.
 * @param {number} width Width.
 * @param {number} height Height.
 * @return {!ImageRect}
 */
ImageRect.createFromWidthAndHeight = function(width, height) {
  return new ImageRect(0, 0, width, height);
};

ImageRect.prototype = /** @struct */ ({
  // TODO(yawano): Change getters to methods (e.g. getRight()).

  /**
   * Obtains the x coordinate of right edge. The most right pixels in the
   * rectangle are (x = right - 1) and the pixels (x = right) are not included
   * in the rectangle.
   * @return {number}
   */
  get right() {
    return this.left + this.width;
  },

  /**
   * Obtains the y coordinate of bottom edge. The most bottom pixels in the
   * rectangle are (y = bottom - 1) and the pixels (y = bottom) are not included
   * in the rectangle.
   * @return {number}
   */
  get bottom() {
    return this.top + this.height;
  }
});

/**
 * @param {number} factor Factor to scale.
 * @return {!ImageRect} A rectangle with every dimension scaled.
 */
ImageRect.prototype.scale = function(factor) {
  return new ImageRect(
      this.left * factor,
      this.top * factor,
      this.width * factor,
      this.height * factor);
};

/**
 * @param {number} dx Difference in X.
 * @param {number} dy Difference in Y.
 * @return {!ImageRect} A rectangle shifted by (dx,dy), same size.
 */
ImageRect.prototype.shift = function(dx, dy) {
  return new ImageRect(this.left + dx, this.top + dy, this.width, this.height);
};

/**
 * @param {number} x Coordinate of the left top corner.
 * @param {number} y Coordinate of the left top corner.
 * @return {!ImageRect} A rectangle with left==x and top==y, same size.
 */
ImageRect.prototype.moveTo = function(x, y) {
  return new ImageRect(x, y, this.width, this.height);
};

/**
 * @param {number} dx Difference in X.
 * @param {number} dy Difference in Y.
 * @return {!ImageRect} A rectangle inflated by (dx, dy), same center.
 */
ImageRect.prototype.inflate = function(dx, dy) {
  return new ImageRect(
      this.left - dx, this.top - dy, this.width + 2 * dx, this.height + 2 * dy);
};

/**
 * @param {number} x Coordinate of the point.
 * @param {number} y Coordinate of the point.
 * @return {boolean} True if the point lies inside the rectangle.
 */
ImageRect.prototype.inside = function(x, y) {
  return this.left <= x && x < this.left + this.width &&
         this.top <= y && y < this.top + this.height;
};

/**
 * @param {!ImageRect} rect Rectangle to check.
 * @return {boolean} True if this rectangle intersects with the |rect|.
 */
ImageRect.prototype.intersects = function(rect) {
  return (this.left + this.width) > rect.left &&
         (rect.left + rect.width) > this.left &&
         (this.top + this.height) > rect.top &&
         (rect.top + rect.height) > this.top;
};

/**
 * @param {!ImageRect} rect Rectangle to check.
 * @return {boolean} True if this rectangle containing the |rect|.
 */
ImageRect.prototype.contains = function(rect) {
  return (this.left <= rect.left) &&
         (rect.left + rect.width) <= (this.left + this.width) &&
         (this.top <= rect.top) &&
         (rect.top + rect.height) <= (this.top + this.height);
};

/**
 * @return {boolean} True if rectangle is empty.
 */
ImageRect.prototype.isEmpty = function() {
  return this.width === 0 || this.height === 0;
};

/**
 * Clamp the rectangle to the bounds by moving it.
 * Decrease the size only if necessary.
 * @param {!ImageRect} bounds Bounds.
 * @return {!ImageRect} Calculated rectangle.
 */
ImageRect.prototype.clamp = function(bounds) {
  var rect = ImageRect.clone(this);

  if (rect.width > bounds.width) {
    rect.left = bounds.left;
    rect.width = bounds.width;
  } else if (rect.left < bounds.left) {
    rect.left = bounds.left;
  } else if (rect.left + rect.width >
             bounds.left + bounds.width) {
    rect.left = bounds.left + bounds.width - rect.width;
  }

  if (rect.height > bounds.height) {
    rect.top = bounds.top;
    rect.height = bounds.height;
  } else if (rect.top < bounds.top) {
    rect.top = bounds.top;
  } else if (rect.top + rect.height >
             bounds.top + bounds.height) {
    rect.top = bounds.top + bounds.height - rect.height;
  }

  return rect;
};

/**
 * @return {string} String representation.
 */
ImageRect.prototype.toString = function() {
  return '(' + this.left + ',' + this.top + '):' +
         '(' + (this.left + this.width) + ',' + (this.top + this.height) + ')';
};
/*
 * Useful shortcuts for drawing (static functions).
 */

/**
 * Draw the image in context with appropriate scaling.
 * @param {!CanvasRenderingContext2D} context Context to draw.
 * @param {!(HTMLCanvasElement|HTMLImageElement)} image Image to draw.
 * @param {ImageRect=} opt_dstRect Rectangle in the canvas (whole canvas by
 *     default).
 * @param {ImageRect=} opt_srcRect Rectangle in the image (whole image by
 *     default).
 */
ImageRect.drawImage = function(context, image, opt_dstRect, opt_srcRect) {
  opt_dstRect = opt_dstRect ||
      ImageRect.createFromImage(assert(context.canvas));
  opt_srcRect = opt_srcRect || ImageRect.createFromImage(image);
  if (opt_dstRect.isEmpty() || opt_srcRect.isEmpty())
    return;
  context.drawImage(image,
      opt_srcRect.left, opt_srcRect.top, opt_srcRect.width, opt_srcRect.height,
      opt_dstRect.left, opt_dstRect.top, opt_dstRect.width, opt_dstRect.height);
};

/**
 * Draw a box around the rectangle.
 * @param {!CanvasRenderingContext2D} context Context to draw.
 * @param {!ImageRect} rect Rectangle.
 */
ImageRect.outline = function(context, rect) {
  context.strokeRect(
      rect.left - 0.5, rect.top - 0.5, rect.width + 1, rect.height + 1);
};

/**
 * Fill the rectangle.
 * @param {!CanvasRenderingContext2D} context Context to draw.
 * @param {!ImageRect} rect Rectangle.
 */
ImageRect.fill = function(context, rect) {
  context.fillRect(rect.left, rect.top, rect.width, rect.height);
};

/**
 * Fills the space between the two rectangles.
 * @param {!CanvasRenderingContext2D} context Context to draw.
 * @param {!ImageRect} inner Inner rectangle.
 * @param {!ImageRect} outer Outer rectangle.
 */
ImageRect.fillBetween = function(context, inner, outer) {
  var innerRight = inner.left + inner.width;
  var innerBottom = inner.top + inner.height;
  var outerRight = outer.left + outer.width;
  var outerBottom = outer.top + outer.height;
  if (inner.top > outer.top) {
    context.fillRect(
        outer.left, outer.top, outer.width, inner.top - outer.top);
  }
  if (inner.left > outer.left) {
    context.fillRect(
        outer.left, inner.top, inner.left - outer.left, inner.height);
  }
  if (inner.width < outerRight) {
    context.fillRect(
        innerRight, inner.top, outerRight - innerRight, inner.height);
  }
  if (inner.height < outerBottom) {
    context.fillRect(
        outer.left, innerBottom, outer.width, outerBottom - innerBottom);
  }
};

/**
 * Circle class.
 * @param {number} x X coordinate of circle center.
 * @param {number} y Y coordinate of circle center.
 * @param {number} r Radius.
 * @constructor
 */
function Circle(x, y, r) {
  this.x = x;
  this.y = y;
  this.squaredR = r * r;
}

/**
 * Check if the point is inside the circle.
 * @param {number} x X coordinate of the point.
 * @param {number} y Y coordinate of the point.
 * @return {boolean} True if the point is inside.
 */
Circle.prototype.inside = function(x, y) {
  x -= this.x;
  y -= this.y;
  return x * x + y * y <= this.squaredR;
};

/**
 * Copy an image applying scaling and rotation.
 *
 * @param {!HTMLCanvasElement} dst Destination.
 * @param {!(HTMLCanvasElement|HTMLImageElement)} src Source.
 * @param {number} scaleX Y scale transformation.
 * @param {number} scaleY X scale transformation.
 * @param {number} angle (in radians).
 */
ImageUtil.drawImageTransformed = function(dst, src, scaleX, scaleY, angle) {
  var context = dst.getContext('2d');
  context.save();
  context.translate(context.canvas.width / 2, context.canvas.height / 2);
  context.rotate(angle);
  context.scale(scaleX, scaleY);
  context.drawImage(src, -src.width / 2, -src.height / 2);
  context.restore();
};

/**
 * Adds or removes an attribute to/from an HTML element.
 * @param {!HTMLElement} element To be applied to.
 * @param {string} attribute Name of attribute.
 * @param {boolean} on True if add, false if remove.
 */
ImageUtil.setAttribute = function(element, attribute, on) {
  if (on)
    element.setAttribute(attribute, '');
  else
    element.removeAttribute(attribute);
};

/**
 * Adds or removes CSS class to/from an HTML element.
 * @param {!HTMLElement} element To be applied to.
 * @param {string} className Name of CSS class.
 * @param {boolean} on True if add, false if remove.
 */
ImageUtil.setClass = function(element, className, on) {
  var cl = element.classList;
  if (on)
    cl.add(className);
  else
    cl.remove(className);
};

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

  this.image_ = new Image();
  this.generation_ = 0;

  /**
   * @type {number}
   * @private
   */
  this.timeout_ = 0;

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
 * Loads an image.
 * TODO(mtomasz): Simplify, or even get rid of this class and merge with the
 * ThumbnaiLoader class.
 *
 * @param {!GalleryItem} item Item representing the image to be loaded.
 * @param {function(!HTMLCanvasElement, string=)} callback Callback to be
 *     called when loaded. The second optional argument is an error identifier.
 * @param {number=} opt_delay Load delay in milliseconds, useful to let the
 *     animations play out before the computation heavy image loading starts.
 */
ImageUtil.ImageLoader.prototype.load = function(item, callback, opt_delay) {
  var entry = item.getEntry();

  this.cancel();
  this.entry_ = entry;
  this.callback_ = callback;

  var targetImage = this.image_;
  // The transform fetcher is not cancellable so we need a generation counter.
  var generation = ++this.generation_;

  /**
   * @param {!HTMLImageElement} image Image to be transformed.
   * @param {Object=} opt_transform Transformation.
   */
  var onTransform = function(image, opt_transform) {
    if (generation === this.generation_) {
      this.convertImage_(
          image, opt_transform || { scaleX: 1, scaleY: 1, rotate90: 0});
    }
  };
  onTransform = onTransform.bind(this);

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
    if (generation !== this.generation_)
      return;

    ImageUtil.metrics.startInterval(ImageUtil.getMetricName('LoadTime'));
    this.timeout_ = 0;

    targetImage.onload = function() {
      targetImage.onerror = null;
      targetImage.onload = null;
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

  // Loads the image. If already loaded, then forces a reload.
  var startLoad = function() {
    if (generation !== this.generation_)
      return;

    // Target current image.
    targetImage = this.image_;

    // Obtain target URL.
    if (FileType.isRaw(entry)) {
      var timestamp =
          item.getMetadataItem() &&
          item.getMetadataItem().modificationTime &&
          item.getMetadataItem().modificationTime.getTime();
      ImageLoaderClient.getInstance().load(entry.toURL(), function(result) {
        if (generation !== this.generation_)
          return;
        if (result.status === 'success')
          loadImage(result.data);
        else
          onError('GALLERY_IMAGE_ERROR');
      }.bind(this), {
        cache: true,
        timestamp: timestamp,
        priority: 0  // Use highest priority to show main image.
      });
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
  if (!this.callback_)
    return;
  this.callback_ = null;
  if (this.timeout_) {
    clearTimeout(this.timeout_);
    this.timeout_ = 0;
  }
  if (this.image_) {
    this.image_.onload = function() {};
    this.image_.onerror = function() {};
    // Force to free internal image by assigning empty image.
    this.image_.src = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAA' +
        'AAABAAEAAAICTAEAOw==';
    this.image_ = document.createElement('img');
  }
  this.generation_++;  // Silence the transform fetcher if it is in progress.
};

/**
 * @param {!HTMLImageElement} image Image to be transformed.
 * @param {!Object} transform transformation description to apply to the image.
 * @private
 */
ImageUtil.ImageLoader.prototype.convertImage_ = function(image, transform) {
  var canvas = this.document_.createElement('canvas');

  if (transform.rotate90 & 1) {  // Rotated +/-90deg, swap the dimensions.
    canvas.width = image.height;
    canvas.height = image.width;
  } else {
    canvas.width = image.width;
    canvas.height = image.height;
  }

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
      ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('LoadTime'));
    }
    try {
      setTimeout(this.callback_, 0, context.canvas);
    } catch (e) {
      console.error(e);
    }
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

/**
 * @param {!HTMLElement} element To remove children from.
 */
ImageUtil.removeChildren = function(element) {
  element.textContent = '';
};

/**
 * @param {string} name File name (with extension).
 * @return {string} File name without extension.
 */
ImageUtil.getDisplayNameFromName = function(name) {
  var index = name.lastIndexOf('.');
  if (index !== -1)
    return name.substr(0, index);
  else
    return name;
};

/**
 * @param {string} name File name.
 * @return {string} File extension.
 */
ImageUtil.getExtensionFromFullName = function(name) {
  var index = name.lastIndexOf('.');
  if (index !== -1)
    return name.substring(index);
  else
    return '';
};

/**
 * Metrics (from metrics.js) itnitialized by the File Manager from owner frame.
 * @type {Object}
 */
ImageUtil.metrics = null;

/**
 * @param {string} name Local name.
 * @return {string} Full name.
 */
ImageUtil.getMetricName = function(name) {
  return 'PhotoEditor.' + name;
};

/**
 * Used for metrics reporting, keep in sync with the histogram description.
 * @type {Array<string>}
 * @const
 */
ImageUtil.FILE_TYPES = ['jpg', 'png', 'gif', 'bmp', 'webp'];
