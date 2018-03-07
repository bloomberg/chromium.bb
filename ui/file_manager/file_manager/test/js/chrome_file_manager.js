// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test implementation of chrome.file[ManagerPrivate|System] apis.
// These APIs are provided natively to a chrome app, but since we are
// running as a regular web page, we must provide test implementations.

mockVolumeManager = new MockVolumeManager();
mockVolumeManager
    .getCurrentProfileVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS)
    .fileSystem.populate(
        ['/New Folder/', '/a.txt', '/kittens.jpg', '/unknown.ext']);
mockVolumeManager
    .getCurrentProfileVolumeInfo(VolumeManagerCommon.VolumeType.DRIVE)
    .fileSystem.populate(
        ['/root/New Folder/', '/root/a.txt', '/root/kittens.jpg']);

chrome.fileManagerPrivate = {
  currentId_: 'test@example.com',
  displayedId_: 'test@example.com',
  preferences_: {
    allowRedeemOffers: true,
    cellularDisabled: true,
    driveEnabled: true,
    hostedFilesDisabled: true,
    searchSuggestEnabled: true,
    timezone: 'Australia/Sydney',
    use24hourClock: false,
  },
  profiles_: [{
    displayName: 'Test User',
    isCurrentProfile: true,
    profileId: 'test@example.com'
  }],
  token_: 'token',
  SourceRestriction: {
    ANY_SOURCE: 'any_source',
    NATIVE_OR_DRIVE_SOURCE: 'native_or_drive_source',
    NATIVE_SOURCE: 'native_source',
  },
  addFileWatch: (entry, callback) => {
    console.debug('c.fmp.addFileWatch called', entry);
    setTimeout(callback, 0, true);
  },
  enableExternalFileScheme: () => {
    console.debug('c.fmp.enableExternalFileScheme called');
  },
  executeTask: (taskId, entries, callback) => {
    console.debug('c.fmp.executeTask called', taskId, entries);
    // Returns opened|message_sent|failed|empty.
    setTimeout(callback, 0, 'failed');
  },
  getDriveConnectionState: (callback) => {
    console.debug('c.fmp.getDriveConnectionState called');
    setTimeout(callback, 0, mockVolumeManager.getDriveConnectionState());
  },
  getEntryProperties: (entries, names, callback) => {
    console.debug('c.fmp.getEntryProperties called', entries, names);
    // Returns EntryProperties[].
    var results = [];
    for (var i = 0; i < entries.length; i++) {
      results.push({});
    }
    setTimeout(callback, 0, results);
  },
  getFileTasks: (entries, callback) => {
    console.debug('c.fmp.getFileTasks called', entries);
    // Returns FileTask[].
    setTimeout(callback, 0, []);
  },
  getPreferences: (callback) => {
    console.debug('c.fmp.getPreferences called');
    setTimeout(callback, 0, chrome.fileManagerPrivate.preferences_);
  },
  getProfiles: (callback) => {
    console.debug('c.fmp.getProfiles called');
    setTimeout(
        callback, 0, chrome.fileManagerPrivate.profiles_,
        chrome.fileManagerPrivate.currentId_,
        chrome.fileManagerPrivate.displayedId_);
  },
  getProviders: (callback) => {
    console.debug('c.fmp.getProviders called');
    // Returns Provider[].
    setTimeout(callback, 0, []);
  },
  getRecentFiles: (restriction, callback) => {
    console.debug('c.fmp.getRecentFiles called', restriction);
    // Returns Entry[].
    setTimeout(callback, 0, []);
  },
  getSizeStats: (volumeId, callback) => {
    console.debug('c.fmp.getSizeStats called', volumeId);
    // MountPointSizeStats { totalSize: double,  remainingSize: double }
    setTimeout(callback, 0, {totalSize: 16e9, remainingSize: 8e9});
  },
  getStrings: (callback) => {
    console.debug('c.fmp.getStrings called');
    setTimeout(callback, 0, loadTimeData.data_);
  },
  getVolumeMetadataList: (callback) => {
    console.debug('c.fmp.getVolumeMetadatalist called');
    var list = [];
    for (var i = 0; i < mockVolumeManager.volumeInfoList.length; i++) {
      list.push(mockVolumeManager.volumeInfoList.item(i));
    }
    setTimeout(callback, 0, list);
  },
  grantAccess: (entryUrls, callback) => {
    console.debug('c.fmp.grantAccess called', entryUrls);
    setTimeout(callback, 0);
  },
  isUMAEnabled: (callback) => {
    console.debug('c.fmp.isUMAEnabled called');
    setTimeout(callback, 0, false);
  },
  onAppsUpdated: {
    addListener: () => {
      console.debug('c.fmp.onAppsUpdated.addListener called');
    },
  },
  onDeviceChanged: {
    addListener: () => {
      console.debug('c.fmp.onDeviceChanged.addListener called');
    },
  },
  onDirectoryChanged: {
    addListener: () => {
      console.debug('c.fmp.onDirectoryChanged.addListener called');
    },
  },
  onDriveConnectionStatusChanged: {
    addListener: () => {
      console.debug('c.fmp.onDriveConnectionStatusChanged.addListener called');
    },
  },
  onDriveSyncError: {
    addListener: () => {
      console.debug('c.fmp.onDriveSyncError.addListener called');
    },
  },
  onFileTransfersUpdated: {
    addListener: () => {
      console.debug('c.fmp.onFileTransfersUpdated.addListener called');
    },
  },
  onMountCompleted: {
    addListener: () => {
      console.debug('c.fmp.onMountCompleted.addListener called');
    },
  },
  onPreferencesChanged: {
    addListener: () => {
      console.debug('c.fmp.onPreferencesChanged.addListener called');
    },
  },
  removeFileWatch: (entry, callback) => {
    console.debug('c.fmp.removeFileWatch called', entry);
    setTimeout(callback, 0, true);
  },
  requestWebStoreAccessToken: (callback) => {
    console.debug('c.fmp.requestWebStoreAccessToken called');
    setTimeout(callback, 0, chrome.fileManagerPrivate.token_);
  },
  resolveIsolatedEntries: (entries, callback) => {
    console.debug('c.fmp.resolveIsolatedEntries called', entries);
    setTimeout(callback, 0, entries);
  },
  searchDriveMetadata: (searchParams, callback) => {
    console.debug('c.fmp.searchDriveMetadata called', searchParams);
    // Returns SearchResult[].
    // SearchResult { entry: Entry, highlightedBaseName: string }
    setTimeout(callback, 0, []);
  },
  validatePathNameLength: (parentEntry, name, callback) => {
    console.debug('c.fmp.validatePathNameLength called', parentEntry, name);
    setTimeout(callback, 0, true);
  },
};

