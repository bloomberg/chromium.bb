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
  crostini.initVolumeManager(volumeManager);
}

/**
 * Sets the DriveFs enabled state.
 * @param {boolean} enabled
 */
function setDriveFsEnabled(enabled) {
  window.loadTimeData.data['DRIVE_FS_ENABLED'] = enabled;
}

/**
 * Tests init sets crostini and PluginVm enabled status.
 */
function testInitCrostiniPluginVmEnabled() {
  window.loadTimeData.data['CROSTINI_ENABLED'] = true;
  window.loadTimeData.data['PLUGIN_VM_ENABLED'] = true;
  crostini.initEnabled();
  assertTrue(crostini.isEnabled('termina'));
  assertTrue(crostini.isEnabled('PvmDefault'));

  window.loadTimeData.data['CROSTINI_ENABLED'] = false;
  window.loadTimeData.data['PLUGIN_VM_ENABLED'] = false;
  crostini.initEnabled();
  assertFalse(crostini.isEnabled('termina'));
  assertFalse(crostini.isEnabled('PvmDefault'));
}

/**
 * Tests init sets crostini root access allowed status.
 */
function testInitCrostiniRootAccessAllowed() {
  window.loadTimeData.data['CROSTINI_ROOT_ACCESS_ALLOWED'] = true;
  crostini.initEnabled();
  assertTrue(crostini.isRootAccessAllowed('termina'));

  window.loadTimeData.data['CROSTINI_ROOT_ACCESS_ALLOWED'] = false;
  crostini.initEnabled();
  assertFalse(crostini.isRootAccessAllowed('termina'));
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

  assertFalse(crostini.isPathShared('vm1', a));
  assertFalse(crostini.isPathShared('vm2', a));

  crostini.registerSharedPath('vm1', a);
  assertFalse(crostini.isPathShared('vm1', root));
  assertTrue(crostini.isPathShared('vm1', a));
  assertTrue(crostini.isPathShared('vm1', aa));

  crostini.registerSharedPath('vm2', a);
  assertTrue(crostini.isPathShared('vm2', a));

  crostini.registerSharedPath('vm1', bb);
  assertFalse(crostini.isPathShared('vm1', b));
  assertTrue(crostini.isPathShared('vm1', bb));

  crostini.unregisterSharedPath('vm1', bb);
  assertFalse(crostini.isPathShared('vm1', bb));

  // Test collapsing vm1, but not vm2.  Setup with /a/a, /a/b, /b
  crostini.unregisterSharedPath('vm1', a);
  crostini.unregisterSharedPath('vm2', a);
  crostini.registerSharedPath('vm1', aa);
  crostini.registerSharedPath('vm1', ab);
  crostini.registerSharedPath('vm1', b);
  crostini.registerSharedPath('vm2', aa);
  assertFalse(crostini.isPathShared('vm1', a));
  assertFalse(crostini.isPathShared('vm2', a));
  assertTrue(crostini.isPathShared('vm1', aa));
  assertTrue(crostini.isPathShared('vm1', ab));
  assertTrue(crostini.isPathShared('vm1', b));
  assertTrue(crostini.isPathShared('vm2', aa));
  // Add /a for vm1, collapses /a/a, /a/b in vm1.
  crostini.registerSharedPath('vm1', a);
  assertTrue(crostini.isPathShared('vm1', a));
  assertTrue(crostini.isPathShared('vm1', aa));
  assertTrue(crostini.isPathShared('vm1', ab));
  assertTrue(crostini.isPathShared('vm1', b));
  assertTrue(crostini.isPathShared('vm2', aa));
  assertFalse(crostini.isPathShared('vm2', a));
  // Unregister /a for vm1, /a/a and /a/b should be lost in vm1.
  crostini.unregisterSharedPath('vm1', a);
  assertFalse(crostini.isPathShared('vm1', a));
  assertFalse(crostini.isPathShared('vm1', aa));
  assertFalse(crostini.isPathShared('vm1', ab));
  assertTrue(crostini.isPathShared('vm1', b));
  assertTrue(crostini.isPathShared('vm2', aa));
  // Register root for vm1, collapses all vm1.
  crostini.registerSharedPath('vm1', root);
  assertTrue(crostini.isPathShared('vm1', a));
  assertTrue(crostini.isPathShared('vm1', aa));
  assertTrue(crostini.isPathShared('vm1', ab));
  assertTrue(crostini.isPathShared('vm1', b));
  // Unregister root for vm1, all vm1 should be lost.
  crostini.unregisterSharedPath('vm1', root);
  assertFalse(crostini.isPathShared('vm1', a));
  assertFalse(crostini.isPathShared('vm1', aa));
  assertFalse(crostini.isPathShared('vm1', ab));
  assertFalse(crostini.isPathShared('vm1', b));
  assertTrue(crostini.isPathShared('vm2', aa));
}

