// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The container of the VolumeInfo for each mounted volume.
 * @constructor
 * @implements {VolumeInfoList}
 */
function VolumeInfoListImpl() {
  /**
   * Holds VolumeInfo instances.
   * @type {cr.ui.ArrayDataModel}
   * @private
   */
  this.model_ = new cr.ui.ArrayDataModel([]);
  Object.freeze(this);
}

VolumeInfoListImpl.prototype = {
  get length() { return this.model_.length; }
};

/** @override */
VolumeInfoListImpl.prototype.addEventListener = function(type, handler) {
  this.model_.addEventListener(type, handler);
};

/** @override */
VolumeInfoListImpl.prototype.removeEventListener = function(type, handler) {
  this.model_.removeEventListener(type, handler);
};

/** @override */
VolumeInfoListImpl.prototype.add = function(volumeInfo) {
  var index = this.findIndex(volumeInfo.volumeId);
  if (index !== -1)
    this.model_.splice(index, 1, volumeInfo);
  else
    this.model_.push(volumeInfo);
};

/** @override */
VolumeInfoListImpl.prototype.remove = function(volumeId) {
  var index = this.findIndex(volumeId);
  if (index !== -1)
    this.model_.splice(index, 1);
};

/** @override */
VolumeInfoListImpl.prototype.item = function(index) {
  return /** @type {!VolumeInfo} */ (this.model_.item(index));
};

/**
 * Obtains an index from the volume ID.
 * @param {string} volumeId Volume ID.
 * @return {number} Index of the volume.
 */
VolumeInfoListImpl.prototype.findIndex = function(volumeId) {
  for (var i = 0; i < this.model_.length; i++) {
    if (this.model_.item(i).volumeId === volumeId)
      return i;
  }
  return -1;
};
