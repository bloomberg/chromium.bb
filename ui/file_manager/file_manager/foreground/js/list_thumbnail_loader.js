// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A thumbnail loader for list style UI.
 *
 * ListThumbnailLoader is a thubmanil loader designed for list style ui. List
 * thumbnail loader loads thumbnail in a viewport of the UI. ListThumbnailLoader
 * is responsible to return dataUrls of valid thumbnails and fetch them with
 * proper priority.
 *
 * TODOs
 * The following list is a todo list for this class. This list will be deleted
 * after all of them are implemented.
 * * Done: Fetch thumbnails with range based priority control.
 * * Done: Implement cache size limitation.
 * * Done: Modest queueing for low priority thumbnail fetches.
 * * Handle other event types of FileListModel, e.g. sort.
 * * Done: Change ThumbnailLoader to directly return dataUrl.
 * * Handle file types for which generic images are used.
 *
 * @param {!FileListModel} dataModel A file list model.
 * @param {!MetadataCache} metadataCache Metadata cache.
 * @param {!Document} document Document.
 * @param {Function=} opt_thumbnailLoaderConstructor A constructor of thumbnail
 *     loader. This argument is used for testing.
 * @struct
 * @constructor
 * @extends {cr.EventTarget}
 * @suppress {checkStructDictInheritance}
 */
function ListThumbnailLoader(
    dataModel, metadataCache, document, opt_thumbnailLoaderConstructor) {
  /**
   * @type {!FileListModel}
   * @private
   */
  this.dataModel_ = dataModel;

  /**
   * @type {!MetadataCache}
   * @private
   */
  this.metadataCache_ = metadataCache;

  /**
   * @type {!Document}
   * @private
   */
  this.document_ = document;

  /**
   * Constructor of thumbnail loader.
   * @type {!Function}
   * @private
   */
  this.thumbnailLoaderConstructor_ =
      opt_thumbnailLoaderConstructor || ThumbnailLoader;

  /**
   * @type {Object<string, !ListThumbnailLoader.Task>}
   * @private
   */
  this.active_ = {};

  /**
   * @type {LRUCache<!ListThumbnailLoader.ThumbnailData>}
   * @private
   */
  this.cache_ = new LRUCache(ListThumbnailLoader.CACHE_SIZE);

  /**
   * @type {number}
   * @private
   */
  this.beginIndex_ = 0;

  /**
   * @type {number}
   * @private
   */
  this.endIndex_ = 0;

  /**
   * Cursor.
   * @type {number}
   * @private
   */
  this.cursor_ = 0;

  // TODO(yawano): Handle other event types of FileListModel, e.g. sort.
  this.dataModel_.addEventListener('splice', this.onSplice_.bind(this));
}

ListThumbnailLoader.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Number of maximum active tasks.
 * @const {number}
 */
ListThumbnailLoader.NUM_OF_MAX_ACTIVE_TASKS = 5;

/**
 * Number of prefetch requests.
 * @const {number}
 */
ListThumbnailLoader.NUM_OF_PREFETCH = 10;

/**
 * Cache size. Cache size must be larger than sum of high priority range size
 * and number of prefetch tasks.
 * @const {number}
 */
ListThumbnailLoader.CACHE_SIZE = 100;

/**
 * An event handler for splice event of data model. When list is changed, start
 * to rescan items.
 *
 * @param {!Event} event Event
 */
ListThumbnailLoader.prototype.onSplice_ = function(event) {
  this.cursor_ = this.beginIndex_;
  this.continue_();
}

/**
 * Sets high priority range in the list.
 *
 * @param {number} beginIndex Begin index of the range, inclusive.
 * @param {number} endIndex End index of the range, exclusive.
 */
ListThumbnailLoader.prototype.setHighPriorityRange = function(
    beginIndex, endIndex) {
  if (!(beginIndex < endIndex))
    return;

  this.beginIndex_ = beginIndex;
  this.endIndex_ = endIndex;
  this.cursor_ = this.beginIndex_;

  this.continue_();
}

/**
 * Returns a thumbnail of an entry if it is in cache.
 *
 * @return {Object} If the thumbnail is not in cache, this returns null.
 */
ListThumbnailLoader.prototype.getThumbnailFromCache = function(entry) {
  // Since we want to evict cache based on high priority range, we use peek here
  // instead of get.
  return this.cache_.peek(entry.toURL()) || null;
}

