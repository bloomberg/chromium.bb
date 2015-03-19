// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A thumbnail loader for list style UI.
 *
 * ListThumbnailLoader is a thubmanil loader designed for list style ui. List
 * thumbnail loader loads thumbnail in a viewport of the UI. ListThumbnailLoader
 * is responsible to return dataUrls of thumbnails and fetch them with proper
 * priority.
 *
 * @param {!FileListModel} dataModel A file list model.
 * @param {!ThumbnailModel} thumbnailModel Thumbnail metadata model.
 * @param {!VolumeManagerWrapper} volumeManager Volume manager.
 * @param {Function=} opt_thumbnailLoaderConstructor A constructor of thumbnail
 *     loader. This argument is used for testing.
 * @struct
 * @constructor
 * @extends {cr.EventTarget}
 * @suppress {checkStructDictInheritance}
 */
function ListThumbnailLoader(
    dataModel, thumbnailModel, volumeManager, opt_thumbnailLoaderConstructor) {
  /**
   * @private {!FileListModel}
   */
  this.dataModel_ = dataModel;

  /**
   * @private {!ThumbnailModel}
   */
  this.thumbnailModel_ = thumbnailModel;

  /**
   * @private {!VolumeManagerWrapper}
   */
  this.volumeManager_ = volumeManager;

  /**
   * Constructor of thumbnail loader.
   * @private {!Function}
   */
  this.thumbnailLoaderConstructor_ =
      opt_thumbnailLoaderConstructor || ThumbnailLoader;

  /**
   * @private {Object<string, !ListThumbnailLoader.Task>}
   */
  this.active_ = {};

  /**
   * @private {LRUCache<!ListThumbnailLoader.ThumbnailData>}
   */
  this.cache_ = new LRUCache(ListThumbnailLoader.CACHE_SIZE);

  /**
   * @private {number}
   */
  this.beginIndex_ = 0;

  /**
   * @private {number}
   */
  this.endIndex_ = 0;

  /**
   * Cursor.
   * @private {number}
   */
  this.cursor_ = 0;

  /**
   * Current volume type.
   * @private {?ListThumbnailLoader.VolumeType}
   */
  this.currentVolumeType_ = null;

  this.dataModel_.addEventListener('splice', this.onSplice_.bind(this));
  this.dataModel_.addEventListener('sorted', this.onSorted_.bind(this));
  this.dataModel_.addEventListener('change', this.onChange_.bind(this));
}

ListThumbnailLoader.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Cache size. Cache size must be larger than sum of high priority range size
 * and number of prefetch tasks.
 * @const {number}
 */
ListThumbnailLoader.CACHE_SIZE = 500;

/**
 * Volume type for testing.
 * @const {string}
 */
ListThumbnailLoader.TEST_VOLUME_TYPE = 'test_volume_type';

/**
 * Number of maximum active tasks for testing.
 * @type {number}
 */
ListThumbnailLoader.numOfMaxActiveTasksForTest = 2;

/**
 * @typedef {(VolumeManagerCommon.VolumeType|string)}
 */
ListThumbnailLoader.VolumeType;

/**
 * Gets number of prefetch requests. This number changes based on current volume
 * type.
 * @return {number} Number of prefetch requests.
 * @private
 */
ListThumbnailLoader.prototype.getNumOfPrefetch_ = function () {
  switch (/** @type {?ListThumbnailLoader.VolumeType} */
      (this.currentVolumeType_)) {
    case VolumeManagerCommon.VolumeType.MTP:
      return 0;
    case ListThumbnailLoader.TEST_VOLUME_TYPE:
      return 1;
    default:
      return 20;
  }
};

/**
 * Gets maximum number of active thumbnail fetch tasks. This number changes
 * based on current volume type.
 * @return {number} Maximum number of active thumbnail fetch tasks.
 * @private
 */
ListThumbnailLoader.prototype.getNumOfMaxActiveTasks_ = function() {
  switch (/** @type {?ListThumbnailLoader.VolumeType} */
      (this.currentVolumeType_)) {
    case VolumeManagerCommon.VolumeType.MTP:
      return 1;
    case ListThumbnailLoader.TEST_VOLUME_TYPE:
      return ListThumbnailLoader.numOfMaxActiveTasksForTest;
    default:
      return 10;
  }
};

/**
 * An event handler for splice event of data model. When list is changed, start
 * to rescan items.
 *
 * @param {!Event} event Event
 */
ListThumbnailLoader.prototype.onSplice_ = function(event) {
  this.cursor_ = this.beginIndex_;
  this.continue_();
};

/**
 * An event handler for sorted event of data model. When list is sorted, start
 * to rescan items.
 *
 * @param {!Event} event Event
 */
ListThumbnailLoader.prototype.onSorted_ = function(event) {
  this.cursor_ = this.beginIndex_;
  this.continue_();
};

/**
 * An event handler for change event of data model.
 *
 * @param {!Event} event Event
 */
ListThumbnailLoader.prototype.onChange_ = function(event) {
  // Mark the thumbnail in cache as invalid.
  var entry = this.dataModel_.item(event.index);
  var cachedThumbnail = this.cache_.peek(entry.toURL());
  if (cachedThumbnail)
    cachedThumbnail.outdated = true;

  this.cursor_ = this.beginIndex_;
  this.continue_();
};

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
};

/**
 * Returns a thumbnail of an entry if it is in cache. This method returns
 * thumbnail even if the thumbnail is outdated.
 *
 * @return {ListThumbnailLoader.ThumbnailData} If the thumbnail is not in cache,
 *     this returns null.
 */