/*
 * Tests disallowed and allowed shared paths.
 */
function testCanSharePath() {
  crostini.setEnabled('vm', true);

  const mockFileSystem = new MockFileSystem('test');
  const root = new MockDirectoryEntry(mockFileSystem, '/');
  const rootFile = new MockEntry(mockFileSystem, '/file');
  const rootFolder = new MockDirectoryEntry(mockFileSystem, '/folder');
  const fooFile = new MockEntry(mockFileSystem, '/foo/file');
  const fooFolder = new MockDirectoryEntry(mockFileSystem, '/foo/folder');

  // Test with DriveFs disabled.
  setDriveFsEnabled(false);
  const disallowed = [
    'computers_grand_root', 'computer', 'drive', 'shared_drives_grand_root',
    'team_drive', 'test'
  ];
  for (const type of disallowed) {
    volumeManagerRootType =
        /** @type {!VolumeManagerCommon.RootType<string>} */ (type);
    assertFalse(crostini.canSharePath('vm', root, true));
    assertFalse(crostini.canSharePath('vm', root, false));
    assertFalse(crostini.canSharePath('vm', rootFile, true));
    assertFalse(crostini.canSharePath('vm', rootFile, false));
    assertFalse(crostini.canSharePath('vm', rootFolder, true));
    assertFalse(crostini.canSharePath('vm', rootFolder, false));
    assertFalse(crostini.canSharePath('vm', fooFile, true));
    assertFalse(crostini.canSharePath('vm', fooFile, false));
    assertFalse(crostini.canSharePath('vm', fooFolder, true));
    assertFalse(crostini.canSharePath('vm', fooFolder, false));
  }

  // Test with DriveFs enabled.
  setDriveFsEnabled(true);
  // TODO(crbug.com/917920): Add computers_grand_root and computers when DriveFS
  // enforces allowed write paths.

  const allowed = [
    'downloads', 'removable', 'android_files', 'drive',
    'shared_drives_grand_root', 'team_drive'
  ];
  for (const type of allowed) {
    volumeManagerRootType = type;
    // TODO(crbug.com/958840): Sharing Play files root is disallowed until
    // we can ensure it will not also share Downloads.
    if (type === 'android_files') {
      assertFalse(crostini.canSharePath('vm', root, true));
      assertFalse(crostini.canSharePath('vm', root, false));
    } else {
      assertTrue(crostini.canSharePath('vm', root, true));
      assertTrue(crostini.canSharePath('vm', root, false));
    }
    assertFalse(crostini.canSharePath('vm', rootFile, true));
    assertTrue(crostini.canSharePath('vm', rootFile, false));
    assertTrue(crostini.canSharePath('vm', rootFolder, true));
    assertTrue(crostini.canSharePath('vm', rootFolder, false));
    assertFalse(crostini.canSharePath('vm', fooFile, true));
    assertTrue(crostini.canSharePath('vm', fooFile, false));
    assertTrue(crostini.canSharePath('vm', fooFolder, true));
    assertTrue(crostini.canSharePath('vm', fooFolder, false));
  }

  // TODO(crbug.com/917920): Remove when DriveFS enforces allowed write paths.
  const grandRootFolder = new MockDirectoryEntry(mockFileSystem, '/Computers');
  const computerRootFolder =
      new MockDirectoryEntry(mockFileSystem, '/Computers/My');
  const computerFolder =
      new MockDirectoryEntry(mockFileSystem, '/Computers/My/foo');
  volumeManagerRootType = VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT;
  assertFalse(crostini.canSharePath('vm', root, false));
  assertFalse(crostini.canSharePath('vm', grandRootFolder, false));
  assertFalse(crostini.canSharePath('vm', computerRootFolder, false));
  assertFalse(crostini.canSharePath('vm', computerFolder, false));
  volumeManagerRootType = VolumeManagerCommon.RootType.COMPUTER;
  assertFalse(crostini.canSharePath('vm', root, false));
  assertFalse(crostini.canSharePath('vm', grandRootFolder, false));
  assertFalse(crostini.canSharePath('vm', computerRootFolder, false));
  assertTrue(crostini.canSharePath('vm', computerFolder, false));
}
