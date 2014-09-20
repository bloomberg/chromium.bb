// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * MetadataCache is a map from Entry to an object containing properties.
 * Properties are divided by types, and all properties of one type are accessed
 * at once.
 * Some of the properties:
 * {
 *   filesystem: size, modificationTime
 *   internal: presence
 *   external: pinned, present, hosted, availableOffline
 *
 *   Following are not fetched for non-present external files.
 *   media: artist, album, title, width, height, imageTransform, etc.
 *   thumbnail: url, transform
 *
 *   Following are always fetched from content, and so force the downloading
 *   of external files. One should use this for required content metadata,
 *   i.e. image orientation.
 *   fetchedMedia: width, height, etc.
 * }
 *
 * Typical usages:
 * {
 *   cache.get([entry1, entry2], 'external|filesystem', function(metadata) {
 *     if (metadata[0].external.pinned && metadata[1].filesystem.size === 0)
 *       alert("Pinned and empty!");
 *   });
 *
 *   cache.set(entry, 'internal', {presence: 'deleted'});
 *
 *   cache.clear([fileEntry1, fileEntry2], 'filesystem');
 *
 *   // Getting fresh value.
 *   cache.clear(entry, 'thumbnail');
 *   cache.getOne(entry, 'thumbnail', function(thumbnail) {
 *     img.src = thumbnail.url;
 *   });
 *
 *   var cached = cache.getCached(entry, 'filesystem');
 *   var size = (cached && cached.size) || UNKNOWN_SIZE;
 * }
 *
 * @param {Array.<MetadataProvider>} providers Metadata providers.
 * @constructor
 */
function MetadataCache(providers) {
  /**
   * Map from Entry (using Entry.toURL) to metadata. Metadata contains
   * |properties| - an hierarchical object of values, and an object for each
   * metadata provider: <prodiver-id>: {time, callbacks}
   * @private
   */
  this.cache_ = {};

  /**
   * List of metadata providers.
   * @private
   */
  this.providers_ = providers;

  /**
   * List of observers added. Each one is an object with fields:
   *   re - regexp of urls;
   *   type - metadata type;
   *   callback - the callback.
   * @private
   */
  this.observers_ = [];
  this.observerId_ = 0;

  this.batchCount_ = 0;
  this.totalCount_ = 0;

  this.currentCacheSize_ = 0;

  /**
   * Time of first get query of the current batch. Items updated later than this
   * will not be evicted.
   *
   * @type {Date}
   * @private
   */
  this.lastBatchStart_ = new Date();

  Object.seal(this);
}

/**
 * Observer type: it will be notified if the changed Entry is exactly the same
 * as the observed Entry.
 */
MetadataCache.EXACT = 0;

/**
 * Observer type: it will be notified if the changed Entry is an immediate child
 * of the observed Entry.
 */
MetadataCache.CHILDREN = 1;

/**
 * Observer type: it will be notified if the changed Entry is a descendant of
 * of the observer Entry.
 */
MetadataCache.DESCENDANTS = 2;

/**
 * Margin of the cache size. This amount of caches may be kept in addition.
 */
MetadataCache.EVICTION_THRESHOLD_MARGIN = 500;

/**
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 * @return {MetadataCache!} The cache with all providers.
 */
MetadataCache.createFull = function(volumeManager) {
  // ExternalProvider should be prior to FileSystemProvider, because it covers
  // FileSystemProvider for files on the external backend, eg. Drive.
  return new MetadataCache([
    new ExternalProvider(volumeManager),
    new FilesystemProvider(),
    new ContentProvider()
  ]);
};

/**
 * Clones metadata entry. Metadata entries may contain scalars, arrays,
 * hash arrays and Date object. Other objects are not supported.
 * @param {Object} metadata Metadata object.
 * @return {Object} Cloned entry.
 */
