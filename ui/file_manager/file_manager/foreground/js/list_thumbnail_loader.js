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
 * * Implement cache size limitation.
 * * Modest queueing for low priority thumbnail fetches (i.e. not to use up IO
 *     by low priority tasks).
 * * Handle other event types of FileListModel, e.g. sort.
 * * Change ThumbnailLoader to directly return dataUrl.
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
   * @type {Object<string, !Object>}
   * @private
   *
   * TODO(yawano) Add size limitation to the cache.
   */
  this.cache_ = {};

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
   * Cursor begins from 0, and the origin in the list is beginIndex_.
   * @type {number}
   * @private
   */
  this.cursor_ = 0;

  // TODO(yawano) Handle other event types of FileListModel, e.g. sort.
  this.dataModel_.addEventListener('splice', this.onSplice_.bind(this));
}

ListThumbnailLoader.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Number of maximum active tasks.
 * @const {number}
 */
ListThumbnailLoader.NUM_OF_MAX_ACTIVE_TASKS = 5;

/**
 * An event handler for splice event of data model. When list is changed, start
 * to rescan items.
 *
 * @param {!Event} event Event
 */
ListThumbnailLoader.prototype.onSplice_ = function(event) {
  // Delete thumbnails of removed items from cache.
  for (var i = 0; i < event.removed.length; i++) {
    var removedItem = event.removed[i];
    if (this.cache_[removedItem.toURL()])
      delete this.cache_[removedItem.toURL()];
  }

  this.cursor_ = 0;
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
  this.cursor_ = 0;

  this.continue_();
}

/**
 * Returns a thumbnail of an entry if it is in cache.
 *
 * @return {!Object} If the thumbnail is not in cache, this returns null.
 */
ListThumbnailLoader.prototype.getThumbnailFromCache = function(entry) {
  return this.cache_[entry.toURL()] || null;
}

/**
 * Enqueues tasks if available.
 */
ListThumbnailLoader.prototype.continue_ = function() {
  // If tasks are running full or all items are scanned, do nothing.
  if (!(Object.keys(this.active_).length <
        ListThumbnailLoader.NUM_OF_MAX_ACTIVE_TASKS) ||
      !(this.cursor_ < this.dataModel_.length)) {
    return;
  }

  var index = (this.beginIndex_ + this.cursor_) % this.dataModel_.length;
  this.cursor_ += 1;

  var entry = /** @type {Entry} */ (this.dataModel_.item(index));

  // If the entry is a directory, already in cache or fetching, skip it.
  if (entry.isDirectory ||
      this.cache_[entry.toURL()] ||
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

  this.active_[entry.toURL()] = task;

  task.fetch().then(function(thumbnail) {
    delete this.active_[thumbnail.fileUrl];
    this.cache_[thumbnail.fileUrl] = thumbnail;
    this.dispatchThumbnailLoaded_(thumbnail);
    this.continue_();
  }.bind(this));
}

/**
 * Dispatches thumbnail loaded event.
 *
 * @param {Object} thumbnail Thumbnail.
 */
ListThumbnailLoader.prototype.dispatchThumbnailLoaded_ = function(thumbnail) {
  // TODO(yawano) Create ThumbnailLoadedEvent class.
  var event = new Event('thumbnailLoaded');
  event.fileUrl = thumbnail.fileUrl;
  event.dataUrl = thumbnail.dataUrl;
  event.width = thumbnail.width;
  event.height = thumbnail.height;
  this.dispatchEvent(event);
};

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
 * TODO(yawano) Add error handling.
 *
 * @return {!Promise} A promise which is resolved when thumbnail is fetched.
 */
ListThumbnailLoader.Task.prototype.fetch = function() {
  return new Promise(function(resolve, reject) {
    this.metadataCache_.getOne(this.entry_,
        'thumbnail|filesystem|external|media',
        function(metadata) {
          // TODO(yawano) Change ThumbnailLoader to directly return data url of
          // an image.
          var box = this.document_.createElement('div');

          var thumbnailLoader = new this.thumbnailLoaderConstructor_(
              this.entry_,
              ThumbnailLoader.LoaderType.IMAGE,
              metadata);
          thumbnailLoader.load(box,
              ThumbnailLoader.FillMode.FIT,
              ThumbnailLoader.OptimizationMode.DISCARD_DETACHED,
              function(image, transform) {
                // TODO(yawano) Transform an image if necessary.
                var canvas = this.document_.createElement('canvas');
                canvas.width = image.width;
                canvas.height = image.height;

                var context = canvas.getContext('2d');
                context.drawImage(image, 0, 0);

                // TODO(yawano) Create ThumbnailData class.
                resolve({
                  fileUrl: this.entry_.toURL(),
                  dataUrl: canvas.toDataURL('image/jpeg', 0.5),
                  width: image.width,
                  height: image.height
                });
              }.bind(this));
        }.bind(this));
  }.bind(this));
}
