// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock class for VolumeManager.
 * @constructor
 */
function MockVolumeManager() {
  this.volumeInfoList = new VolumeInfoListImpl();
  this.driveConnectionState = {
    type: VolumeManagerCommon.DriveConnectionType.ONLINE
  };
  this.createVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE,
      'drive',
      str('DRIVE_DIRECTORY_LABEL'));
  this.createVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS,
      'downloads',
      str('DOWNLOADS_DIRECTORY_LABEL'));
}

/**
 * @private {?VolumeManager}
 */
MockVolumeManager.instance_ = null;

/**
 * Replaces the VolumeManager singleton with a MockVolumeManager.
 * @param {!MockVolumeManager=} opt_singleton
 */
MockVolumeManager.installMockSingleton = function(opt_singleton) {
  MockVolumeManager.instance_ = opt_singleton || new MockVolumeManager();

  volumeManagerFactory.getInstance = function() {
    return Promise.resolve(MockVolumeManager.instance_);
  };
};

/**
 * Creates, installs and returns a mock VolumeInfo instance.
 *
 * @param {!VolumeType} type
 * @param {string} volumeId
 * @param {string} label
 *
 * @return {!VolumeInfo}
 */
MockVolumeManager.prototype.createVolumeInfo =
    function(type, volumeId, label) {
  var volumeInfo =
      MockVolumeManager.createMockVolumeInfo(type, volumeId, label);
  this.volumeInfoList.add(volumeInfo);
  return volumeInfo;
};

/**
 * Returns the corresponding VolumeInfo.
 *
 * @param {MockFileEntry} entry MockFileEntry pointing anywhere on a volume.
 * @return {VolumeInfo} Corresponding VolumeInfo.
 */
MockVolumeManager.prototype.getVolumeInfo = function(entry) {
  return this.volumeInfoList.findByEntry(entry);
};

/**
 * Obtains location information from an entry.
 * Current implementation can handle only fake entries.
 *
 * @param {Entry} entry A fake entry.
 * @return {EntryLocation} Location information.
 */
MockVolumeManager.prototype.getLocationInfo = function(entry) {
  if (util.isFakeEntry(entry)) {
    return new EntryLocationImpl(this.volumeInfoList.item(0), entry.rootType,
        true, true);
  }

  if (entry.filesystem.name === VolumeManagerCommon.VolumeType.DRIVE) {
    var volumeInfo = this.volumeInfoList.item(0);
    var isRootEntry = entry.fullPath === '/root';
    return new EntryLocationImpl(volumeInfo, VolumeManagerCommon.RootType.DRIVE,
        isRootEntry, true);
  }

  throw new Error('Not implemented exception.');
};

/**
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {VolumeInfo} Volume info.
 */
MockVolumeManager.prototype.getCurrentProfileVolumeInfo = function(volumeType) {
  return VolumeManagerImpl.prototype.getCurrentProfileVolumeInfo.call(
      this, volumeType);
};

/**
 * @return {VolumeManagerCommon.DriveConnectionState} Current drive connection
 *     state.
 */
MockVolumeManager.prototype.getDriveConnectionState = function() {
  return this.driveConnectionState;
};

/**
 * Utility function to create a mock VolumeInfo.
 * @param {VolumeType} type Volume type.
 * @param {string} volumeId Volume id.
 * @param {string} label Label.
 * @return {VolumeInfo} Created mock VolumeInfo.
 */
MockVolumeManager.createMockVolumeInfo = function(type, volumeId, label) {
  var fileSystem = new MockFileSystem(volumeId, 'filesystem:' + volumeId);
  fileSystem.entries['/'] = new MockDirectoryEntry(fileSystem, '');

  var volumeInfo = new VolumeInfoImpl(
      type,
      volumeId,
      fileSystem,
      '',      // error
      '',      // deviceType
      '',      // devicePath
      false,   // isReadOnly
      false,   // isReadOnlyRemovableDevice
      {isCurrentProfile: true, displayName: ''},  // profile
      label,   // label
      '',      // extensionId
      false,   // hasMedia
      false);  // configurable

  return volumeInfo;
};

/**
 * Mock class for VolumeManagerWrapper.
 *
 * TODO(mtomasz): Merge mocks once VolumeManagerWrapper and VolumeManager
 * implement an identical interface.
 * @constructor
 */