MetadataCache.cloneMetadata = function(metadata) {
  if (metadata instanceof Array) {
    var result = [];
    for (var index = 0; index < metadata.length; index++) {
      result[index] = MetadataCache.cloneMetadata(metadata[index]);
    }
    return result;
  } else if (metadata instanceof Date) {
    var result = new Date();
    result.setTime(metadata.getTime());
    return result;
  } else if (metadata instanceof Object) {  // Hash array only.
    var result = {};
    for (var property in metadata) {
      if (metadata.hasOwnProperty(property))
        result[property] = MetadataCache.cloneMetadata(metadata[property]);
    }
    return result;
  } else {
    return metadata;
  }
};

/**
 * @return {boolean} Whether all providers are ready.
 */
MetadataCache.prototype.isInitialized = function() {
  for (var index = 0; index < this.providers_.length; index++) {
    if (!this.providers_[index].isInitialized()) return false;
  }
  return true;
};

/**
 * Changes the size of cache by delta value. The actual cache size may be larger
 * than the given value.
 *
 * @param {number} delta The delta size to be changed the cache size by.
 */
MetadataCache.prototype.resizeBy = function(delta) {
  this.currentCacheSize_ += delta;
  if (this.currentCacheSize_ < 0)
    this.currentCacheSize_ = 0;

  if (this.totalCount_ > this.currentEvictionThreshold_())
    this.evict_();
};

/**
 * Returns the current threshold to evict caches. When the number of caches
 * exceeds this, the cache should be evicted.
 * @return {number} Threshold to evict caches.
 * @private
 */
MetadataCache.prototype.currentEvictionThreshold_ = function() {
  return this.currentCacheSize_ * 2 + MetadataCache.EVICTION_THRESHOLD_MARGIN;
};

/**
 * Fetches the metadata, puts it in the cache, and passes to callback.
 * If required metadata is already in the cache, does not fetch it again.
 *
 * @param {Array.<Entry>} entries The list of entries.
 * @param {string} type The metadata type.
 * @param {function(Object)} callback The metadata is passed to callback.
 *     The callback is called asynchronously.
 */
MetadataCache.prototype.get = function(entries, type, callback) {
  this.getInternal_(entries, type, false, callback);
};

/**
 * Fetches the metadata, puts it in the cache, and passes to callback.
 * Even if required metadata is already in the cache, fetches it again.
 *
 * @param {Array.<Entry>} entries The list of entries.
 * @param {string} type The metadata type.
 * @param {function(Object)} callback The metadata is passed to callback.
 *     The callback is called asynchronously.
 */
MetadataCache.prototype.getLatest = function(entries, type, callback) {
  this.getInternal_(entries, type, true, callback);
};

/**
 * Fetches the metadata, puts it in the cache. This is only for internal use.
 *
 * @param {Array.<Entry>} entries The list of entries.
 * @param {string} type The metadata type.
 * @param {boolean} refresh True to get the latest value and refresh the cache,
 *     false to get the value from the cache.
 * @param {function(Object)} callback The metadata is passed to callback.
 *     The callback is called asynchronously.
 * @private
 */
MetadataCache.prototype.getInternal_ =
    function(entries, type, refresh, callback) {
  if (entries.length === 0) {
    if (callback) setTimeout(callback.bind(null, []), 0);
    return;
  }

  var result = [];
  var remaining = entries.length;
  this.startBatchUpdates();

  var onOneItem = function(index, value) {
    result[index] = value;
    remaining--;
    if (remaining === 0) {
      this.endBatchUpdates();
      if (callback) callback(result);
    }
  };

  for (var index = 0; index < entries.length; index++) {
    result.push(null);
    this.getOneInternal_(entries[index],
                         type,
                         refresh,
                         onOneItem.bind(this, index));
  }
};

/**
 * Fetches the metadata for one Entry. See comments to |get|.
 * If required metadata is already in the cache, does not fetch it again.
 *
 * @param {Entry} entry The entry.
 * @param {string} type Metadata type.
 * @param {function(Object)} callback The metadata is passed to callback.
 *     The callback is called asynchronously.
 */
