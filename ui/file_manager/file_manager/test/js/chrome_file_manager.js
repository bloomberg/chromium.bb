// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test implementation of chrome.file[ManagerPrivate|System] apis.
// These APIs are provided natively to a chrome app, but since we are
// running as a regular web page, we must provide test implementations.

mockVolumeManager = new MockVolumeManager();

// Create drive /root/ immediately.
mockVolumeManager
    .getCurrentProfileVolumeInfo(VolumeManagerCommon.VolumeType.DRIVE)
    .fileSystem.populate(['/root/']);

chrome.fileManagerPrivate = {
  currentId_: 'test@example.com',
  dispatchEvent_: function(listenerType, event) {
    setTimeout(() => {
      this[listenerType].listeners_.forEach(l => l.call(null, event));
    }, 0);
  },
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
  listeners_: {
    'onDirectoryChanged': [],
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
    // Returns success.
    setTimeout(callback, 0, true);
  },
  enableExternalFileScheme: () => {},
  executeTask: (taskId, entries, callback) => {
    // Returns opened|message_sent|failed|empty.
    setTimeout(callback, 0, 'failed');
  },
  getDriveConnectionState: (callback) => {
    setTimeout(callback, 0, mockVolumeManager.getDriveConnectionState());
  },
  getEntryProperties: (entries, names, callback) => {
    // Returns EntryProperties[].
    var results = [];
    entries.forEach(entry => {
      var props = {};
      names.forEach(name => {
        props[name] = entry.metadata[name];
      });
      results.push(props);
    });
    setTimeout(callback, 0, results);
  },
  getFileTasks: (entries, callback) => {
    // Returns FileTask[].
    setTimeout(callback, 0, []);
  },
  getPreferences: (callback) => {
    setTimeout(callback, 0, chrome.fileManagerPrivate.preferences_);
  },
  getProfiles: (callback) => {
    // Returns profiles, currentId, displayedId
    setTimeout(
        callback, 0, chrome.fileManagerPrivate.profiles_,
        chrome.fileManagerPrivate.currentId_,
        chrome.fileManagerPrivate.displayedId_);
  },
  getProviders: (callback) => {
    // Returns Provider[].
    setTimeout(callback, 0, []);
  },
  getRecentFiles: (restriction, callback) => {
    // Returns Entry[].
    setTimeout(callback, 0, []);
  },
  getSizeStats: (volumeId, callback) => {
    // MountPointSizeStats { totalSize: double,  remainingSize: double }
    setTimeout(callback, 0, {totalSize: 16e9, remainingSize: 8e9});
  },
  getStrings: (callback) => {
    // Returns map of strings.
    setTimeout(callback, 0, loadTimeData.data_);
  },
  getVolumeMetadataList: (callback) => {
    var list = [];
    for (var i = 0; i < mockVolumeManager.volumeInfoList.length; i++) {
      list.push(mockVolumeManager.volumeInfoList.item(i));
    }
    setTimeout(callback, 0, list);
  },
  grantAccess: (entryUrls, callback) => {
    setTimeout(callback, 0);
  },
  isUMAEnabled: (callback) => {
    setTimeout(callback, 0, false);
  },
  onAppsUpdated: {
    addListener: () => {},
  },
  onDeviceChanged: {
    addListener: () => {},
  },
  onDirectoryChanged: {
    listeners_: [],
    addListener: function(l) {
      this.listeners_.push(l);
    },
  },
  onDriveConnectionStatusChanged: {
    addListener: () => {},
  },
  onDriveSyncError: {
    addListener: () => {},
  },
  onFileTransfersUpdated: {
    addListener: () => {},
  },
  onMountCompleted: {
    addListener: () => {},
  },
  onPreferencesChanged: {
    addListener: () => {},
  },
  openInspector: (type) => {},
  removeFileWatch: (entry, callback) => {
    setTimeout(callback, 0, true);
  },
  requestWebStoreAccessToken: (callback) => {
    setTimeout(callback, 0, chrome.fileManagerPrivate.token_);
  },
  resolveIsolatedEntries: (entries, callback) => {
    setTimeout(callback, 0, entries);
  },
  searchDriveMetadata: (searchParams, callback) => {
    // Returns SearchResult[].
    // SearchResult { entry: Entry, highlightedBaseName: string }
    setTimeout(callback, 0, []);
  },
  validatePathNameLength: (parentEntry, name, callback) => {
    setTimeout(callback, 0, true);
  },
};

chrome.mediaGalleries = {
  getMetadata: (mediaFile, options, callback) => {
    // Returns metdata {mimeType: ..., ...}.
    setTimeout(() => {
      webkitResolveLocalFileSystemURL(mediaFile.name, entry => {
        callback({mimeType: entry.metadata.contentMimeType});
      }, 0);
    });
  },
};

chrome.fileSystem = {
  requestFileSystem: (options, callback) => {
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
  var match = url.match(/^filesystem:(\w+)(\/.*)/);
  if (match) {
    var volumeType = match[1];
    var path = match[2];
    var volume = mockVolumeManager.getCurrentProfileVolumeInfo(volumeType);
    if (volume) {
      // Decode URI in file paths.
      path = path.split('/').map(decodeURIComponent).join('/');
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
