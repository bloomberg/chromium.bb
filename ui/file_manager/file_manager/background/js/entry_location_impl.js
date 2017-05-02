// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Location information which shows where the path points in FileManager's
 * file system.
 *
 * @param {!VolumeInfo} volumeInfo Volume information.
 * @param {VolumeManagerCommon.RootType} rootType Root type.
 * @param {boolean} isRootEntry Whether the entry is root entry or not.
 * @param {boolean} isReadOnly Whether the entry is read only or not.
 * @constructor
 * @implements {EntryLocation}
 */
function EntryLocationImpl(volumeInfo, rootType, isRootEntry, isReadOnly) {
  /** @override */
  this.volumeInfo = volumeInfo;

  /** @override */
  this.rootType = rootType;

  /** @override */
  this.isRootEntry = isRootEntry;

  /** @override */
  this.isSpecialSearchRoot =
      this.rootType === VolumeManagerCommon.RootType.DRIVE_OFFLINE ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_RECENT;

  /** @override */
  this.isDriveBased = this.rootType === VolumeManagerCommon.RootType.DRIVE ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_OTHER ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_RECENT ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_OFFLINE ||
      this.rootType === VolumeManagerCommon.RootType.TEAM_DRIVES_GRAND_ROOT ||
      this.rootType === VolumeManagerCommon.RootType.TEAM_DRIVE;

  /** @override */
  this.isReadOnly = isReadOnly;

  /** @type{boolean} */
  this.hasFixedLabel =
      this.isRootEntry && rootType !== VolumeManagerCommon.RootType.TEAM_DRIVE;

  Object.freeze(this);
}