MetadataCache.prototype.getOne = function(entry, type, callback) {
  this.getOneInternal_(entry, type, false, callback);
};

/**
 * Fetches the metadata for one Entry. This is only for internal use.
 *
 * @param {Entry} entry The entry.
 * @param {string} type Metadata type.
 * @param {boolean} refresh True to get the latest value and refresh the cache,
 *     false to get the value from the cache.
 * @param {function(Object)} callback The metadata is passed to callback.
 *     The callback is called asynchronously.
 * @private
 */
MetadataCache.prototype.getOneInternal_ =
    function(entry, type, refresh, callback) {
  if (type.indexOf('|') !== -1) {
    var types = type.split('|');
    var result = {};
    var typesLeft = types.length;

    var onOneType = function(requestedType, metadata) {
      result[requestedType] = metadata;
      typesLeft--;
      if (typesLeft === 0) callback(result);
    };

    for (var index = 0; index < types.length; index++) {
      this.getOneInternal_(entry, types[index], refresh,
          onOneType.bind(null, types[index]));
    }
    return;
  }

  callback = callback || function() {};

  var entryURL = entry.toURL();
  if (!(entryURL in this.cache_)) {
    this.cache_[entryURL] = this.createEmptyItem_();
    this.totalCount_++;
  }

  var item = this.cache_[entryURL];

  if (!refresh && type in item.properties) {
    // Uses cache, if available and not on the 'refresh' mode.
    setTimeout(callback.bind(null, item.properties[type]), 0);
    return;
  }

  this.startBatchUpdates();
  var providers = this.providers_.slice();
  var currentProvider;
  var self = this;

  var queryProvider = function() {
    var id = currentProvider.getId();

    // If on 'refresh'-mode, replaces the callback array. The previous
    // array may be remaining in the closure captured by previous tasks.
    if (refresh)
      item[id].callbacks = [];
    var fetchedCallbacks = item[id].callbacks;

    var onFetched = function() {
      if (type in item.properties) {
        self.endBatchUpdates();
        // Got properties from provider.
        callback(item.properties[type]);
      } else {
        tryNextProvider();
      }
    };

    var onProviderProperties = function(properties) {
      var callbacks = fetchedCallbacks.splice(0);
      item.time = new Date();
      self.mergeProperties_(entry, properties);

      for (var index = 0; index < callbacks.length; index++) {
        callbacks[index]();
      }
    };

    fetchedCallbacks.push(onFetched);

    // Querying now.
    if (fetchedCallbacks.length === 1)
      currentProvider.fetch(entry, type, onProviderProperties);
  };

  var tryNextProvider = function() {
    if (providers.length === 0) {
      self.endBatchUpdates();
      setTimeout(callback.bind(null, item.properties[type] || null), 0);
      return;
    }

    currentProvider = providers.shift();
    if (currentProvider.supportsEntry(entry) &&
        currentProvider.providesType(type)) {
      queryProvider();
    } else {
      tryNextProvider();
    }
  };

  tryNextProvider();
};

/**
 * Returns the cached metadata value, or |null| if not present.
 * @param {Entry} entry Entry.
 * @param {string} type The metadata type.
 * @return {Object} The metadata or null.
 */
MetadataCache.prototype.getCached = function(entry, type) {
  // Entry.cachedUrl may be set in DirectoryContents.onNewEntries_().
  // See the comment there for detail.
  var entryURL = entry.cachedUrl || entry.toURL();
  var cache = this.cache_[entryURL];
  return cache ? (cache.properties[type] || null) : null;
};

/**
 * Puts the metadata into cache
 * @param {Entry|Array.<Entry>} entries The list of entries. May be just a
 *     single entry.
 * @param {string} type The metadata type.
 * @param {Array.<Object>} values List of corresponding metadata values.
 */
