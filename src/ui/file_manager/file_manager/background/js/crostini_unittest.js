// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Mock metrics.
 * @type {!Object}
 */
window.metrics = {
  recordSmallCount: function() {},
};

/** @type {!VolumeManagerCommon.RootType<string>} */
let volumeManagerRootType;

/** @type {!VolumeManager} */
let volumeManager;

/** @type {!Crostini} */
let crostini;

// Set up the test components.
function setUp() {
  // Mock LoadTimeData strings.
  window.loadTimeData = {
    data: {
      'DRIVE_FS_ENABLED': false,
    },
    getBoolean: function(key) {
      return window.loadTimeData.data[key];
    },
    getString: id => id,
  };

  // Create a fake volume manager that provides entry location info.
  volumeManager = /** @type {!VolumeManager} */ ({
    getLocationInfo: (entry) => {
      return /** @type {!EntryLocation} */ ({
        rootType: volumeManagerRootType,
      });
    },
  });

  // Reset initial root type.
  volumeManagerRootType =
      /** @type {!VolumeManagerCommon.RootType<string>} */ ('testroot');

  // Create and initialize Crostini.
  crostini = createCrostiniForTest();
  crostini.init(volumeManager);
}

/**
 * Sets the DriveFs enabled state.
 * @param {boolean} enabled
 */
function setDriveFsEnabled(enabled) {
  window.loadTimeData.data['DRIVE_FS_ENABLED'] = enabled;
}

/**
 * Tests path sharing.
 */
function testIsPathShared() {
  const mockFileSystem = new MockFileSystem('volumeId');
  const root = new MockDirectoryEntry(mockFileSystem, '/');
  const a = new MockDirectoryEntry(mockFileSystem, '/a');
  const aa = new MockDirectoryEntry(mockFileSystem, '/a/a');
  const ab = new MockDirectoryEntry(mockFileSystem, '/a/b');
  const b = new MockDirectoryEntry(mockFileSystem, '/b');
  const bb = new MockDirectoryEntry(mockFileSystem, '/b/b');

  assertFalse(crostini.isPathShared(a));

  crostini.registerSharedPath(a);
  assertFalse(crostini.isPathShared(root));
  assertTrue(crostini.isPathShared(a));
  assertTrue(crostini.isPathShared(aa));

  crostini.registerSharedPath(bb);
  assertFalse(crostini.isPathShared(b));
  assertTrue(crostini.isPathShared(bb));

  crostini.unregisterSharedPath(bb);
  assertFalse(crostini.isPathShared(bb));

  // Test collapsing.  Setup with /a/a, /a/b, /b
  crostini.unregisterSharedPath(a);
  crostini.registerSharedPath(aa);
  crostini.registerSharedPath(ab);
  crostini.registerSharedPath(b);
  assertFalse(crostini.isPathShared(a));
  assertTrue(crostini.isPathShared(aa));
  assertTrue(crostini.isPathShared(ab));
  assertTrue(crostini.isPathShared(b));
  // Add /a, collapses /a/a, /a/b
  crostini.registerSharedPath(a);
  assertTrue(crostini.isPathShared(a));
  assertTrue(crostini.isPathShared(aa));
  assertTrue(crostini.isPathShared(ab));
  assertTrue(crostini.isPathShared(b));
  // Unregister /a, /a/a and /a/b should be lost.
  crostini.unregisterSharedPath(a);
  assertFalse(crostini.isPathShared(a));
  assertFalse(crostini.isPathShared(aa));
  assertFalse(crostini.isPathShared(ab));
  assertTrue(crostini.isPathShared(b));
  // Register root, collapses all.
  crostini.registerSharedPath(root);
  assertTrue(crostini.isPathShared(a));
  assertTrue(crostini.isPathShared(aa));
  assertTrue(crostini.isPathShared(ab));
  assertTrue(crostini.isPathShared(b));
  // Unregister root, all should be lost.
  crostini.unregisterSharedPath(root);
  assertFalse(crostini.isPathShared(a));
  assertFalse(crostini.isPathShared(aa));
  assertFalse(crostini.isPathShared(ab));
  assertFalse(crostini.isPathShared(b));
}