chrome.fileSystem = {
  requestFileSystem: (options, callback) => {
    console.debug('chrome.fileSystem.requestFileSystem called', options);
    var volume =
        mockVolumeManager.volumeInfoList.findByVolumeId(options.volumeId);
    setTimeout(callback, 0, volume ? volume.fileSystem : null);
  },
};

/**
 * Override webkitResolveLocalFileSystemURL for testing.
 * @param {string} url URL to resolve.
 * @param {function(!MockEntry)} successCallback Success callback.
 * @param {function(!DOMException)} errorCallback Error callback.
 */
webkitResolveLocalFileSystemURL = (url, successCallback, errorCallback) => {
  console.debug('webkitResolveLocalFileSystemURL', url);
  var match = url.match(/^filesystem:(\w+)(\/.*)/);
  if (match) {
    var volumeType = match[1];
    var path = match[2];
    var volume = mockVolumeManager.getCurrentProfileVolumeInfo(volumeType);
    if (volume) {
      var entry = volume.fileSystem.entries[path];
      if (entry) {
        setTimeout(successCallback, 0, entry);
        return;
      }
    }
  }
  var error = new DOMException(
      'webkitResolveLocalFileSystemURL not found: [' + url + ']');
  if (errorCallback) {
    setTimeout(errorCallback, 0, error);
  } else {
    throw error;
  }
};