MetadataCache.prototype.set = function(entries, type, values) {
  if (!(entries instanceof Array)) {
    entries = [entries];
    values = [values];
  }

  this.startBatchUpdates();
  for (var index = 0; index < entries.length; index++) {
    var entryURL = entries[index].toURL();
    if (!(entryURL in this.cache_)) {
      this.cache_[entryURL] = this.createEmptyItem_();
      this.totalCount_++;
    }
    this.cache_[entryURL].properties[type] = values[index];
    this.notifyObservers_(entries[index], type);
  }
  this.endBatchUpdates();
};

/**
 * Clears the cached metadata values.
 * @param {Entry|Array.<Entry>} entries The list of entries. May be just a
 *     single entry.
 * @param {string} type The metadata types or * for any type.
 */
MetadataCache.prototype.clear = function(entries, type) {
  if (!(entries instanceof Array))
    entries = [entries];

  this.clearByUrl(
      entries.map(function(entry) { return entry.toURL(); }),
      type);
};

/**
 * Clears the cached metadata values. This method takes an URL since some items
 * may be already removed and can't be fetches their entry.
 *
 * @param {Array.<string>} urls The list of URLs.
 * @param {string} type The metadata types or * for any type.
 */
MetadataCache.prototype.clearByUrl = function(urls, type) {
  var types = type.split('|');

  for (var index = 0; index < urls.length; index++) {
    var entryURL = urls[index];
    if (entryURL in this.cache_) {
      if (type === '*') {
        this.cache_[entryURL].properties = {};
      } else {
        for (var j = 0; j < types.length; j++) {
          var type = types[j];
          delete this.cache_[entryURL].properties[type];
        }
      }
    }
  }
};

/**
 * Clears the cached metadata values recursively.
 * @param {Entry} entry An entry to be cleared recursively from cache.
 * @param {string} type The metadata types or * for any type.
 */
MetadataCache.prototype.clearRecursively = function(entry, type) {
  var types = type.split('|');
  var keys = Object.keys(this.cache_);
  var entryURL = entry.toURL();

  for (var index = 0; index < keys.length; index++) {
    var cachedEntryURL = keys[index];
    if (cachedEntryURL.substring(0, entryURL.length) === entryURL) {
      if (type === '*') {
        this.cache_[cachedEntryURL].properties = {};
      } else {
        for (var j = 0; j < types.length; j++) {
          var type = types[j];
          delete this.cache_[cachedEntryURL].properties[type];
        }
      }
    }
  }
};

/**
 * Adds an observer, which will be notified when metadata changes.
 * @param {Entry} entry The root entry to look at.
 * @param {number} relation This defines, which items will trigger the observer.
 *     See comments to |MetadataCache.EXACT| and others.
 * @param {string} type The metadata type.
 * @param {function(Array.<Entry>, Object.<string, Object>)} observer Map of
 *     entries and corresponding metadata values are passed to this callback.
 * @return {number} The observer id, which can be used to remove it.
 */
MetadataCache.prototype.addObserver = function(
    entry, relation, type, observer) {
  var entryURL = entry.toURL();
  var re;
  if (relation === MetadataCache.CHILDREN)
    re = entryURL + '(/[^/]*)?';
  else if (relation === MetadataCache.DESCENDANTS)
    re = entryURL + '(/.*)?';
  else
    re = entryURL;

  var id = ++this.observerId_;
  this.observers_.push({
    re: new RegExp('^' + re + '$'),
    type: type,
    callback: observer,
    id: id,
    pending: {}
  });

  return id;
};

/**
 * Removes the observer.
 * @param {number} id Observer id.
 * @return {boolean} Whether observer was removed or not.
 */
MetadataCache.prototype.removeObserver = function(id) {
  for (var index = 0; index < this.observers_.length; index++) {
    if (this.observers_[index].id === id) {
      this.observers_.splice(index, 1);
      return true;
    }
  }
  return false;
};

/**
 * Start batch updates.
 */
