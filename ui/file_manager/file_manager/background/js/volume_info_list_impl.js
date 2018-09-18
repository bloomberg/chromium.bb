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
VolumeInfoListImpl.prototype.findIndex = function(volumeId) {
  for (var i = 0; i < this.model_.length; i++) {
    if (this.model_.item(i).volumeId === volumeId)
      return i;
  }
  return -1;
};

/** @override */
VolumeInfoListImpl.prototype.findByEntry = function(entry) {
  for (var i = 0; i < this.length; i++) {
    var volumeInfo = this.item(i);
    if (volumeInfo.fileSystem &&
        util.isSameFileSystem(volumeInfo.fileSystem, entry.filesystem)) {
      return volumeInfo;
    }
    // Additionally, check fake entries.
    for (var key in volumeInfo.fakeEntries_) {
      var fakeEntry = volumeInfo.fakeEntries_[key];
      if (util.isSameEntry(fakeEntry, entry))
        return volumeInfo;
    }
  }
  return null;
};

/** @override */
VolumeInfoListImpl.prototype.findByDevicePath = function(devicePath) {
  for (var i = 0; i < this.length; i++) {
    var volumeInfo = this.item(i);
    if (volumeInfo.devicePath &&
        volumeInfo.devicePath == devicePath) {
      return volumeInfo;
    }
  }
  return null;
};

/**
 * Returns a VolumInfo for the volume ID, or null if not found.
 *
 * @param {string} volumeId
 * @return {VolumeInfo} The volume's information, or null if not found.
 */
VolumeInfoListImpl.prototype.findByVolumeId = function(volumeId) {
  var index = this.findIndex(volumeId);
  return (index !== -1) ?
      /** @type {VolumeInfo} */ (this.model_.item(index)) :
      null;
};

/** @override */
VolumeInfoListImpl.prototype.whenVolumeInfoReady = function(volumeId) {
  return new Promise(function(fulfill) {
    var handler = function() {
      var info = this.findByVolumeId(volumeId);
      if (info) {
        fulfill(info);
        this.model_.removeEventListener('splice', handler);
      }
    }.bind(this);
    this.model_.addEventListener('splice', handler);
    handler();
  }.bind(this));
};

/** @override */
VolumeInfoListImpl.prototype.item = function(index) {
  return /** @type {!VolumeInfo} */ (this.model_.item(index));
};
