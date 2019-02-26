// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let mockChrome;

function setUp() {
  window.loadTimeData.getString = id => id;
  window.loadTimeData.data = {};

  // Set up mock of chrome.fileManagerPrivate APIs.
  mockChrome = {
    runtime: {lastError: undefined},
    fileSystem: {
      requestFileSystem: function(options, callback) {
        if (!(options.volumeId in chrome.fileManagerPrivate.fileSystemMap_)) {
          chrome.runtime.lastError = {message: 'Not found.'};
        }
        callback(chrome.fileManagerPrivate.fileSystemMap_[options.volumeId]);
      },
    },
    fileManagerPrivate: {
      mountSourcePath_: null,
      onMountCompletedListeners_: [],
      onDriveConnectionStatusChangedListeners_: [],
      driveConnectionState_: VolumeManagerCommon.DriveConnectionType.ONLINE,
      volumeMetadataList_: [],
      addMount: function(fileUrl, callback) {
        callback(mockChrome.fileManagerPrivate.mountSourcePath_);
      },
      removeMount: function(volumeId) {
        var event = {
          eventType: 'unmount',
          status: 'success',
          volumeMetadata: {
            volumeId: volumeId
          }
        };
        mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent(event);
      },
      onDriveConnectionStatusChanged: {
        addListener: function(listener) {
          mockChrome.fileManagerPrivate.onDriveConnectionStatusChangedListeners_
              .push(listener);
        },
        dispatchEvent: function(event) {
          mockChrome.fileManagerPrivate.onDriveConnectionStatusChangedListeners_
              .forEach(function(listener) {
                listener(event);
              });
        }
      },
      onMountCompleted: {
        addListener: function(listener) {
          mockChrome.fileManagerPrivate.onMountCompletedListeners_.push(
              listener);
        },
        dispatchEvent: function(event) {
          mockChrome.fileManagerPrivate.onMountCompletedListeners_.forEach(
              function(listener) {
                listener(event);
              });
        }
      },
      getDriveConnectionState: function(callback) {
        callback(mockChrome.fileManagerPrivate.driveConnectionState_);
      },
      getVolumeMetadataList: function(callback) {
        callback(mockChrome.fileManagerPrivate.volumeMetadataList_);
      },
      resolveIsolatedEntries: function(entries, callback) {
        console.log('*** RESOLVE ISOLATED');
        callback(entries);
      },
      set driveConnectionState(state) {
        mockChrome.fileManagerPrivate.driveConnectionState_ = state;
        mockChrome.fileManagerPrivate.onDriveConnectionStatusChanged
            .dispatchEvent(null);
      },
    }
  };
  installMockChrome(mockChrome);
  new MockCommandLinePrivate();
  chrome.fileManagerPrivate.volumeMetadataList_ = [
    {
      volumeId: 'download:Downloads',
      volumeLabel: '',
      volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
      isReadOnly: false,
      profile: getMockProfile(),
      configurable: false,
      watchable: true,
      source: VolumeManagerCommon.Source.SYSTEM
    },
    {
      volumeId: 'drive:drive-foobar%40chromium.org-hash',
      volumeLabel: '',
      volumeType: VolumeManagerCommon.VolumeType.DRIVE,
      isReadOnly: false,
      profile: getMockProfile(),
      configurable: false,
      watchable: true,
      source: VolumeManagerCommon.Source.NETWORK
    },
    {
      volumeId: 'android_files:0',
      volumeLabel: '',
      volumeType: VolumeManagerCommon.VolumeType.ANDROID_FILES,
      isReadOnly: false,
      provile: getMockProfile(),
      configurable: false,
      watchable: true,
      source: VolumeManagerCommon.Source.SYSTEM
    }
  ];
  chrome.fileManagerPrivate.fileSystemMap_ = {
    'download:Downloads': new MockFileSystem('download:Downloads'),
    'drive:drive-foobar%40chromium.org-hash':
        new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
    'android_files:0': new MockFileSystem('android_files:0')
  };
}