MetadataCache.prototype.startBatchUpdates = function() {
  this.batchCount_++;
  if (this.batchCount_ === 1)
    this.lastBatchStart_ = new Date();
};

/**
 * End batch updates. Notifies observers if all nested updates are finished.
 */
MetadataCache.prototype.endBatchUpdates = function() {
  this.batchCount_--;
  if (this.batchCount_ !== 0) return;
  if (this.totalCount_ > this.currentEvictionThreshold_())
    this.evict_();
  for (var index = 0; index < this.observers_.length; index++) {
    var observer = this.observers_[index];
    var entries = [];
    var properties = {};
    for (var entryURL in observer.pending) {
      if (observer.pending.hasOwnProperty(entryURL) &&
          entryURL in this.cache_) {
        var entry = observer.pending[entryURL];
        entries.push(entry);
        properties[entryURL] =
            this.cache_[entryURL].properties[observer.type] || null;
      }
    }
    observer.pending = {};
    if (entries.length > 0) {
      observer.callback(entries, properties);
    }
  }
};

/**
 * Notifies observers or puts the data to pending list.
 * @param {Entry} entry Changed entry.
 * @param {string} type Metadata type.
 * @private
 */
MetadataCache.prototype.notifyObservers_ = function(entry, type) {
  var entryURL = entry.toURL();
  for (var index = 0; index < this.observers_.length; index++) {
    var observer = this.observers_[index];
    if (observer.type === type && observer.re.test(entryURL)) {
      if (this.batchCount_ === 0) {
        // Observer expects array of urls and map of properties.
        var property = {};
        property[entryURL] = this.cache_[entryURL].properties[type] || null;
        observer.callback(
            [entry], property);
      } else {
        observer.pending[entryURL] = entry;
      }
    }
  }
};

/**
 * Removes the oldest items from the cache.
 * This method never removes the items from last batch.
 * @private
 */
MetadataCache.prototype.evict_ = function() {
  var toRemove = [];

  // We leave only a half of items, so we will not call evict_ soon again.
  var desiredCount = this.currentEvictionThreshold_();
  var removeCount = this.totalCount_ - desiredCount;
  for (var url in this.cache_) {
    if (this.cache_.hasOwnProperty(url) &&
        this.cache_[url].time < this.lastBatchStart_) {
      toRemove.push(url);
    }
  }

  toRemove.sort(function(a, b) {
    var aTime = this.cache_[a].time;
    var bTime = this.cache_[b].time;
    return aTime < bTime ? -1 : aTime > bTime ? 1 : 0;
  }.bind(this));

  removeCount = Math.min(removeCount, toRemove.length);
  this.totalCount_ -= removeCount;
  for (var index = 0; index < removeCount; index++) {
    delete this.cache_[toRemove[index]];
  }
};

/**
 * @return {Object} Empty cache item.
 * @private
 */
MetadataCache.prototype.createEmptyItem_ = function() {
  var item = {properties: {}};
  for (var index = 0; index < this.providers_.length; index++) {
    item[this.providers_[index].getId()] = {callbacks: []};
  }
  return item;
};

/**
 * Caches all the properties from data to cache entry for the entry.
 * @param {Entry} entry The file entry.
 * @param {Object} data The properties.
 * @private
 */
MetadataCache.prototype.mergeProperties_ = function(entry, data) {
  if (data === null) return;
  var entryURL = entry.toURL();
  if (!(entryURL in this.cache_)) {
    this.cache_[entryURL] = this.createEmptyItem_();
    this.totalCount_++;
  }
  var properties = this.cache_[entryURL].properties;
  for (var type in data) {
    if (data.hasOwnProperty(type)) {
      properties[type] = data[type];
      this.notifyObservers_(entry, type);
    }
  }
};

/**
 * Base class for metadata providers.
 * @constructor
 */
function MetadataProvider() {
}

/**
 * @param {Entry} entry The entry.
 * @return {boolean} Whether this provider supports the entry.
 */
