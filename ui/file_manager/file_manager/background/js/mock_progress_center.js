// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Mock of Progress Center.
 * @constructor
 */
function MockProgressCenter() {
  /**
   * Items stored in the progress center.
   * @type {Object.<string, ProgressCenterItem>}
   */
  this.items = {};

  Object.seal(this);
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
