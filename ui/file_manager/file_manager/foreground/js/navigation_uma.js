// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * UMA exporter for navigation in the Files app.
 *
 * @param {!VolumeManagerWrapper} volumeManager
 *
 * @constructor
 */
function NavigationUma(volumeManager) {
  /**
   * @type {!VolumeManagerWrapper}
   * @private
   */
  this.volumeManager_ = volumeManager;
}

/**
 * Keep the order of this in sync with FileManagerRootType in
 * tools/metrics/histograms/histograms.xml.
 *
 * @type {!Array<VolumeManagerCommon.RootType>}
 * @const
 */
NavigationUma.RootType = [
  VolumeManagerCommon.RootType.DOWNLOADS,
  VolumeManagerCommon.RootType.ARCHIVE,
  VolumeManagerCommon.RootType.REMOVABLE,
  VolumeManagerCommon.RootType.DRIVE,
  VolumeManagerCommon.RootType.TEAM_DRIVES_GRAND_ROOT,
  VolumeManagerCommon.RootType.TEAM_DRIVE,
  VolumeManagerCommon.RootType.MTP,
  VolumeManagerCommon.RootType.PROVIDED,
  VolumeManagerCommon.RootType.DRIVE_OTHER,
  VolumeManagerCommon.RootType.DRIVE_OFFLINE,
  VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
  VolumeManagerCommon.RootType.DRIVE_RECENT,
  VolumeManagerCommon.RootType.MEDIA_VIEW,
];

/**
 * Exports file type metric with the given |name|.
 *
 * @param {!FileEntry} entry
 * @param {string} name The histogram name.
 *
 * @private
 */
NavigationUma.prototype.exportRootType_ = function(entry, name) {
  var locationInfo = this.volumeManager_.getLocationInfo(entry);
  if (locationInfo)
    metrics.recordEnum(name, locationInfo.rootType, NavigationUma.RootType);
};

/**
 * Exports UMA based on the entry that has became new current directory.
 *
 * @param {!FileEntry} entry the new directory
 */
NavigationUma.prototype.onDirectoryChanged = function(entry) {
  this.exportRootType_(entry, 'FileBrowser.ChangeDirectory.RootType');
};
