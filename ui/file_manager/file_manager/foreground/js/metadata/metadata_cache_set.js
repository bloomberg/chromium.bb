// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Set of MetadataCacheItem.
 * @param {!MetadataCacheSetStorage} items Storage object containing
 *     MetadataCacheItem.
 * @constructor
 * @struct
 */
function MetadataCacheSet(items) {
  /**
   * @private {!MetadataCacheSetStorage}
   * @const
   */
  this.items_ = items;
}

/**
 * Creates list of MetadataRequest based on the cache state.
 * @param {!Array<!FileEntry>} entries
 * @param {!Array<string>} names
 * @return {!Array<!MetadataRequest>}
 */
MetadataCacheSet.prototype.createRequests = function(entries, names) {
  var requests = [];
  for (var i = 0; i < entries.length; i++) {
    var item = this.items_.peek(entries[i].toURL());
    var requestedNames = item ? item.createRequests(names) : names;
    if (requestedNames.length)
      requests.push(new MetadataRequest(entries[i], requestedNames));
  }
  return requests;
};

/**
 * Updates cache states to start the given requests.
 * @param {number} requestId
 * @param {!Array<!MetadataRequest>} requests
 */
MetadataCacheSet.prototype.startRequests = function(requestId, requests) {
  for (var i = 0; i < requests.length; i++) {
    var request = requests[i];
    var url = request.entry.toURL();
    var item = this.items_.peek(url);
    if (!item) {
      item = new MetadataCacheItem();
      this.items_.put(url, item);
    }
    item.startRequests(requestId, request.names);
  }
};

/**
 * Stores results from NewMetadataProvider with the request Id.
 * @param {number} requestId Request ID. If a newer operation has already been
 *     done, the results must be ingored.
 * @param {!Array<!FileEntry>} entries
 * @param {!Array<!Object>} results
 * @return {boolean} Whether at least one result is stored or not.
 */
MetadataCacheSet.prototype.storeProperties = function(
    requestId, entries, results) {
  var changed = false;
  for (var i = 0; i < entries.length; i++) {
    var url = entries[i].toURL();
    var item = this.items_.peek(url);
    if (item && item.storeProperties(requestId, results[i]))
      changed = true;
  }
  return changed;
};

/**
 * Obtains cached properties for entries and names.
 * Note that it returns invalidated properties also.
 * @param {!Array<!FileEntry>} entries Entries.
 * @param {!Array<string>} names Property names.
 */
MetadataCacheSet.prototype.get = function(entries, names) {
  var results = [];
  for (var i = 0; i < entries.length; i++) {
    var item = this.items_.get(entries[i].toURL());
    results.push(item ? item.get(names) : {});
  }
  return results;
};

/**
 * Marks the caches of entries as invalidates and forces to reload at the next
 * time of startRequests.
 * @param {number} requestId Request ID of the invalidation request. This must
 *     be larger than other requets ID passed to the set before.
 * @param {!Array<!FileEntry>} entries
 */
MetadataCacheSet.prototype.invalidate = function(requestId, entries) {
  for (var i = 0; i < entries.length; i++) {
    var item = this.items_.peek(entries[i].toURL());
    if (item)
      item.invalidate(requestId);
  }
};

/**
 * Creates snapshot of the cache for entries.
 * @param {!Array<!FileEntry>} entries
 */
MetadataCacheSet.prototype.createSnapshot = function(entries) {
  var items = {};
  for (var i = 0; i < entries.length; i++) {
    var url = entries[i].toURL();
    var item = this.items_.peek(url);
    if (item)
      items[url] = item.clone();
  }
  return new MetadataCacheSet(new MetadataCacheSetStorageForObject(items));
};

/**
 * Returns whether all the given properties are fulfilled.
 * @param {!Array<!FileEntry>} entries Entries.
 * @param {!Array<string>} names Property names.
 * @return {boolean}
 */
MetadataCacheSet.prototype.hasFreshCache = function(entries, names) {
  for (var i = 0; i < entries.length; i++) {
    var item = this.items_.peek(entries[i].toURL());
    if (!(item && item.hasFreshCache(names)))
      return false;
  }
  return true;
};

/**
 * Interface of raw strage for MetadataCacheItem.
 * TODO(hirono): Add implementation of the interface for LRUCache.
 * @interface
 */
function MetadataCacheSetStorage() {
}

/**
 * Returns an item corresponding to the given URL.
 * @param {string} url Entry URL.
 * @return {MetadataCacheItem}
 */
MetadataCacheSetStorage.prototype.get = function(url) {};

/**
 * Returns an item corresponding to the given URL without changing orders in
 * the cache list.
 * @param {string} url Entry URL.
 * @return {MetadataCacheItem}
 */
MetadataCacheSetStorage.prototype.peek = function(url) {};

/**
 * Saves an item corresponding to the given URL.
 * @param {string} url Entry URL.
 * @param {!MetadataCacheItem} item Item to be saved.
 */
MetadataCacheSetStorage.prototype.put = function(url, item) {};

/**
 * Implementation of MetadataCacheSetStorage by using raw object.
 * @param {Object} items Map of URL and MetadataCacheItem.
 * @constructor
 * @implements {MetadataCacheSetStorage}
 * @struct
 */
function MetadataCacheSetStorageForObject(items) {
  this.items_ = items;
}

MetadataCacheSetStorageForObject.prototype.get = function(url) {
  return this.items_[url];
};

MetadataCacheSetStorageForObject.prototype.peek = function(url) {
  return this.items_[url];
};

MetadataCacheSetStorageForObject.prototype.put = function(url, item) {
  this.items_[url] = item;
};

/**
 * @param {!FileEntry} entry Entry
 * @param {!Array<string>} names Property name list to be requested.
 * @constructor
 * @struct
 */
function MetadataRequest(entry, names) {
  /**
   * @public {!FileEntry}
   * @const
   */
  this.entry = entry;

  /**
   * @public {!Array<string>}
   * @const
   */
  this.names = names;
}