/*
 * Tests disallowed and allowed shared paths.
 */
function testCanSharePath() {
  crostini.setEnabled(true);

  const mockFileSystem = new MockFileSystem('test');
  const root = new MockDirectoryEntry(mockFileSystem, '/');
  const rootFile = new MockEntry(mockFileSystem, '/file');
  const rootFolder = new MockDirectoryEntry(mockFileSystem, '/folder');
  const fooFile = new MockEntry(mockFileSystem, '/foo/file');
  const fooFolder = new MockDirectoryEntry(mockFileSystem, '/foo/folder');

  // Test with DriveFs disabled.
  setDriveFsEnabled(false);
  const disallowed = [
    'computers_grand_root', 'computer', 'drive', 'team_drives_grand_root',
    'team_drive', 'test'
  ];
  for (const type of disallowed) {
    volumeManagerRootType =
        /** @type {!VolumeManagerCommon.RootType<string>} */ (type);
    assertFalse(crostini.canSharePath(root, true));
    assertFalse(crostini.canSharePath(root, false));
    assertFalse(crostini.canSharePath(rootFile, true));
    assertFalse(crostini.canSharePath(rootFile, false));
    assertFalse(crostini.canSharePath(rootFolder, true));
    assertFalse(crostini.canSharePath(rootFolder, false));
    assertFalse(crostini.canSharePath(fooFile, true));
    assertFalse(crostini.canSharePath(fooFile, false));
    assertFalse(crostini.canSharePath(fooFolder, true));
    assertFalse(crostini.canSharePath(fooFolder, false));
  }

  // Test with DriveFs enabled.
  setDriveFsEnabled(true);
  // TODO(crbug.com/917920): Add computers_grand_root and computers when DriveFS
  // enforces allowed write paths.
  const allowed = [
    'downloads', 'removable', 'android_files', 'drive',
    'team_drives_grand_root', 'team_drive'
  ];
  for (const type of allowed) {
    volumeManagerRootType = type;
    assertTrue(crostini.canSharePath(root, true));
    assertTrue(crostini.canSharePath(root, false));
    assertFalse(crostini.canSharePath(rootFile, true));
    assertTrue(crostini.canSharePath(rootFile, false));
    assertTrue(crostini.canSharePath(rootFolder, true));
    assertTrue(crostini.canSharePath(rootFolder, false));
    assertFalse(crostini.canSharePath(fooFile, true));
    assertTrue(crostini.canSharePath(fooFile, false));
    assertTrue(crostini.canSharePath(fooFolder, true));
    assertTrue(crostini.canSharePath(fooFolder, false));
  }

  // TODO(crbug.com/917920): Remove when DriveFS enforces allowed write paths.
  const grandRootFolder = new MockDirectoryEntry(mockFileSystem, '/Computers');
  const computerRootFolder =
      new MockDirectoryEntry(mockFileSystem, '/Computers/My');
  const computerFolder =
      new MockDirectoryEntry(mockFileSystem, '/Computers/My/foo');
  volumeManagerRootType = VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT;
  assertFalse(crostini.canSharePath(root, false));
  assertFalse(crostini.canSharePath(grandRootFolder, false));
  assertFalse(crostini.canSharePath(computerRootFolder, false));
  assertFalse(crostini.canSharePath(computerFolder, false));
  volumeManagerRootType = VolumeManagerCommon.RootType.COMPUTER;
  assertFalse(crostini.canSharePath(root, false));
  assertFalse(crostini.canSharePath(grandRootFolder, false));
  assertFalse(crostini.canSharePath(computerRootFolder, false));
  assertTrue(crostini.canSharePath(computerFolder, false));
}
