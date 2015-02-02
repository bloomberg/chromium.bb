// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Set of MetadataCacheItem.
 * @param {!MetadataCacheSetStorage} items Storage object containing
 *     MetadataCacheItem.
 * @constructor
 * @const
 */
function MetadataCacheSet(items) {
  /**
   * @private {!MetadataCacheSetStorage}
   * @const
   */
  this.items_ = items;
}

/**
 * Starts requests for invalidated properties.
 * @param {number} requestId
 * @param {!Array<!FileEntry>} entries
 * @param {!Array<string>} names
 * @return {!Array<!MetadataRequest>} Requests to be passed NewMetadataProvider.
 */
MetadataCacheSet.prototype.startRequests = function(requestId, entries, names) {
  var requests = [];
  for (var i = 0; i < entries.length; i++) {
    var url = entries[i].toURL();
    var item = this.items_.get(url);
    if (!item) {
      item = new MetadataCacheItem();
      this.items_.put(url, item);
    }
    var loadRequested = item.startRequests(requestId, names);
    if (loadRequested.length)
      requests.push(new MetadataRequest(entries[i], loadRequested));
  }
  return requests;
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
 * Interface of raw strage for MetadataCacheItem.
 * TODO(hirono): Add implementation of the interface for LRUCache.
 * @interface
 */
function MetadataCacheSetStorage() {
}

/**
 * Returns an item corresponding to the given URL.
 * @param {string} url Entry URL.
 * @return {!MetadataCacheItem}
 */
MetadataCacheSetStorage.prototype.get = function(url) {};

/**
 * Returns an item corresponding to the given URL without changing orders in
 * the cache list.
 * @param {string} url Entry URL.
 * @return {!MetadataCacheItem}
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