MetadataProvider.prototype.supportsEntry = function(entry) { return false; };

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
MetadataProvider.prototype.providesType = function(type) { return false; };

/**
 * @return {string} Unique provider id.
 */
MetadataProvider.prototype.getId = function() { return ''; };

/**
 * @return {boolean} Whether provider is ready.
 */
MetadataProvider.prototype.isInitialized = function() { return true; };

/**
 * Fetches the metadata. It's suggested to return all the metadata this provider
 * can fetch at once.
 * @param {Entry} entry File entry.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value. This callback must be called asynchronously.
 */
MetadataProvider.prototype.fetch = function(entry, type, callback) {
  throw new Error('Default metadata provider cannot fetch.');
};


/**
 * Provider of filesystem metadata.
 * This provider returns the following objects:
 * filesystem: { size, modificationTime }
 * @constructor
 */
function FilesystemProvider() {
  MetadataProvider.call(this);
}

FilesystemProvider.prototype = {
  __proto__: MetadataProvider.prototype
};

/**
 * @param {Entry} entry The entry.
 * @return {boolean} Whether this provider supports the entry.
 */
FilesystemProvider.prototype.supportsEntry = function(entry) {
  return true;
};

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
FilesystemProvider.prototype.providesType = function(type) {
  return type === 'filesystem';
};

/**
 * @return {string} Unique provider id.
 */
FilesystemProvider.prototype.getId = function() { return 'filesystem'; };

/**
 * Fetches the metadata.
 * @param {Entry} entry File entry.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value. This callback is called asynchronously.
 */
FilesystemProvider.prototype.fetch = function(
    entry, type, callback) {
  function onError(error) {
    callback(null);
  }

  function onMetadata(entry, metadata) {
    callback({
      filesystem: {
        size: (entry.isFile ? (metadata.size || 0) : -1),
        modificationTime: metadata.modificationTime
      }
    });
  }

  entry.getMetadata(onMetadata.bind(null, entry), onError);
};

/**
 * Provider of metadata for entries on the external file system backend.
 * This provider returns the following objects:
 *     external: { pinned, hosted, present, customIconUrl, etc. }
 *     thumbnail: { url, transform }
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 * @constructor
 */
function ExternalProvider(volumeManager) {
  MetadataProvider.call(this);

  /**
   * @type {VolumeManagerWrapper}
   * @private
   */
  this.volumeManager_ = volumeManager;

  // We batch metadata fetches into single API call.
  this.entries_ = [];
  this.callbacks_ = [];
  this.scheduled_ = false;

  this.callApiBound_ = this.callApi_.bind(this);
}

ExternalProvider.prototype = {
  __proto__: MetadataProvider.prototype
};

/**
 * @param {Entry} entry The entry.
 * @return {boolean} Whether this provider supports the entry.
 */
ExternalProvider.prototype.supportsEntry = function(entry) {
  var locationInfo = this.volumeManager_.getLocationInfo(entry);
  // TODO(mtomasz): Add support for provided file systems.
  return locationInfo && locationInfo.isDriveBased;
};

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
ExternalProvider.prototype.providesType = function(type) {
  return type === 'external' || type === 'thumbnail' ||
      type === 'media' || type === 'filesystem';
};

/**
 * @return {string} Unique provider id.
 */
ExternalProvider.prototype.getId = function() { return 'external'; };

/**
 * Fetches the metadata.
 * @param {Entry} entry File entry.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value. This callback is called asynchronously.
 */
ExternalProvider.prototype.fetch = function(entry, type, callback) {
  this.entries_.push(entry);
  this.callbacks_.push(callback);
  if (!this.scheduled_) {
    this.scheduled_ = true;
    setTimeout(this.callApiBound_, 0);
  }
};

/**
 * Schedules the API call.
 * @private
 */
