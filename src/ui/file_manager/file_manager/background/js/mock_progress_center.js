// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Mock implementation of {ProgressCenter} for tests.
 *
 * @constructor
 * @struct
 * @implements {ProgressCenter}
 */
function MockProgressCenter() {
  /**
   * Items stored in the progress center.
   * @type {Object<ProgressCenterItem>}
   */
  this.items = {};
}

/**
 * Stores an item to the progress center.
 * @param {ProgressCenterItem} item Progress center item to be stored.
 */
MockProgressCenter.prototype.updateItem = function(item) {
  this.items[item.id] = item;
};

/**
 * Obtains an item stored in the progress center.
 * @param {string} id ID spcifying the progress item.
 */
MockProgressCenter.prototype.getItemById = function(id) {
  return this.items[id];
};

MockProgressCenter.prototype.requestCancel = function() {};

MockProgressCenter.prototype.addPanel = function() {};

MockProgressCenter.prototype.removePanel = function() {};

/**
 * Returns the number of unique keys in |this.items|.
 * @return {number}
 */
MockProgressCenter.prototype.getItemCount = function() {
  const array = Object.keys(
      /** @type {!Object} */ (this.items));
  return array.length;
};
