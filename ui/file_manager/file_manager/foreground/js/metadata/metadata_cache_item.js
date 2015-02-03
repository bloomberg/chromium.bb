// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Cache of metadata for a FileEntry.
 * @constructor
 * @struct
 */
function MetadataCacheItem() {
  /**
   * Map of property name and MetadataCacheItemProperty.
   * @private {!Object<string, !MetadataCacheItemProperty>}
   * @const
   */
  this.properties_ = {};
}

/**
 * Marks invalidated properies as loading and returns property names that are
 * need to be loaded.
 * @param {number} requestId
 * @param {!Array<string>} names
 * @return {!Array<string>} Load requested names.
 */
MetadataCacheItem.prototype.startRequests = function(requestId, names) {
  var loadRequested = [];
  for (var i = 0; i < names.length; i++) {
    var name = names[i];
    // Check if the property needs to be updated.
    if (this.properties_[name] &&
        this.properties_[name].state !==
        MetadataCacheItemPropertyState.INVALIDATED) {
      continue;
    }
    if (!this.properties_[name])
      this.properties_[name] = new MetadataCacheItemProperty();
    this.properties_[name].requestId = requestId;
    this.properties_[name].state = MetadataCacheItemPropertyState.LOADING;
    loadRequested.push(name);
  }
  return loadRequested;
};

/**
 * Feeds the result of startRequests.
 * @param {number} requestId Request ID passed when calling startRequests.
 * @param {!Object} object Map of property name and value.
 * @return {boolean} Whether at least one property is updated or not.
 */
MetadataCacheItem.prototype.storeProperties = function(requestId, object) {
  var changed = false;
  for (var name in object) {
    if (!this.properties_[name])
      this.properties_[name] = new MetadataCacheItemProperty();
    if (requestId < this.properties_[name].requestId)
      continue;
    changed = true;
    this.properties_[name].requestId = requestId;
    this.properties_[name].value = object[name];
    this.properties_[name].state = MetadataCacheItemPropertyState.FULFILLED;
  }
  return changed;
};

/**
 * Marks the caches of all properties in the item as invalidates and forces to
 * reload at the next time of startRequests.
 * @param {number} requestId Request ID of the invalidation request. This must
 *     be larger than other requets ID passed to the item before.
 */
MetadataCacheItem.prototype.invalidate = function(requestId) {
  for (var name in this.properties_) {
    assert(this.properties_[name].requestId < requestId);
    this.properties_[name].requestId = requestId;
    this.properties_[name].state = MetadataCacheItemPropertyState.INVALIDATED;
  }
};

/**
 * Obtains property for entries and names.
 * Note that it returns invalidated properties also.
 * @param {!Array<string>} names
 * @return {!Object}
 */
MetadataCacheItem.prototype.get = function(names) {
  var result = {};
  for (var i = 0; i < names.length; i++) {
    var name = names[i];
    if (this.properties_[name])
      result[name] = this.properties_[name].value;
  }
  return result;
};

/**
 * Creates deep copy of the item.
 * @return {!MetadataCacheItem}
 */
MetadataCacheItem.prototype.clone = function() {
  var clonedItem = new MetadataCacheItem();
  for (var name in this.properties_) {
    var property = this.properties_[name];
    clonedItem.properties_[name] = new MetadataCacheItemProperty();
    clonedItem.properties_[name].value = property.value;
    clonedItem.properties_[name].requestId = property.requestId;
    clonedItem.properties_[name].state = property.state;
  }
  return clonedItem;
};

/**
 * Returns whether all the given properties are fulfilled.
 * @param {!Array<string>} names Property names.
 * @return {boolean}
 */
MetadataCacheItem.prototype.hasFreshCache = function(names) {
  for (var i = 0; i < names.length; i++) {
    if (!(this.properties_[names[i]] &&
          this.properties_[names[i]].state ===
          MetadataCacheItemPropertyState.FULFILLED)) {
      return false;
    }
  }
  return true;
};

/**
 * @enum {string}
 */
var MetadataCacheItemPropertyState = {
  INVALIDATED: 'invalidated',
  LOADING: 'loading',
  FULFILLED: 'fulfilled'
};

/**
 * Cache of metadata for a property.
 * @constructor
 * @struct
 */
function MetadataCacheItemProperty() {
  /**
   * Cached value of property.
   * @public {*}
   */
  this.value = null;

  /**
   * Last request ID.
   * @public {number}
   */
  this.requestId = -1;

  /**
   * Cache state of the property.
   * @public {MetadataCacheItemPropertyState}
   */
  this.state = MetadataCacheItemPropertyState.INVALIDATED;
}