function tearDown() {
  volumeManagerFactory.revokeInstanceForTesting();
  // To avoid a closure warning assigning to |chrome|, tearDown() does not
  // balance the call to installMockChrome() here.
}

/**
 * Returns a mock profile.
 *
 * @return {{displayName:string, isCurrentProfile:boolean, profileId:string}}
 *     Mock profile
 */
function getMockProfile() {
  return {
    displayName: 'foobar@chromium.org',
    isCurrentProfile: true,
    profileId: ''
  };
}

function testGetVolumeInfo(callback) {
  reportPromise(
      volumeManagerFactory.getInstance().then(function(volumeManager) {
        var entry = new MockFileEntry(new MockFileSystem('download:Downloads'),
            '/foo/bar/bla.zip');

        var volumeInfo = volumeManager.getVolumeInfo(entry);
        assertEquals('download:Downloads', volumeInfo.volumeId);
        assertEquals(VolumeManagerCommon.VolumeType.DOWNLOADS,
            volumeInfo.volumeType);
      }),
      callback);
}

function testGetDriveConnectionState(callback) {
  reportPromise(
      volumeManagerFactory.getInstance().then(function(volumeManager) {
        // Default connection state is online
        assertEquals(VolumeManagerCommon.DriveConnectionType.ONLINE,
            volumeManager.getDriveConnectionState());

        // Sets it to offline.
        chrome.fileManagerPrivate.driveConnectionState =
            VolumeManagerCommon.DriveConnectionType.OFFLINE;
        assertEquals(VolumeManagerCommon.DriveConnectionType.OFFLINE,
            volumeManager.getDriveConnectionState());

        // Sets it back to online
        chrome.fileManagerPrivate.driveConnectionState =
            VolumeManagerCommon.DriveConnectionType.ONLINE;
        assertEquals(VolumeManagerCommon.DriveConnectionType.ONLINE,
            volumeManager.getDriveConnectionState());
      }),
      callback);
}

function testMountArchiveAndUnmount(callback) {
  // Set states of mock fileManagerPrivate APIs.
  const mountSourcePath = '/usr/local/home/test/Downloads/foobar.zip';
  chrome.fileManagerPrivate.mountSourcePath_ = mountSourcePath;
  chrome.fileManagerPrivate.fileSystemMap_['archive:foobar.zip'] =
      new MockFileSystem('archive:foobar.zip');

  reportPromise(
      volumeManagerFactory.getInstance().then(function(volumeManager) {
        var numberOfVolumes = volumeManager.volumeInfoList.length;

        return new Promise(function(resolve, reject) {
          // Mount an archieve
          volumeManager.mountArchive(
              'filesystem:chrome-extension://extensionid/external/' +
              'Downloads-test/foobar.zip',
              resolve, reject);

          mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent({
            eventType: 'mount',
            status: 'success',
            volumeMetadata: {
              volumeId: 'archive:foobar.zip',
              volumeLabel: 'foobar.zip',
              volumeType: VolumeManagerCommon.VolumeType.ARCHIVE,
              isReadOnly: true,
              sourcePath: mountSourcePath,
              profile: getMockProfile(),
              configurable: false,
              watchable: true,
              source: VolumeManagerCommon.Source.FILE
            }
          });
        }).then(function(result) {
          assertEquals(numberOfVolumes + 1,
                       volumeManager.volumeInfoList.length);

          return new Promise(function(resolve, reject) {
            // Unmount the mounted archievea
            volumeManager.volumeInfoList.addEventListener('splice', function() {
              assertEquals(numberOfVolumes,
                  volumeManager.volumeInfoList.length);
              resolve(true);
            });
            var entry = new MockFileEntry(
                new MockFileSystem('archive:foobar.zip'),
                '/foo.txt');
            var volumeInfo = volumeManager.getVolumeInfo(entry);
            volumeManager.unmount(volumeInfo);
          });
        });
      }),
      callback);
}

