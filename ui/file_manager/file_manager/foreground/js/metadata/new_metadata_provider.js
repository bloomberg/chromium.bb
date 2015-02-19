// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TODO(hirono): Remove 'New' from the name after removing old MetadataProvider.
 * @param {!MetadataProviderCache} cache
 * @param {!Array<string>} validPropertyNames
 * @constructor
 * @struct
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
 * @return {!Promise<!Array<!MetadataItem>>} Promise with obtained metadata. It
 *     should not return rejected promise. Instead it should return undefined
 *     property for property error, and should return empty MetadataItem for
 *     entry error.
 * @protected
 */
NewMetadataProvider.prototype.getImpl;

/**
 * Obtains metadata for entries.
 * @param {!Array<!Entry>} entries Entries.
 * @param {!Array<string>} names Metadata property names to be obtained.
 * @return {!Promise<!Array<!MetadataItem>>}
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
  var promise = new Promise(function(fulfill) {
    this.callbackRequests_.push(new MetadataProviderCallbackRequest(
        entries, names, snapshot, fulfill));
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
      this.cache_.storeProperties(requestId, requestedEntries, list);

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
    }.bind(this));
  }

  return promise;
};

/**
 * Obtains metadata cache for entries.
 * @param {!Array<!Entry>} entries Entries.
 * @param {!Array<string>} names Metadata property names to be obtained.
 * @return {!Array<!MetadataItem>}
 */
NewMetadataProvider.prototype.getCache = function(entries, names) {
  // Check if the property name is correct or not.
  for (var i = 0; i < names.length; i++) {
    assert(this.validPropertyNames_[names[i]]);
  }
  return this.cache_.get(entries, names);
};

/**
 * @param {!Array<!Entry>} entries
 * @param {!Array<string>} names
 * @param {!MetadataCacheSet} cache
 * @param {function(!MetadataItem):undefined} fulfill
 * @constructor
 * @struct
 */
function MetadataProviderCallbackRequest(entries, names, cache, fulfill) {
  /**
   * @private {!Array<!Entry>}
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
   * @private {function(!MetadataItem):undefined}
   * @const
   */
  this.fulfill_ = fulfill;
}

/**
 * Stores properties to snapshot cache of the callback request.
 * If all the requested property are served, it invokes the callback.
 * @param {number} requestId
 * @param {!Array<!Entry>} entries
 * @param {!Array<!MetadataItem>} objects
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
  MetadataCacheSet.call(this, new MetadataCacheSetStorageForObject({}));

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
