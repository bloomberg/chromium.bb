// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Implementation of Crostini shared path state handler.
 *
 * @constructor
 * @implements {Crostini}
 */
function CrostiniImpl() {
  /**
   * True if crostini is enabled.
   * @private {boolean}
   */
  this.enabled_ = false;

  /**
   * Maintains a list of paths shared with the crostini container.
   * Keyed by entry.toURL().
   * @private @dict {!Object<boolean>}
   */
  this.shared_paths_ = {};
}

/**
 * Keep in sync with histograms.xml:FileBrowserCrostiniSharedPathsDepth
 * histogram_suffix.
 * @type {!Map<VolumeManagerCommon.RootType, string>}
 * @const
 */
CrostiniImpl.VALID_ROOT_TYPES_FOR_SHARE = new Map([
  [VolumeManagerCommon.RootType.DOWNLOADS, 'Downloads'],
  [VolumeManagerCommon.RootType.REMOVABLE, 'Removable'],
  [VolumeManagerCommon.RootType.ANDROID_FILES, 'AndroidFiles'],
]);

/**
 * Can be collapsed into VALID_ROOT_TYPES_FOR_SHARE once
 * DriveFS flag is removed.
 * Keep in sync with histograms.xml:FileBrowserCrostiniSharedPathsDepth
 * histogram_suffix.
 * @type {!Map<VolumeManagerCommon.RootType, string>}
 * @const
 */
CrostiniImpl.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE = new Map([
  [VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT, 'DriveComputers'],
  [VolumeManagerCommon.RootType.COMPUTER, 'DriveComputers'],
  [VolumeManagerCommon.RootType.DRIVE, 'MyDrive'],
  [VolumeManagerCommon.RootType.TEAM_DRIVES_GRAND_ROOT, 'TeamDrive'],
  [VolumeManagerCommon.RootType.TEAM_DRIVE, 'TeamDrive'],
]);

/**
 * @private {string}
 * @const
 */
CrostiniImpl.UMA_ROOT_TYPE_OTHER = 'Other';

/**
 * Initialize Volume Manager.
 * @param {!VolumeManager} volumeManager
 */
CrostiniImpl.prototype.init = function(volumeManager) {
  this.volumeManager_ = volumeManager;
};

/**
 * Register for any shared path changes.
 */
CrostiniImpl.prototype.listen = function() {
  chrome.fileManagerPrivate.onCrostiniChanged.addListener(
      this.onChange_.bind(this));
};

/**
 * Set from feature 'crostini-files'.
 * @param {boolean} enabled
 */
CrostiniImpl.prototype.setEnabled = function(enabled) {
  this.enabled_ = enabled;
};

/**
 * Returns true if crostini is enabled.
 * @return {boolean}
 */
CrostiniImpl.prototype.isEnabled = function() {
  return this.enabled_;
};

/**
 * @param {!Entry} entry
 * @return {VolumeManagerCommon.RootType}
 * @private
 */
CrostiniImpl.prototype.getRoot_ = function(entry) {
  const info =
      this.volumeManager_ && this.volumeManager_.getLocationInfo(entry);
  return info && info.rootType;
};

/**
 * Registers an entry as a shared path.
 * @param {!Entry} entry
 */
CrostiniImpl.prototype.registerSharedPath = function(entry) {
  const url = entry.toURL();
  // Remove any existing paths that are children of the new path.
  for (let path in this.shared_paths_) {
    if (path.startsWith(url)) {
      delete this.shared_paths_[path];
    }
  }
  this.shared_paths_[url] = true;

  // Record UMA.
  const root = this.getRoot_(entry);
  let suffix = CrostiniImpl.VALID_ROOT_TYPES_FOR_SHARE.get(root) ||
      CrostiniImpl.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE.get(root) ||
      CrostiniImpl.UMA_ROOT_TYPE_OTHER;
  metrics.recordSmallCount(
      'CrostiniSharedPaths.Depth.' + suffix,
      entry.fullPath.split('/').length - 1);
};

/**
 * Unregisters entry as a shared path.
 * @param {!Entry} entry
 */
CrostiniImpl.prototype.unregisterSharedPath = function(entry) {
  delete this.shared_paths_[entry.toURL()];
};

/**
 * Handles shared path changes.
 * @param {chrome.fileManagerPrivate.CrostiniEvent} event
 * @private
 */
CrostiniImpl.prototype.onChange_ = function(event) {
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
CrostiniImpl.prototype.isPathShared = function(entry) {
  // Check path and all ancestor directories.
  let path = entry.toURL();
  let root = path;
  if (entry && entry.filesystem && entry.filesystem.root) {
    root = entry.filesystem.root.toURL();
  }

  while (path.length > root.length) {
    if (this.shared_paths_[path]) {
      return true;
    }
    path = path.substring(0, path.lastIndexOf('/'));
  }
  return !!this.shared_paths_[root];
};

/**
 * Returns true if entry can be shared with Crostini.
 * @param {!Entry} entry
 * @param {boolean} persist If path is to be persisted.
 */
CrostiniImpl.prototype.canSharePath = function(entry, persist) {
  if (!this.enabled_) {
    return false;
  }

  // Only directories for persistent shares.
  if (persist && !entry.isDirectory) {
    return false;
  }

  const root = this.getRoot_(entry);

  // TODO(crbug.com/917920): Remove when DriveFS enforces allowed write paths.
  // Disallow Computers Grand Root, and Computer Root.
  if (root === VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
      (root === VolumeManagerCommon.RootType.COMPUTER &&
       entry.fullPath.split('/').length <= 3)) {
    return false;
  }

  return CrostiniImpl.VALID_ROOT_TYPES_FOR_SHARE.has(root) ||
      (loadTimeData.getBoolean('DRIVE_FS_ENABLED') &&
       CrostiniImpl.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE.has(root));
};