ListThumbnailLoader.prototype.getThumbnailFromCache = function(entry) {
  // Since we want to evict cache based on high priority range, we use peek here
  // instead of get.
  return this.cache_.peek(entry.toURL()) || null;
};

/**
 * Enqueues tasks if available.
 */
ListThumbnailLoader.prototype.continue_ = function() {
  // If all items are scanned, do nothing.
  if (!(this.cursor_ < this.dataModel_.length))
    return;

  var entry = /** @type {Entry} */ (this.dataModel_.item(this.cursor_));

  // Check volume type for optimizing the parameters.
  var volumeInfo = this.volumeManager_.getVolumeInfo(entry);
  this.currentVolumeType_ = volumeInfo ? volumeInfo.volumeType : null;

  // If tasks are running full or all items are scanned, do nothing.
  if (!(Object.keys(this.active_).length < this.getNumOfMaxActiveTasks_()) ||
      !(this.cursor_ < this.endIndex_ + this.getNumOfPrefetch_())) {
    return;
  }

  // If the entry is a directory, already in cache as valid or fetching, skip.
  var thumbnail = this.cache_.get(entry.toURL());
  if (entry.isDirectory ||
      (thumbnail && !thumbnail.outdated) ||
      this.active_[entry.toURL()]) {
    this.cursor_++;
    this.continue_();
    return;
  }

  this.enqueue_(this.cursor_, entry);
  this.cursor_++;
  this.continue_();
};

/**
 * Enqueues a thumbnail fetch task for an entry.
 *
 * @param {number} index Index of an entry in current data model.
 * @param {!Entry} entry An entry.
 */
ListThumbnailLoader.prototype.enqueue_ = function(index, entry) {
  var task = new ListThumbnailLoader.Task(
      entry, this.thumbnailModel_, this.thumbnailLoaderConstructor_);

  var url = entry.toURL();
  this.active_[url] = task;

  task.fetch().then(function(thumbnail) {
    delete this.active_[url];
    this.cache_.put(url, thumbnail);
    this.dispatchThumbnailLoaded_(index, thumbnail);
    this.continue_();
  }.bind(this), function() {
    delete this.active_[url];
    this.continue_();
  }.bind(this));
};

/**
 * Dispatches thumbnail loaded event.
 *
 * @param {number} index Index of an original image in the data model.
 * @param {!ListThumbnailLoader.ThumbnailData} thumbnail Thumbnail.
 */
ListThumbnailLoader.prototype.dispatchThumbnailLoaded_ = function(
    index, thumbnail) {
  // Update index if it's already invalid, i.e. index may be invalid if some
  // change had happened in the data model during thumbnail fetch.
  var item = this.dataModel_.item(index);
  if (item && item.toURL() !== thumbnail.fileUrl) {
    index = -1;;
    for (var i = 0; i < this.dataModel_.length; i++) {
      if (this.dataModel_.item(i).toURL() === thumbnail.fileUrl) {
        index = i;
        break;
      }
    }
  }

  if (index > -1) {
    this.dispatchEvent(
        new ListThumbnailLoader.ThumbnailLoadedEvent(index, thumbnail));
  }
};

/**
 * Thumbnail loaded event.
 * @param {number} index Index of an original image in the current data model.
 * @param {!ListThumbnailLoader.ThumbnailData} thumbnail Thumbnail.
 * @extends {Event}
 * @suppress {checkStructDictInheritance}
 * @constructor
 * @struct
 */
ListThumbnailLoader.ThumbnailLoadedEvent = function(index, thumbnail) {
  var event = new Event('thumbnailLoaded');

  /** @type {number} */
  event.index = index;

  /** @type {string}*/
  event.fileUrl = thumbnail.fileUrl;

  /** @type {string} */
  event.dataUrl = thumbnail.dataUrl;

  /** @type {number} */
  event.width = thumbnail.width;

  /** @type {number}*/
  event.height = thumbnail.height;

  return event;
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

  /**
   * @type {boolean}
   */
  this.outdated = false;
};

/**
 * A task to load thumbnail.
 *
 * @param {!Entry} entry An entry.
 * @param {!ThumbnailModel} thumbnailModel Metadata cache.
 * @param {!Function} thumbnailLoaderConstructor A constructor of thumbnail
 *     loader.
 * @constructor
 * @struct
 */
ListThumbnailLoader.Task = function(
    entry, thumbnailModel, thumbnailLoaderConstructor) {
  this.entry_ = entry;
  this.thumbnailModel_ = thumbnailModel;
  this.thumbnailLoaderConstructor_ = thumbnailLoaderConstructor;
};

/**
 * Fetches thumbnail.
 *
 * @return {!Promise<!ListThumbnailLoader.ThumbnailData>} A promise which is
 *     resolved when thumbnail is fetched.
 */
ListThumbnailLoader.Task.prototype.fetch = function() {
  return this.thumbnailModel_.get([this.entry_]).then(function(metadatas) {
    // When an error happens during metadata fetch, abort here.
    if (metadatas[0].thumbnail.urlError)
      throw metadatas[0].thumbnail.urlError;

    return new this.thumbnailLoaderConstructor_(
        this.entry_, ThumbnailLoader.LoaderType.IMAGE, metadatas[0])
        .loadAsDataUrl();
  }.bind(this)).then(function(result) {
    return new ListThumbnailLoader.ThumbnailData(
        this.entry_.toURL(), result.data, result.width, result.height);
  }.bind(this));
};
