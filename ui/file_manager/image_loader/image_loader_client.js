// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Client used to connect to the remote ImageLoader extension. Client class runs
 * in the extension, where the client.js is included (eg. Files.app).
 * It sends remote requests using IPC to the ImageLoader class and forwards
 * its responses.
 *
 * Implements cache, which is stored in the calling extension.
 *
 * @constructor
 */
function ImageLoaderClient() {
  /**
   * Hash array with active tasks.
   * @type {Object}
   * @private
   */
  this.tasks_ = {};

  /**
   * @type {number}
   * @private
   */
  this.lastTaskId_ = 0;

  /**
   * LRU cache for images.
   * @type {ImageLoaderClient.Cache}
   * @private
   */
  this.cache_ = new ImageLoaderClient.Cache();
}

/**
 * Image loader's extension id.
 * @const
 * @type {string}
 */
ImageLoaderClient.EXTENSION_ID = 'pmfjbimdmchhbnneeidfognadeopoehp';

/**
 * Returns a singleton instance.
 * @return {Client} Client instance.
 */
ImageLoaderClient.getInstance = function() {
  if (!ImageLoaderClient.instance_)
    ImageLoaderClient.instance_ = new ImageLoaderClient();
  return ImageLoaderClient.instance_;
};

/**
 * Records binary metrics. Counts for true and false are stored as a histogram.
 * @param {string} name Histogram's name.
 * @param {boolean} value True or false.
 */
ImageLoaderClient.recordBinary = function(name, value) {
  chrome.metricsPrivate.recordValue(
      { metricName: 'ImageLoader.Client.' + name,
        type: 'histogram-linear',
        min: 1,  // According to histogram.h, this should be 1 for enums.
        max: 2,  // Maximum should be exclusive.
        buckets: 3 },  // Number of buckets: 0, 1 and overflowing 2.
      value ? 1 : 0);
};

/**
 * Records percent metrics, stored as a histogram.
 * @param {string} name Histogram's name.
 * @param {number} value Value (0..100).
 */
ImageLoaderClient.recordPercentage = function(name, value) {
  chrome.metricsPrivate.recordPercentage('ImageLoader.Client.' + name,
                                         Math.round(value));
};

/**
 * Sends a message to the Image Loader extension.
 * @param {Object} request Hash array with request data.
 * @param {function(Object)=} opt_callback Response handling callback.
 *     The response is passed as a hash array.
 * @private
 */
ImageLoaderClient.sendMessage_ = function(request, opt_callback) {
  opt_callback = opt_callback || function(response) {};
  var sendMessage = chrome.runtime ? chrome.runtime.sendMessage :
                                     chrome.extension.sendMessage;
  sendMessage(ImageLoaderClient.EXTENSION_ID, request, opt_callback);
};

/**
 * Handles a message from the remote image loader and calls the registered
 * callback to pass the response back to the requester.
 *
 * @param {Object} message Response message as a hash array.
 * @private
 */
ImageLoaderClient.prototype.handleMessage_ = function(message) {
  if (!(message.taskId in this.tasks_)) {
    // This task has been canceled, but was already fetched, so it's result
    // should be discarded anyway.
    return;
  }

  var task = this.tasks_[message.taskId];

  // Check if the task is still valid.
  if (task.isValid())
    task.accept(message);

  delete this.tasks_[message.taskId];
};

/**
 * Loads and resizes and image. Use opt_isValid to easily cancel requests
 * which are not valid anymore, which will reduce cpu consumption.
 *
 * @param {string} url Url of the requested image.
 * @param {function} callback Callback used to return response.
 * @param {Object=} opt_options Loader options, such as: scale, maxHeight,
 *     width, height and/or cache.
 * @param {function(): boolean=} opt_isValid Function returning false in case
 *     a request is not valid anymore, eg. parent node has been detached.
 * @return {?number} Remote task id or null if loaded from cache.
 */
ImageLoaderClient.prototype.load = function(
    url, callback, opt_options, opt_isValid) {
  opt_options = opt_options || {};
  opt_isValid = opt_isValid || function() { return true; };

  // Record cache usage.
  ImageLoaderClient.recordPercentage('Cache.Usage', this.cache_.getUsage());

  // Cancel old, invalid tasks.
  var taskKeys = Object.keys(this.tasks_);
  for (var index = 0; index < taskKeys.length; index++) {
    var taskKey = taskKeys[index];
    var task = this.tasks_[taskKey];
    if (!task.isValid()) {
      // Cancel this task since it is not valid anymore.
      this.cancel(taskKey);
      delete this.tasks_[taskKey];
    }
  }

  // Replace the extension id.
  var sourceId = chrome.i18n.getMessage('@@extension_id');
  var targetId = ImageLoaderClient.EXTENSION_ID;

  url = url.replace('filesystem:chrome-extension://' + sourceId,
                    'filesystem:chrome-extension://' + targetId);

  // Try to load from cache, if available.
  var cacheKey = ImageLoaderClient.Cache.createKey(url, opt_options);
  if (opt_options.cache) {
    // Load from cache.
    ImageLoaderClient.recordBinary('Cached', 1);
    var cachedData = this.cache_.loadImage(cacheKey, opt_options.timestamp);
    if (cachedData) {
      ImageLoaderClient.recordBinary('Cache.HitMiss', 1);
      callback({status: 'success', data: cachedData});
      return null;
    } else {
      ImageLoaderClient.recordBinary('Cache.HitMiss', 0);
    }
  } else {
    // Remove from cache.
    ImageLoaderClient.recordBinary('Cached', 0);
    this.cache_.removeImage(cacheKey);
  }

  // Not available in cache, performing a request to a remote extension.
  var request = opt_options;
  this.lastTaskId_++;
  var task = {isValid: opt_isValid};
  this.tasks_[this.lastTaskId_] = task;

  request.url = url;
  request.taskId = this.lastTaskId_;
  request.timestamp = opt_options.timestamp;

  ImageLoaderClient.sendMessage_(
      request,
      function(result) {
        // Save to cache.
        if (result.status == 'success' && opt_options.cache)
          this.cache_.saveImage(cacheKey, result.data, opt_options.timestamp);
        callback(result);
      }.bind(this));
  return request.taskId;
};