/**
 * Enqueues tasks if available.
 */
ListThumbnailLoader.prototype.continue_ = function() {
  // If tasks are running full or all items are scanned, do nothing.
  if (!(Object.keys(this.active_).length <
        ListThumbnailLoader.NUM_OF_MAX_ACTIVE_TASKS) ||
      !(this.cursor_ < this.dataModel_.length) ||
      !(this.cursor_ < this.endIndex_ + ListThumbnailLoader.NUM_OF_PREFETCH)) {
    return;
  }

  var entry = /** @type {Entry} */ (this.dataModel_.item(this.cursor_++));

  // If the entry is a directory, already in cache or fetching, skip it.
  if (entry.isDirectory ||
      this.cache_.get(entry.toURL()) ||
      this.active_[entry.toURL()]) {
    this.continue_();
    return;
  }

  this.enqueue_(entry);
  this.continue_();
}

/**
 * Enqueues a thumbnail fetch task for an entry.
 *
 * @param {!Entry} entry An entry.
 */
ListThumbnailLoader.prototype.enqueue_ = function(entry) {
  var task = new ListThumbnailLoader.Task(
      entry, this.metadataCache_, this.document_,
      this.thumbnailLoaderConstructor_);

  var url = entry.toURL();
  this.active_[url] = task;

  task.fetch().then(function(thumbnail) {
    delete this.active_[url];
    this.cache_.put(url, thumbnail);
    this.dispatchThumbnailLoaded_(thumbnail);
    this.continue_();
  }.bind(this), function() {
    delete this.active_[url];
    this.continue_();
  }.bind(this));
}

/**
 * Dispatches thumbnail loaded event.
 *
 * @param {Object} thumbnail Thumbnail.
 */
ListThumbnailLoader.prototype.dispatchThumbnailLoaded_ = function(thumbnail) {
  // TODO(yawano): Create ThumbnailLoadedEvent class.
  var event = new Event('thumbnailLoaded');
  event.fileUrl = thumbnail.fileUrl;
  event.dataUrl = thumbnail.dataUrl;
  event.width = thumbnail.width;
  event.height = thumbnail.height;
  this.dispatchEvent(event);
};

/**
 * A class to represent thumbnail data.
 * @param {string} fileUrl File url of an original image.
 * @param {string} dataUrl Data url of thumbnail.
 * @param {number} width Width of thumbnail.
 * @param {number} height Height of thumbnail.
 * @constructor
 * @struct
 */
ListThumbnailLoader.ThumbnailData = function(fileUrl, dataUrl, width, height) {
  /**
   * @const {string}
   */
  this.fileUrl = fileUrl;

  /**
   * @const {string}
   */
  this.dataUrl = dataUrl;

  /**
   * @const {number}
   */
  this.width = width;

  /**
   * @const {number}
   */
  this.height = height;
}

/**
 * A task to load thumbnail.
 *
 * @param {!Entry} entry An entry.
 * @param {!MetadataCache} metadataCache Metadata cache.
 * @param {!Document} document Document.
 * @param {!Function} thumbnailLoaderConstructor A constructor of thumbnail
 *     loader.
 * @constructor
 * @struct
 */
ListThumbnailLoader.Task = function(
    entry, metadataCache, document, thumbnailLoaderConstructor) {
  this.entry_ = entry;
  this.metadataCache_ = metadataCache;
  this.document_ = document;
  this.thumbnailLoaderConstructor_ = thumbnailLoaderConstructor;
}

/**
 * Fetches thumbnail.
 *
 * @return {!Promise<!ListThumbnailLoader.ThumbnailData>} A promise which is
 *     resolved when thumbnail is fetched.
 */
ListThumbnailLoader.Task.prototype.fetch = function() {
  return new Promise(function(resolve, reject) {
    this.metadataCache_.getOne(
        this.entry_, 'thumbnail|filesystem|external|media', resolve);
  }.bind(this)).then(function(metadata) {
    return new this.thumbnailLoaderConstructor_(
        this.entry_, ThumbnailLoader.LoaderType.IMAGE, metadata)
        .loadAsDataUrl();
  }.bind(this)).then(function(result) {
    return new ListThumbnailLoader.ThumbnailData(
        this.entry_.toURL(), result.data, result.width, result.height);
  }.bind(this));
}