function MockVolumeManagerWrapper() {
  this.volumeInfoList = new cr.ui.ArrayDataModel([]);
  this.driveConnectionState = {
    type: VolumeManagerCommon.DriveConnectionType.ONLINE
  };
  this.createVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE,
      'drive',
      str('DRIVE_DIRECTORY_LABEL'));
  this.createVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS,
      'downloads',
      str('DOWNLOADS_DIRECTORY_LABEL'));
}
/**
 * @private {?VolumeManager}
 */
MockVolumeManagerWrapper.instance_ = null;
/**
 * Replaces the VolumeManager singleton with a MockVolumeManagerWrapper.
 * @param {!MockVolumeManagerWrapper=} opt_singleton
 */
MockVolumeManagerWrapper.installMockSingleton = function(opt_singleton) {
  MockVolumeManagerWrapper.instance_ =
      opt_singleton || new MockVolumeManagerWrapper();
  volumeManagerFactory.getInstance = function() {
    return Promise.resolve(MockVolumeManagerWrapper.instance_);
  };
};
/**
 * Creates, installs and returns a mock VolumeInfo instance.
 *
 * @param {!VolumeType} type
 * @param {string} volumeId
 * @param {string} label
 *
 * @return {!VolumeInfo}
 */
MockVolumeManagerWrapper.prototype.createVolumeInfo =
    function(type, volumeId, label) {
  var volumeInfo =
      MockVolumeManagerWrapper.createMockVolumeInfo(type, volumeId, label);
  this.volumeInfoList.push(volumeInfo);
  return volumeInfo;
};
/**
 * Returns the corresponding VolumeInfo.
 *
 * @param {MockFileEntry} entry MockFileEntry pointing anywhere on a volume.
 * @return {VolumeInfo} Corresponding VolumeInfo.
 */
MockVolumeManagerWrapper.prototype.getVolumeInfo = function(entry) {
  for (var i = 0; i < this.volumeInfoList.length; i++) {
    if (this.volumeInfoList.item(i).volumeId === entry.filesystem.name)
      return this.volumeInfoList.item(i);
  }
  return null;
};
/**
 * Obtains location information from an entry.
 * Current implementation can handle only fake entries.
 *
 * @param {Entry} entry A fake entry.
 * @return {EntryLocation} Location information.
 */
MockVolumeManagerWrapper.prototype.getLocationInfo = function(entry) {
  if (util.isFakeEntry(entry)) {
    return new EntryLocationImpl(this.volumeInfoList.item(0), entry.rootType,
        true, true);
  }
  if (entry.filesystem.name === VolumeManagerCommon.VolumeType.DRIVE) {
    var volumeInfo = this.volumeInfoList.item(0);
    var isRootEntry = entry.fullPath === '/root';
    return new EntryLocationImpl(volumeInfo, VolumeManagerCommon.RootType.DRIVE,
        isRootEntry, true);
  }
  throw new Error('Not implemented exception.');
};
/**
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {VolumeInfo} Volume info.
 */
MockVolumeManagerWrapper.prototype.getCurrentProfileVolumeInfo =
    function(volumeType) {
  return VolumeManager.prototype.getCurrentProfileVolumeInfo.call(
      this, volumeType);
};

/**
 * @return {VolumeManagerCommon.DriveConnectionState} Current drive connection
 *     state.
 */
MockVolumeManagerWrapper.getDriveConnectionState = function() {
  return this.driveConnectionState;
};

/**
 * Utility function to create a mock VolumeInfo.
 * @param {VolumeType} type Volume type.
 * @param {string} volumeId Volume id.
 * @param {string} label Label.
 * @return {VolumeInfo} Created mock VolumeInfo.
 */
MockVolumeManagerWrapper.createMockVolumeInfo =
    function(type, volumeId, label) {
  var fileSystem = new MockFileSystem(volumeId, 'filesystem:' + volumeId);
  fileSystem.entries['/'] = new MockDirectoryEntry(fileSystem, '');
  var volumeInfo = new VolumeInfoImpl(
      type,
      volumeId,
      fileSystem,
      '',     // error
      '',     // deviceType
      '',     // devicePath
      false,  // isReadonly
      false,  // isReadOnlyRemovableDevice
      {isCurrentProfile: true, displayName: ''},  // profile
      label,  // label
      '',     // extensionId
      false); // hasMedia
  return volumeInfo;
};
