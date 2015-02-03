// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TODO(hirono): Remove 'New' from the name after removing old MetadataProvider.
 * @param {!MetadataProviderCache} cache
 * @param {!Array<string>} validPropertyNames
 * @constructor
 * @struct
 * @template T
 */
function NewMetadataProvider(cache, validPropertyNames) {
  /**
   * @private {!MetadataProviderCache}
   * @const
   */
  this.cache_ = cache;

  /**
   * Set of valid property names. Key is the name of property and value is
   * always true.
   * @private {!Object<string, boolean>}
   * @const
   */
  this.validPropertyNames_ = {};
  for (var i = 0; i < validPropertyNames.length; i++) {
    this.validPropertyNames_[validPropertyNames[i]] = true;
  }

  /**
   * @private {!Array<!MetadataProviderCallbackRequest<T>>}
   * @const
   */
  this.callbackRequests_ = [];
}

/**
 * Obtains the metadata for the request.
 * @param {!Array<!MetadataRequest>} requests
 * @return {!Promise<!Array<!T>>}
 * @protected
 */
NewMetadataProvider.prototype.getImpl;

/**
 * Obtains metadata for entries.
 * @param {!Array<!FileEntry>} entries Entries.
 * @param {!Array<string>} names Metadata property names to be obtained.
 * @return {!Promise<!Array<!T>>}
 */
NewMetadataProvider.prototype.get = function(entries, names) {
  // Check if the property name is correct or not.
  for (var i = 0; i < names.length; i++) {
    assert(this.validPropertyNames_[names[i]]);
  }

  // Check if the results are cached or not.
  if (this.cache_.hasFreshCache(entries, names))
    return Promise.resolve(this.getCache(entries, names));

  // The LRU cache may be cached out when the callback is completed.
  // To hold cached values, create snapshot of the cache for entries.
  var requestId = this.cache_.generateRequestId();
  var snapshot = this.cache_.createSnapshot(entries);
  var requests = snapshot.createRequests(entries, names);
  snapshot.startRequests(requestId, requests);
  this.cache_.startRequests(requestId, requests);

  // Register callback.
  var promise = new Promise(function(fulfill, reject) {
    this.callbackRequests_.push(new MetadataProviderCallbackRequest(
        entries, names, snapshot, fulfill, reject));
  }.bind(this));

  // If the requests are not empty, call the requests.
  if (requests.length) {
    this.getImpl(requests).then(function(list) {
      // Obtain requested entries and ensure all the requested properties are
      // contained in the result.
      var requestedEntries = [];
      for (var i = 0; i < requests.length; i++) {
        requestedEntries.push(requests[i].entry);
        for (var j = 0; j < requests[i].names.length; j++) {
          var name = requests[i].names[j];
          if (!(name in list[i]))
            list[i][name] = undefined;
        }
      }

      // Store cache.
      if (this.cache_.storeProperties(requestId, requestedEntries, list)) {
        // TODO(hirono): Dispatch metadata change event here.
      }

      // Invoke callbacks.
      var i = 0;
      while (i < this.callbackRequests_.length) {
        if (this.callbackRequests_[i].storeProperties(
            requestId, requestedEntries, list)) {
          // Callback was called.
          this.callbackRequests_.splice(i, 1);
        } else {
          i++;
        }
      }
    }.bind(this), function(error) {
      // TODO(hirono): Handle rejection here and call rejection callback of
      // MetadataProviderCallbackRequest.
      console.error(error.stack);
    });
  }

  return promise;
};

/**
 * Obtains metadata cache for entries.
 * @param {!Array<!FileEntry>} entries Entries.
 * @param {!Array<string>} names Metadata property names to be obtained.
 * @return {!Array<!T>}
 */
NewMetadataProvider.prototype.getCache = function(entries, names) {
  return this.cache_.get(entries, names);
};

/**
 * @param {!Array<!FileEntry>} entries
 * @param {!Array<string>} names
 * @param {!MetadataCacheSet} cache
 * @param {function(!T):undefined} fulfill
 * @param {function():undefined} reject
 * @constructor
 * @struct
 * @template T
 */
function MetadataProviderCallbackRequest(
    entries, names, cache, fulfill, reject) {
  /**
   * @private {!Array<!FileEntry>}
   * @const
   */
  this.entries_ = entries;

  /**
   * @private {!Array<string>}
   * @const
   */
  this.names_ = names;

  /**
   * @private {!MetadataCacheSet}
   * @const
   */
  this.cache_ = cache;

  /**
   * @private {function(!T):undefined}
   * @const
   */
  this.fulfill_ = fulfill;

  /**
   * @private {function():undefined}
   * @const
   */
  this.reject_ = reject;
}

/**
 * Stores properties to snapshot cache of the callback request.
 * If all the requested property are served, it invokes the callback.
 * @param {number} requestId
 * @param {!Array<!FileEntry>} entries
 * @param {!Array<!Object>} objects
 * @return {boolean} Whether the callback is invoked or not.
 */
MetadataProviderCallbackRequest.prototype.storeProperties = function(
    requestId, entries, objects) {
  this.cache_.storeProperties(requestId, entries, objects);
  if (this.cache_.hasFreshCache(this.entries_, this.names_)) {
    this.fulfill_(this.cache_.get(this.entries_, this.names_));
    return true;
  }
  return false;
};

/**
 * Helper wrapper for LRUCache.
 * @constructor
 * @extends {MetadataCacheSet}
 * @struct
 */
function MetadataProviderCache() {
  // TODO(hirono): Pass the correct maximum size of cache.
  MetadataCacheSet.call(
      this, new MetadataCacheSetStorageForLRUCache(new LRUCache(100)));

  /**
   * @private {number}
   */
  this.requestIdCounter_ = 0;
}

MetadataProviderCache.prototype.__proto__ = MetadataCacheSet.prototype;

/**
 * Generates a unique request ID every time when it is called.
 * @return {number}
 */
MetadataProviderCache.prototype.generateRequestId = function() {
  return this.requestIdCounter_++;
};
