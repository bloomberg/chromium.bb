// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Represents each volume, such as "drive", "download directory", each "USB
 * flush storage", or "mounted zip archive" etc.
 *
 * @constructor
 * @implements {VolumeInfo}
 * @struct
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType The type of the volume.
 * @param {string} volumeId ID of the volume.
 * @param {FileSystem} fileSystem The file system object for this volume.
 * @param {(string|undefined)} error The error if an error is found.
 * @param {(string|undefined)} deviceType The type of device
 *     ('usb'|'sd'|'optical'|'mobile'|'unknown') (as defined in
 *     chromeos/disks/disk_mount_manager.cc). Can be undefined.
 * @param {(string|undefined)} devicePath Identifier of the device that the
 *     volume belongs to. Can be undefined.
 * @param {boolean} isReadOnly True if the volume is read only.
 * @param {boolean} isReadOnlyRemovableDevice True if the volume is read only
 *     removable device.
 * @param {!{displayName:string, isCurrentProfile:boolean}} profile Profile
 *     information.
 * @param {string} label Label of the volume.
 * @param {(string|undefined)} extensionId Id of the extension providing this
 *     volume. Empty for native volumes.
 * @param {boolean} hasMedia When true the volume has been identified
 *     as containing media such as photos or videos.
 * @param {boolean} configurable When true, then the volume can be configured.
 * @param {VolumeManagerCommon.Source} source Source of the volume's data.
 */
function VolumeInfoImpl(
    volumeType,
    volumeId,
    fileSystem,
    error,
    deviceType,
    devicePath,
    isReadOnly,
    isReadOnlyRemovableDevice,
    profile,
    label,
    extensionId,
    hasMedia,
    configurable,
    watchable,
    source) {
  this.volumeType_ = volumeType;
  this.volumeId_ = volumeId;
  this.fileSystem_ = fileSystem;
  this.label_ = label;
  this.displayRoot_ = null;

  /** @type {Object<!FakeEntry>} */
  this.fakeEntries_ = {};

  /** @type {Promise.<!DirectoryEntry>} */
  this.displayRootPromise_ = null;

  if (volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
    // TODO(mtomasz): Convert fake entries to DirectoryProvider.
    this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_OFFLINE] = {
      isDirectory: true,
      rootType: VolumeManagerCommon.RootType.DRIVE_OFFLINE,
      toURL: function() { return 'fake-entry://drive_offline'; }
    };
    this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME] = {
      isDirectory: true,
      rootType: VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
      toURL: function() { return 'fake-entry://drive_shared_with_me'; }
    };
    this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_RECENT] = {
      isDirectory: true,
      rootType: VolumeManagerCommon.RootType.DRIVE_RECENT,
      toURL: function() { return 'fake-entry://drive_recent'; }
    };
  }

  // Note: This represents if the mounting of the volume is successfully done
  // or not. (If error is empty string, the mount is successfully done).
  // TODO(hidehiko): Rename to make this more understandable.
  this.error_ = error;
  this.deviceType_ = deviceType;
  this.devicePath_ = devicePath;
  this.isReadOnly_ = isReadOnly;
  this.isReadOnlyRemovableDevice_ = isReadOnlyRemovableDevice;
  this.profile_ = Object.freeze(profile);
  this.extensionId_ = extensionId;
  this.hasMedia_ = hasMedia;
  this.configurable_ = configurable;
  this.watchable_ = watchable;
  this.source_ = source;
}

VolumeInfoImpl.prototype = /** @struct */ {
  /**
   * @return {VolumeManagerCommon.VolumeType} Volume type.
   */
  get volumeType() {
    return this.volumeType_;
  },
  /**
   * @return {string} Volume ID.
   */
  get volumeId() {
    return this.volumeId_;
  },
  /**
   * @return {FileSystem} File system object.
   */
  get fileSystem() {
    return this.fileSystem_;
  },
  /**
   * @return {DirectoryEntry} Display root path. It is null before finishing to
   * resolve the entry.
   */
  get displayRoot() {
    return this.displayRoot_;
  },
  /**
   * @return {Object<!FakeEntry>} Fake entries.
   */
  get fakeEntries() {
    return this.fakeEntries_;
  },
  /**
   * @return {(string|undefined)} Error identifier.
   */
  get error() {
    return this.error_;
  },
  /**
   * @return {(string|undefined)} Device type identifier.
   */
  get deviceType() {
    return this.deviceType_;
  },
  /**
   * @return {(string|undefined)} Device identifier.
   */
  get devicePath() {
    return this.devicePath_;
  },
  /**
   * @return {boolean} Whether read only or not.
   */
  get isReadOnly() {
    return this.isReadOnly_;
  },
  /**
   * @return {boolean} Whether the device is read-only removable device or not.
   */
  get isReadOnlyRemovableDevice() {
    return this.isReadOnlyRemovableDevice_;
  },
  /**
   * @return {!{displayName:string, isCurrentProfile:boolean}} Profile data.
   */
  get profile() {
    return this.profile_;
  },
  /**
   * @return {string} Label for the volume.
   */
  get label() {
    return this.label_;
  },
  /**
   * @return {(string|undefined)} Id of an extennsion providing this volume.
   */
  get extensionId() {
    return this.extensionId_;
  },
  /**
   * @return {boolean} True if the volume contains media.
   */
  get hasMedia() {
    return this.hasMedia_;
  },
  /**
   * @return {boolean} True if the volume is configurable.
   */
  get configurable() {
    return this.configurable_;
  },
  /**
   * @return {boolean} True if the volume is watchable.
   */
  get watchable() {
    return this.watchable_;
  },
  /**
   * @return {VolumeManagerCommon.Source} Source of the volume's data.
   */
  get source() {
    return this.source_;
  }
};

/**
 * @override
 */
VolumeInfoImpl.prototype.resolveDisplayRoot = function(opt_onSuccess,
                                                       opt_onFailure) {
  if (!this.displayRootPromise_) {
    // TODO(mtomasz): Do not add VolumeInfo which failed to resolve root, and
    // remove this if logic. Call opt_onSuccess() always, instead.
    if (this.volumeType !== VolumeManagerCommon.VolumeType.DRIVE) {
      if (this.fileSystem_)
        this.displayRootPromise_ = /** @type {Promise.<!DirectoryEntry>} */ (
            Promise.resolve(this.fileSystem_.root));
      else
        this.displayRootPromise_ = /** @type {Promise.<!DirectoryEntry>} */ (
            Promise.reject(this.error));
    } else {
      // For Drive, we need to resolve.
      var displayRootURL = this.fileSystem_.root.toURL() + '/root';
      this.displayRootPromise_ = new Promise(
          window.webkitResolveLocalFileSystemURL.bind(null, displayRootURL));
    }

    // Store the obtained displayRoot.
    this.displayRootPromise_.then(function(displayRoot) {
      this.displayRoot_ = displayRoot;
    }.bind(this));
  }
  if (opt_onSuccess)
    this.displayRootPromise_.then(opt_onSuccess, opt_onFailure);
  return this.displayRootPromise_;
};