function testGetCurrentProfileVolumeInfo(callback) {
  reportPromise(
      volumeManagerFactory.getInstance().then(function(volumeManager) {
        var volumeInfo = volumeManager.getCurrentProfileVolumeInfo(
            VolumeManagerCommon.VolumeType.DRIVE);

        assertEquals('drive:drive-foobar%40chromium.org-hash',
            volumeInfo.volumeId);
        assertEquals(VolumeManagerCommon.VolumeType.DRIVE,
            volumeInfo.volumeType);
      }),
      callback);
}

function testGetLocationInfo(callback) {
  reportPromise(
      volumeManagerFactory.getInstance().then(function(volumeManager) {
        var downloadEntry = new MockFileEntry(
            new MockFileSystem('download:Downloads'),
            '/foo/bar/bla.zip');
        var downloadLocationInfo = volumeManager.getLocationInfo(downloadEntry);
        assertEquals(VolumeManagerCommon.RootType.DOWNLOADS,
            downloadLocationInfo.rootType);
        assertFalse(downloadLocationInfo.hasFixedLabel);
        assertFalse(downloadLocationInfo.isReadOnly);
        assertFalse(downloadLocationInfo.isRootEntry);

        var driveEntry = new MockFileEntry(
            new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
            '/root');
        var driveLocationInfo = volumeManager.getLocationInfo(driveEntry);
        assertEquals(VolumeManagerCommon.RootType.DRIVE,
            driveLocationInfo.rootType);
        assertTrue(driveLocationInfo.hasFixedLabel);
        assertFalse(driveLocationInfo.isReadOnly);
        assertTrue(driveLocationInfo.isRootEntry);

        var teamDrivesGrandRoot = new MockFileEntry(
            new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
            '/team_drives');
        var teamDrivesGrandRootLocationInfo =
            volumeManager.getLocationInfo(teamDrivesGrandRoot);
        assertEquals(
            VolumeManagerCommon.RootType.TEAM_DRIVES_GRAND_ROOT,
            teamDrivesGrandRootLocationInfo.rootType);
        assertTrue(teamDrivesGrandRootLocationInfo.hasFixedLabel);
        assertTrue(teamDrivesGrandRootLocationInfo.isReadOnly);
        assertTrue(teamDrivesGrandRootLocationInfo.isRootEntry);

        var teamDrive = new MockFileEntry(
            new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
            '/team_drives/MyTeamDrive');
        var teamDriveLocationInfo = volumeManager.getLocationInfo(teamDrive);
        assertEquals(
            VolumeManagerCommon.RootType.TEAM_DRIVE,
            teamDriveLocationInfo.rootType);
        assertFalse(teamDriveLocationInfo.hasFixedLabel);
        assertFalse(teamDriveLocationInfo.isReadOnly);
        assertTrue(teamDriveLocationInfo.isRootEntry);

        var driveFilesByIdDirectoryEntry = new MockDirectoryEntry(
            new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
            '/.files-by-id/123');
        var driveFilesByIdDirectoryLocationInfo =
            volumeManager.getLocationInfo(driveFilesByIdDirectoryEntry);
        assertEquals(
            VolumeManagerCommon.RootType.DRIVE_OTHER,
            driveFilesByIdDirectoryLocationInfo.rootType);
        assertFalse(driveFilesByIdDirectoryLocationInfo.hasFixedLabel);
        assertTrue(driveFilesByIdDirectoryLocationInfo.isReadOnly);
        assertFalse(driveFilesByIdDirectoryLocationInfo.isRootEntry);

        var driveFilesByIdEntry = new MockFileEntry(
            new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
            '/.files-by-id/123/foo.txt');
        var driveFilesByIdLocationInfo =
            volumeManager.getLocationInfo(driveFilesByIdEntry);
        assertEquals(
            VolumeManagerCommon.RootType.DRIVE_OTHER,
            driveFilesByIdLocationInfo.rootType);
        assertFalse(driveFilesByIdLocationInfo.hasFixedLabel);
        assertFalse(driveFilesByIdLocationInfo.isReadOnly);
        assertFalse(driveFilesByIdLocationInfo.isRootEntry);

        var androidRoot =
            new MockFileEntry(new MockFileSystem('android_files:0'), '/');
        var androidRootLocationInfo =
            volumeManager.getLocationInfo(androidRoot);
        assertTrue(androidRootLocationInfo.isReadOnly);
        assertTrue(androidRootLocationInfo.isRootEntry);

        var androidSubFolder = new MockFileEntry(
            new MockFileSystem('android_files:0'), '/Pictures');
        var androidSubFolderLocationInfo =
            volumeManager.getLocationInfo(androidSubFolder);
        assertFalse(androidSubFolderLocationInfo.isReadOnly);
        assertFalse(androidSubFolderLocationInfo.isRootEntry);

        const computersGrandRoot = new MockFileEntry(
            new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
            '/Computers');
        const computersGrandRootLocationInfo =
            volumeManager.getLocationInfo(computersGrandRoot);
        assertEquals(
            VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT,
            computersGrandRootLocationInfo.rootType);
        assertTrue(computersGrandRootLocationInfo.hasFixedLabel);
        assertTrue(computersGrandRootLocationInfo.isReadOnly);
        assertTrue(computersGrandRootLocationInfo.isRootEntry);

        const computer = new MockFileEntry(
            new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
            '/Computers/MyComputer');
        const computerLocationInfo = volumeManager.getLocationInfo(computer);
        assertEquals(
            VolumeManagerCommon.RootType.COMPUTER,
            computerLocationInfo.rootType);
        assertFalse(computerLocationInfo.hasFixedLabel);
        assertTrue(computerLocationInfo.isReadOnly);
        assertTrue(computerLocationInfo.isRootEntry);
      }),
      callback);
}

