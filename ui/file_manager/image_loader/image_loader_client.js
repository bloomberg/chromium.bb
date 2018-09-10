// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Client used to connect to the remote ImageLoader extension. Client class runs
 * in the extension, where the client.js is included (eg. Files app).
 * It sends remote requests using IPC to the ImageLoader class and forwards
 * its responses.
 *
 * Implements cache, which is stored in the calling extension.
 *
 * @constructor
 */
function ImageLoaderClient() {
  /**
   * @type {number}
   * @private
   */
  this.lastTaskId_ = 0;

  /**
   * LRU cache for images.
   * @type {!LRUCache.<{
   *     data: string, width:number, height:number, timestamp: ?number}>}
   * @private
   */
  this.cache_ = new LRUCache(ImageLoaderClient.CACHE_MEMORY_LIMIT);
}

/**
 * Image loader's extension id.
 * @const
 * @type {string}
 */
ImageLoaderClient.EXTENSION_ID = 'pmfjbimdmchhbnneeidfognadeopoehp';

/**
 * Returns a singleton instance.
 * @return {ImageLoaderClient} Client instance.
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
        type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
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
  chrome.runtime.sendMessage(
      ImageLoaderClient.EXTENSION_ID, request, opt_callback);
};

/**
 * Loads and resizes and image.
 *
 * @param {string} url Url of the requested image.
 * @param {function({status: string, data:string, width:number, height:number})}
 *     callback Callback used to return response. Width and height in the
 *     response is the size of image (data), i.e. When the image is resized,
 *     these values are resized width and height.
 * @param {Object=} opt_options Loader options, such as: scale, maxHeight,
 *     width, height and/or cache.
 * @return {?number} Remote task id or null if loaded from cache.
 */
ImageLoaderClient.prototype.load = function(url, callback, opt_options) {
  opt_options = /** @type {{cache: (boolean|undefined)}} */(opt_options || {});

  // Record cache usage.
  ImageLoaderClient.recordPercentage('Cache.Usage',
      this.cache_.size() / ImageLoaderClient.CACHE_MEMORY_LIMIT * 100.0);

  // Replace the extension id.
  var sourceId = chrome.i18n.getMessage('@@extension_id');
  var targetId = ImageLoaderClient.EXTENSION_ID;

  url = url.replace('filesystem:chrome-extension://' + sourceId,
                    'filesystem:chrome-extension://' + targetId);

  // Try to load from cache, if available.
  var cacheKey = ImageLoaderClient.createKey(url, opt_options);
  if (cacheKey) {
    if (opt_options.cache) {
      // Load from cache.
      ImageLoaderClient.recordBinary('Cached', true);
      var cachedValue = this.cache_.get(cacheKey);
      // Check if the image in cache is up to date. If not, then remove it.
      if (cachedValue && cachedValue.timestamp != opt_options.timestamp) {
        this.cache_.remove(cacheKey);
        cachedValue = null;
      }
      if (cachedValue && cachedValue.data &&
          cachedValue.width && cachedValue.height) {
        ImageLoaderClient.recordBinary('Cache.HitMiss', true);
        callback({
          status: 'success', data: cachedValue.data,
          width: cachedValue.width, height: cachedValue.height
        });
        return null;
      } else {
        ImageLoaderClient.recordBinary('Cache.HitMiss', false);
      }
    } else {
      // Remove from cache.
      ImageLoaderClient.recordBinary('Cached', false);
      this.cache_.remove(cacheKey);
    }
  }

  // Not available in cache, performing a request to a remote extension.
  var request = opt_options;
  this.lastTaskId_++;

  request.url = url;
  request.taskId = this.lastTaskId_;
  request.timestamp = opt_options.timestamp;

  ImageLoaderClient.sendMessage_(
      request,
      function(result) {
        // Save to cache.
        if (cacheKey && result.status == 'success' && opt_options.cache) {
          var value = {
            timestamp: opt_options.timestamp ? opt_options.timestamp : null,
            data: result.data, width: result.width, height: result.height
          };
          this.cache_.put(cacheKey, value, result.data.length);
        }
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
 * Memory limit for images data in bytes.
 *
 * @const
 * @type {number}
 */
ImageLoaderClient.CACHE_MEMORY_LIMIT = 20 * 1024 * 1024;  // 20 MB.

/**
 * Creates a cache key.
 *
 * @param {string} url Image url.
 * @param {Object=} opt_options Loader options as a hash array.
 * @return {?string} Cache key. It may return null if the class does not provide
 *     caches for the URL. (e.g. Data URL)
 */
ImageLoaderClient.createKey = function(url, opt_options) {
  if (/^data:/i.test(url))
    return null;
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

// Helper functions.

/**
 * Loads and resizes and image.
 *
 * @param {string} url Url of the requested image.
 * @param {HTMLImageElement} image Image node to load the requested picture
 *     into.
 * @param {Object} options Loader options, such as: orientation, scale,
 *     maxHeight, width, height and/or cache.
 * @param {function()} onSuccess Callback for success.
 * @param {function()} onError Callback for failure.
 * @return {?number} Remote task id or null if loaded from cache.
 */
ImageLoaderClient.loadToImage = function(
    url, image, options, onSuccess, onError) {
  var callback = function(result) {
    if (result.status == 'error') {
      onError();
      return;
    }
    image.src = result.data;
    onSuccess();
  };

  return ImageLoaderClient.getInstance().load(url, callback, options);
};
