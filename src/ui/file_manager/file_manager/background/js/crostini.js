// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Crostini shared path state handler.
 * @constructor
 */
function Crostini() {
  /** @private {boolean} */
  this.enabled_ = false;

  /**
   * Maintains a list of paths shared with the crostini container.
   * Keyed by VolumeManagerCommon.RootType, with boolean set values
   * of string paths.  e.g. {'Downloads': {'/foo': true, '/bar': true}}.
   * @private @dict {!Object<!Object<boolean>>}
   */
  this.shared_paths_ = {};
}

/**
 * Keep in sync with histograms.xml:FileBrowserCrostiniSharedPathsDepth
 * histogram_suffix.
 * @type {!Map<VolumeManagerCommon.RootType, string>}
 * @const
 */
Crostini.VALID_ROOT_TYPES_FOR_SHARE = new Map([
  [VolumeManagerCommon.RootType.DOWNLOADS, 'Downloads'],
  [VolumeManagerCommon.RootType.REMOVABLE, 'Removable'],
]);

/**
 * Can be collapsed into VALID_ROOT_TYPES_FOR_SHARE once
 * DriveFS flag is removed.
 * Keep in sync with histograms.xml:FileBrowserCrostiniSharedPathsDepth
 * histogram_suffix.
 * @type {!Map<VolumeManagerCommon.RootType, string>}
 * @const
 */
Crostini.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE = new Map([
  [VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT, 'DriveComputers'],
  [VolumeManagerCommon.RootType.COMPUTER, 'DriveComputers'],
  [VolumeManagerCommon.RootType.DRIVE, 'MyDrive'],
  [VolumeManagerCommon.RootType.TEAM_DRIVES_GRAND_ROOT, 'TeamDrive'],
  [VolumeManagerCommon.RootType.TEAM_DRIVE, 'TeamDrive'],
]);

/** @private {string} */
Crostini.UMA_ROOT_TYPE_OTHER = 'Other';

/**
 * Initialize Volume Manager.
 * @param {!VolumeManager} volumeManager
 */
Crostini.prototype.init = function(volumeManager) {
  this.volumeManager_ = volumeManager;
};

/**
 * Register for any shared path changes.
 */
Crostini.prototype.listen = function() {
  chrome.fileManagerPrivate.onCrostiniSharedPathsChanged.addListener(
      this.onChange_.bind(this));
};

/**
 * Set from feature 'crostini-files'.
 * @param {boolean} enabled
 */
Crostini.prototype.setEnabled = function(enabled) {
  this.enabled_ = enabled;
};

/**
 * @return {boolean} Whether crostini is enabled.
 */
Crostini.prototype.isEnabled = function() {
  return this.enabled_;
};

/**
 * Registers an entry as a shared path.
 * @param {!Entry} entry
 */
Crostini.prototype.registerSharedPath = function(entry) {
  const info = this.volumeManager_.getLocationInfo(entry);
  if (!info)
    return;
  let paths = this.shared_paths_[info.rootType];
  if (!paths) {
    paths = {};
    this.shared_paths_[info.rootType] = paths;
  }
  // Remove any existing paths that are children of the new path.
  for (let path in paths) {
    if (path.startsWith(entry.fullPath))
      delete paths[path];
  }
  paths[entry.fullPath] = true;

  // Record UMA.
  let suffix = Crostini.VALID_ROOT_TYPES_FOR_SHARE.get(info.rootType) ||
      Crostini.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE.get(info.rootType) ||
      Crostini.UMA_ROOT_TYPE_OTHER;
  metrics.recordSmallCount(
      'CrostiniSharedPaths.Depth.' + suffix,
      entry.fullPath.split('/').length - 1);
};

/**
 * Unregisters entry as a shared path.
 * @param {!Entry} entry
 */
Crostini.prototype.unregisterSharedPath = function(entry) {
  const info = this.volumeManager_.getLocationInfo(entry);
  if (!info)
    return;
  const paths = this.shared_paths_[info.rootType];
  if (paths) {
    delete paths[entry.fullPath];
  }
};

/**
 * Handles shared path changes.
 * @param {chrome.fileManagerPrivate.CrostiniSharedPathsChangedEvent} event
 * @private
 */
Crostini.prototype.onChange_ = function(event) {
  if (event.eventType === 'share') {
    for (const entry of event.entries) {
      this.registerSharedPath(entry);
    }
  } else if (event.eventType === 'unshare') {
    for (const entry of event.entries) {
      this.unregisterSharedPath(entry);
    }
  }
};

/**
 * Returns true if entry is shared.
 * @param {!Entry} entry
 * @return {boolean} True if path is shared either by a direct
 *   share or from one of its ancestor directories.
 */
Crostini.prototype.isPathShared = function(entry) {
  const root = this.volumeManager_.getLocationInfo(entry).rootType;
  const paths = this.shared_paths_[root];
  if (!paths)
    return false;
  // Check path and all ancestor directories.
  let path = entry.fullPath;
  while (path.length > 1) {
    if (paths[path])
      return true;
    path = path.substring(0, path.lastIndexOf('/'));
  }
  return !!paths['/'];
};

/**
 * Returns true if entry can be shared with Crostini.
 * @param {!Entry} entry
 * @param {boolean} persist If path is to be persisted.
 */
Crostini.prototype.canSharePath = function(entry, persist) {
  if (!this.enabled_)
    return false;

  // Only directories for persistent shares.
  if (persist && !entry.isDirectory)
    return false;

  // Allow Downloads, and Drive if DriveFS is enabled.
  const rootType = this.volumeManager_.getLocationInfo(entry).rootType;
  return Crostini.VALID_ROOT_TYPES_FOR_SHARE.has(rootType) ||
      (loadTimeData.getBoolean('DRIVE_FS_ENABLED') &&
       Crostini.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE.has(rootType));
};