ExternalProvider.prototype.callApi_ = function() {
  this.scheduled_ = false;

  var entries = this.entries_;
  var callbacks = this.callbacks_;
  this.entries_ = [];
  this.callbacks_ = [];
  var self = this;

  // TODO(mtomasz): Move conversion from entry to url to custom bindings.
  // crbug.com/345527.
  var entryURLs = util.entriesToURLs(entries);
  chrome.fileManagerPrivate.getEntryProperties(
      entryURLs,
      function(propertiesList) {
        console.assert(propertiesList.length === callbacks.length);
        for (var i = 0; i < callbacks.length; i++) {
          callbacks[i](self.convert_(propertiesList[i], entries[i]));
        }
      });
};

/**
 * Converts API metadata to internal format.
 * @param {Object} data Metadata from API call.
 * @param {Entry} entry File entry.
 * @return {Object} Metadata in internal format.
 * @private
 */
ExternalProvider.prototype.convert_ = function(data, entry) {
  var result = {};
  result.external = {
    present: data.isPresent,
    pinned: data.isPinned,
    hosted: data.isHosted,
    imageWidth: data.imageWidth,
    imageHeight: data.imageHeight,
    imageRotation: data.imageRotation,
    availableOffline: data.isAvailableOffline,
    availableWhenMetered: data.isAvailableWhenMetered,
    customIconUrl: data.customIconUrl || '',
    contentMimeType: data.contentMimeType || '',
    sharedWithMe: data.sharedWithMe,
    shared: data.shared,
    thumbnailUrl: data.thumbnailUrl  // Thumbnail passed from external server.
  };

  result.filesystem = {
    size: (entry.isFile ? (data.fileSize || 0) : -1),
    modificationTime: new Date(data.lastModifiedTime)
  };

  if (data.isPresent) {
    // If the file is present, don't fill the thumbnail here and allow to
    // generate it by next providers.
    result.thumbnail = null;
  } else if ('thumbnailUrl' in data) {
    result.thumbnail = {
      url: data.thumbnailUrl,
      transform: null
    };
  } else {
    // Not present in cache, so do not allow to generate it by next providers.
    result.thumbnail = {url: '', transform: null};
  }

  // If present in cache, then allow to fetch media by next providers.
  result.media = data.isPresent ? null : {};

  return result;
};


/**
 * Provider of content metadata.
 * This provider returns the following objects:
 * thumbnail: { url, transform }
 * media: { artist, album, title, width, height, imageTransform, etc. }
 * fetchedMedia: { same fields here }
 * @constructor
 */
function ContentProvider() {
  MetadataProvider.call(this);

  // Pass all URLs to the metadata reader until we have a correct filter.
  this.urlFilter_ = /.*/;

  var dispatcher = new SharedWorker(ContentProvider.WORKER_SCRIPT).port;
  dispatcher.onmessage = this.onMessage_.bind(this);
  dispatcher.postMessage({verb: 'init'});
  dispatcher.start();
  this.dispatcher_ = dispatcher;

  // Initialization is not complete until the Worker sends back the
  // 'initialized' message.  See below.
  this.initialized_ = false;

  // Map from Entry.toURL() to callback.
  // Note that simultaneous requests for same url are handled in MetadataCache.
  this.callbacks_ = {};
}

/**
 * Path of a worker script.
 * @type {string}
 * @const
 */
ContentProvider.WORKER_SCRIPT =
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
    'foreground/js/metadata/metadata_dispatcher.js';

ContentProvider.prototype = {
  __proto__: MetadataProvider.prototype
};

/**
 * @param {Entry} entry The entry.
 * @return {boolean} Whether this provider supports the entry.
 */
ContentProvider.prototype.supportsEntry = function(entry) {
  return entry.toURL().match(this.urlFilter_);
};

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
ContentProvider.prototype.providesType = function(type) {
  return type === 'thumbnail' || type === 'fetchedMedia' || type === 'media';
};

/**
 * @return {string} Unique provider id.
 */