/**
 * Cancels the request.
 * @param {number} taskId Task id returned by ImageLoaderClient.load().
 */
ImageLoaderClient.prototype.cancel = function(taskId) {
  ImageLoaderClient.sendMessage_({taskId: taskId, cancel: true});
};

/**
 * Least Recently Used (LRU) cache implementation to be used by
 * Client class. It has memory constraints, so it will never
 * exceed specified memory limit defined in MEMORY_LIMIT.
 *
 * @constructor
 */
ImageLoaderClient.Cache = function() {
  this.images_ = [];
  this.size_ = 0;
};

/**
 * Memory limit for images data in bytes.
 *
 * @const
 * @type {number}
 */
ImageLoaderClient.Cache.MEMORY_LIMIT = 20 * 1024 * 1024;  // 20 MB.

/**
 * Creates a cache key.
 *
 * @param {string} url Image url.
 * @param {Object=} opt_options Loader options as a hash array.
 * @return {string} Cache key.
 */
ImageLoaderClient.Cache.createKey = function(url, opt_options) {
  opt_options = opt_options || {};
  return JSON.stringify({
    url: url,
    orientation: opt_options.orientation,
    scale: opt_options.scale,
    width: opt_options.width,
    height: opt_options.height,
    maxWidth: opt_options.maxWidth,
    maxHeight: opt_options.maxHeight});
};

/**
 * Evicts the least used elements in cache to make space for a new image.
 *
 * @param {number} size Requested size.
 * @private
 */
ImageLoaderClient.Cache.prototype.evictCache_ = function(size) {
  // Sort from the most recent to the oldest.
  this.images_.sort(function(a, b) {
    return b.lastLoadTimestamp - a.lastLoadTimestamp;
  });

  while (this.images_.length > 0 &&
         (ImageLoaderClient.Cache.MEMORY_LIMIT - this.size_ < size)) {
    var entry = this.images_.pop();
    this.size_ -= entry.data.length;
  }
};

/**
 * Saves an image in the cache.
 *
 * @param {string} key Cache key.
 * @param {string} data Image data.
 * @param {number=} opt_timestamp Last modification timestamp. Used to detect
 *     if the cache entry becomes out of date.
 */
ImageLoaderClient.Cache.prototype.saveImage = function(
    key, data, opt_timestamp) {
  // If the image is currently in cache, then remove it.
  if (this.images_[key])
    this.removeImage(key);

  if (ImageLoaderClient.Cache.MEMORY_LIMIT - this.size_ < data.length) {
    ImageLoaderClient.recordBinary('Evicted', 1);
    this.evictCache_(data.length);
  } else {
    ImageLoaderClient.recordBinary('Evicted', 0);
  }

  if (ImageLoaderClient.Cache.MEMORY_LIMIT - this.size_ >= data.length) {
    this.images_[key] = {
      lastLoadTimestamp: Date.now(),
      timestamp: opt_timestamp ? opt_timestamp : null,
      data: data
    };
    this.size_ += data.length;
  }
};

/**
 * Loads an image from the cache (if available) or returns null.
 *
 * @param {string} key Cache key.
 * @param {number=} opt_timestamp Last modification timestamp. If different
 *     that the one in cache, then the entry will be invalidated.
 * @return {?string} Data of the loaded image or null.
 */
ImageLoaderClient.Cache.prototype.loadImage = function(key, opt_timestamp) {
  if (!(key in this.images_))
    return null;

  var entry = this.images_[key];
  entry.lastLoadTimestamp = Date.now();

  // Check if the image in cache is up to date. If not, then remove it and
  // return null.
  if (entry.timestamp != opt_timestamp) {
    this.removeImage(key);
    return null;
  }

  return entry.data;
};

/**
 * Returns cache usage.
 * @return {number} Value in percent points (0..100).
 */
ImageLoaderClient.Cache.prototype.getUsage = function() {
  return this.size_ / ImageLoaderClient.Cache.MEMORY_LIMIT * 100.0;
};

/**
 * Removes the image from the cache.
 * @param {string} key Cache key.
 */
ImageLoaderClient.Cache.prototype.removeImage = function(key) {
  if (!(key in this.images_))
    return;

  var entry = this.images_[key];
  this.size_ -= entry.data.length;
  delete this.images_[key];
};

// Helper functions.

/**
 * Loads and resizes and image. Use opt_isValid to easily cancel requests
 * which are not valid anymore, which will reduce cpu consumption.
 *
 * @param {string} url Url of the requested image.
 * @param {Image} image Image node to load the requested picture into.
 * @param {Object} options Loader options, such as: orientation, scale,
 *     maxHeight, width, height and/or cache.
 * @param {function} onSuccess Callback for success.
 * @param {function} onError Callback for failure.
 * @param {function=} opt_isValid Function returning false in case
 *     a request is not valid anymore, eg. parent node has been detached.
 * @return {?number} Remote task id or null if loaded from cache.
 */
ImageLoaderClient.loadToImage = function(
    url, image, options, onSuccess, onError, opt_isValid) {
  var callback = function(result) {
    if (result.status == 'error') {
      onError();
      return;
    }
    image.src = result.data;
    onSuccess();
  };

  return ImageLoaderClient.getInstance().load(
      url, callback, options, opt_isValid);
};
