// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Loads and resizes an image.
 * @constructor
 */
function ImageLoader() {
  /**
   * Persistent cache object.
   * @type {Cache}
   * @private
   */
  this.cache_ = new Cache();

  /**
   * Manages pending requests and runs them in order of priorities.
   * @type {Worker}
   * @private
   */
  this.worker_ = new Worker();

  // Grant permissions to all volumes, initialize the cache and then start the
  // worker.
  chrome.fileBrowserPrivate.getVolumeMetadataList(function(volumeMetadataList) {
    var initPromises = volumeMetadataList.map(function(volumeMetadata) {
      var requestPromise = new Promise(function(callback) {
        chrome.fileBrowserPrivate.requestFileSystem(
            volumeMetadata.volumeId,
            callback);
      });
      return requestPromise;
    });
    initPromises.push(new Promise(this.cache_.initialize.bind(this.cache_)));

    // After all initialization promises are done, start the worker.
    Promise.all(initPromises).then(this.worker_.start.bind(this.worker_));

    // Listen for mount events, and grant permissions to volumes being mounted.
    chrome.fileBrowserPrivate.onMountCompleted.addListener(
        function(event) {
          if (event.eventType == 'mount' && event.status == 'success') {
            chrome.fileBrowserPrivate.requestFileSystem(
                event.volumeMetadata.volumeId, function() {});
          }
        });
  }.bind(this));

  // Listen for incoming requests.
  chrome.extension.onMessageExternal.addListener(function(request,
                                                          sender,
                                                          sendResponse) {
    if (ImageLoader.ALLOWED_CLIENTS.indexOf(sender.id) !== -1) {
      // Sending a response may fail if the receiver already went offline.
      // This is not an error, but a normal and quite common situation.
      var failSafeSendResponse = function(response) {
        try {
          sendResponse(response);
        }
        catch (e) {
          // Ignore the error.
        }
      };
      return this.onMessage_(sender.id, request, failSafeSendResponse);
    }
  }.bind(this));
}

/**
 * List of extensions allowed to perform image requests.
 *
 * @const
 * @type {Array.<string>}
 */
ImageLoader.ALLOWED_CLIENTS = [
  'hhaomjibdihmijegdhdafkllkbggdgoj',  // File Manager's extension id.
  'nlkncpkkdoccmpiclbokaimcnedabhhm'  // Gallery extension id.
];

/**
 * Handles a request. Depending on type of the request, starts or stops
 * an image task.
 *
 * @param {string} senderId Sender's extension id.
 * @param {Object} request Request message as a hash array.
 * @param {function} callback Callback to be called to return response.
 * @return {boolean} True if the message channel should stay alive until the
 *     callback is called.
 * @private
 */
ImageLoader.prototype.onMessage_ = function(senderId, request, callback) {
  var requestId = senderId + ':' + request.taskId;
  if (request.cancel) {
    // Cancel a task.
    this.worker_.remove(requestId);
    return false;  // No callback calls.
  } else {
    // Create a request task and add it to the worker (queue).
    var requestTask = new Request(requestId, this.cache_, request, callback);
    this.worker_.add(requestTask);
    return true;  // Request will call the callback.
  }
};

/**
 * Returns the singleton instance.
 * @return {ImageLoader} ImageLoader object.
 */
ImageLoader.getInstance = function() {
  if (!ImageLoader.instance_)
    ImageLoader.instance_ = new ImageLoader();
  return ImageLoader.instance_;
};

/**
 * Checks if the options contain any image processing.
 *
 * @param {number} width Source width.
 * @param {number} height Source height.
 * @param {Object} options Resizing options as a hash array.
 * @return {boolean} True if yes, false if not.
 */
ImageLoader.shouldProcess = function(width, height, options) {
  var targetDimensions = ImageLoader.resizeDimensions(width, height, options);

  // Dimensions has to be adjusted.
  if (targetDimensions.width != width || targetDimensions.height != height)
    return true;

  // Orientation has to be adjusted.
  if (options.orientation)
    return true;

  // No changes required.
  return false;
};

/**
 * Calculates dimensions taking into account resize options, such as:
 * - scale: for scaling,
 * - maxWidth, maxHeight: for maximum dimensions,
 * - width, height: for exact requested size.
 * Returns the target size as hash array with width, height properties.
 *
 * @param {number} width Source width.
 * @param {number} height Source height.
 * @param {Object} options Resizing options as a hash array.
 * @return {Object} Dimensions, eg. {width: 100, height: 50}.
 */
ImageLoader.resizeDimensions = function(width, height, options) {
  var sourceWidth = width;
  var sourceHeight = height;

  // Flip dimensions for odd orientation values: 1 (90deg) and 3 (270deg).
  if (options.orientation && options.orientation % 2) {
    sourceWidth = height;
    sourceHeight = width;
  }

  var targetWidth = sourceWidth;
  var targetHeight = sourceHeight;

  if ('scale' in options) {
    targetWidth = sourceWidth * options.scale;
    targetHeight = sourceHeight * options.scale;
  }

  if (options.maxWidth &&
      targetWidth > options.maxWidth) {
      var scale = options.maxWidth / targetWidth;
      targetWidth *= scale;
      targetHeight *= scale;
  }

  if (options.maxHeight &&
      targetHeight > options.maxHeight) {
      var scale = options.maxHeight / targetHeight;
      targetWidth *= scale;
      targetHeight *= scale;
  }

  if (options.width)
    targetWidth = options.width;

  if (options.height)
    targetHeight = options.height;

  targetWidth = Math.round(targetWidth);
  targetHeight = Math.round(targetHeight);

  return {width: targetWidth, height: targetHeight};
};

/**
 * Performs resizing of the source image into the target canvas.
 *
 * @param {HTMLCanvasElement|Image} source Source image or canvas.
 * @param {HTMLCanvasElement} target Target canvas.
 * @param {Object} options Resizing options as a hash array.
 */
ImageLoader.resize = function(source, target, options) {
  var targetDimensions = ImageLoader.resizeDimensions(
      source.width, source.height, options);

  target.width = targetDimensions.width;
  target.height = targetDimensions.height;

  // Default orientation is 0deg.
  var orientation = options.orientation || 0;

  // For odd orientation values: 1 (90deg) and 3 (270deg) flip dimensions.
  var drawImageWidth;
  var drawImageHeight;
  if (orientation % 2) {
    drawImageWidth = target.height;
    drawImageHeight = target.width;
  } else {
    drawImageWidth = target.width;
    drawImageHeight = target.height;
  }

  var targetContext = target.getContext('2d');
  targetContext.save();
  targetContext.translate(target.width / 2, target.height / 2);
  targetContext.rotate(orientation * Math.PI / 2);
  targetContext.drawImage(
      source,
      0, 0,
      source.width, source.height,
      -drawImageWidth / 2, -drawImageHeight / 2,
      drawImageWidth, drawImageHeight);
  targetContext.restore();
};