ContentProvider.prototype.getId = function() { return 'content'; };

/**
 * Fetches the metadata.
 * @param {Entry} entry File entry.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value. This callback is called asynchronously.
 */
ContentProvider.prototype.fetch = function(entry, type, callback) {
  if (entry.isDirectory) {
    setTimeout(callback.bind(null, {}), 0);
    return;
  }
  var entryURL = entry.toURL();
  this.callbacks_[entryURL] = callback;
  this.dispatcher_.postMessage({verb: 'request', arguments: [entryURL]});
};

/**
 * Dispatch a message from a metadata reader to the appropriate on* method.
 * @param {Object} event The event.
 * @private
 */
ContentProvider.prototype.onMessage_ = function(event) {
  var data = event.data;

  var methodName =
      'on' + data.verb.substr(0, 1).toUpperCase() + data.verb.substr(1) + '_';

  if (!(methodName in this)) {
    console.error('Unknown message from metadata reader: ' + data.verb, data);
    return;
  }

  this[methodName].apply(this, data.arguments);
};

/**
 * @return {boolean} Whether provider is ready.
 */
ContentProvider.prototype.isInitialized = function() {
  return this.initialized_;
};

/**
 * Handles the 'initialized' message from the metadata reader Worker.
 * @param {Object} regexp Regexp of supported urls.
 * @private
 */
ContentProvider.prototype.onInitialized_ = function(regexp) {
  this.urlFilter_ = regexp;

  // Tests can monitor for this state with
  // ExtensionTestMessageListener listener("worker-initialized");
  // ASSERT_TRUE(listener.WaitUntilSatisfied());
  // Automated tests need to wait for this, otherwise we crash in
  // browser_test cleanup because the worker process still has
  // URL requests in-flight.
  util.testSendMessage('worker-initialized');
  this.initialized_ = true;
};

/**
 * Converts content metadata from parsers to the internal format.
 * @param {Object} metadata The content metadata.
 * @param {Object=} opt_result The internal metadata object ot put result in.
 * @return {Object!} Converted metadata.
 */
ContentProvider.ConvertContentMetadata = function(metadata, opt_result) {
  var result = opt_result || {};

  if ('thumbnailURL' in metadata) {
    metadata.thumbnailTransform = metadata.thumbnailTransform || null;
    result.thumbnail = {
      url: metadata.thumbnailURL,
      transform: metadata.thumbnailTransform
    };
  }

  for (var key in metadata) {
    if (metadata.hasOwnProperty(key)) {
      if (!result.media)
        result.media = {};
      result.media[key] = metadata[key];
    }
  }

  if (result.media)
    result.fetchedMedia = result.media;

  return result;
};

/**
 * Handles the 'result' message from the worker.
 * @param {string} url File url.
 * @param {Object} metadata The metadata.
 * @private
 */
ContentProvider.prototype.onResult_ = function(url, metadata) {
  var callback = this.callbacks_[url];
  delete this.callbacks_[url];
  callback(ContentProvider.ConvertContentMetadata(metadata));
};

/**
 * Handles the 'error' message from the worker.
 * @param {string} url File entry.
 * @param {string} step Step failed.
 * @param {string} error Error description.
 * @param {Object?} metadata The metadata, if available.
 * @private
 */
ContentProvider.prototype.onError_ = function(url, step, error, metadata) {
  if (MetadataCache.log)  // Avoid log spam by default.
    console.warn('metadata: ' + url + ': ' + step + ': ' + error);
  metadata = metadata || {};
  // Prevent asking for thumbnail again.
  metadata.thumbnailURL = '';
  this.onResult_(url, metadata);
};

/**
 * Handles the 'log' message from the worker.
 * @param {Array.<*>} arglist Log arguments.
 * @private
 */
ContentProvider.prototype.onLog_ = function(arglist) {
  if (MetadataCache.log)  // Avoid log spam by default.
    console.log.apply(console, ['metadata:'].concat(arglist));
};