function testWhenReady(callback) {
  volumeManagerFactory.getInstance().then((volumeManager) => {
    const promiseBeforeAdd = volumeManager.whenVolumeInfoReady('volumeId');
    const volumeInfo = new VolumeInfoImpl(
        /* volumeType */ VolumeManagerCommon.VolumeType.MY_FILES,
        /* volumeId */ 'volumeId',
        /* fileSystem */ null,
        /* error */ undefined,
        /* deviceType */ undefined,
        /* devicePath */ undefined,
        /* isReadOnly */ false,
        /* isReadOnlyRemovableDevice */ false,
        /* profile */ {displayName: '', isCurrentProfile: true},
        /* label */ 'testLabel',
        /* extensionid */ undefined,
        /* hasMedia */ false,
        /* configurable */ false,
        /* watchable */ true,
        /* source */ VolumeManagerCommon.Source.FILE,
        /* diskFileSystemType */ VolumeManagerCommon.FileSystemType.UNKNOWN,
        /* iconSet*/ {});
    volumeManager.volumeInfoList.add(volumeInfo);
    const promiseAfterAdd = volumeManager.whenVolumeInfoReady('volumeId');
    reportPromise(
        Promise.all([promiseBeforeAdd, promiseAfterAdd]).then((volumes) => {
          assertEquals(volumeInfo, volumes[0]);
          assertEquals(volumeInfo, volumes[1]);
        }),
        callback);
  });
}

function testDriveMountedDuringInitialization(callback) {
  var sendMetadataListCallback;
  chrome.fileManagerPrivate.getVolumeMetadataList = function(callback) {
    sendMetadataListCallback = callback;
  };

  // Start initialization.
  var instancePromise = volumeManagerFactory.getInstance();

  // Drive is mounted during initialization.
  mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      volumeId: 'drive:drive-foobar%40chromium.org-hash',
      volumeType: VolumeManagerCommon.VolumeType.DRIVE,
      sourcePath: '/drive',
      profile: getMockProfile()
    }
  });

  // Complete initialization.
  sendMetadataListCallback([]);

  reportPromise(instancePromise.then(function(volumeManager) {
    assertTrue(!!volumeManager.getCurrentProfileVolumeInfo(
        VolumeManagerCommon.VolumeType.DRIVE));
  }), callback);
}
