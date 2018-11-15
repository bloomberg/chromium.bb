// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.metrics = {
  recordSmallCount: function() {},
};

window.loadTimeData = {
  data: {'DRIVE_FS_ENABLED': false},
  getBoolean: function(key) {
    return window.loadTimeData.data[key];
  }
};

let volumeManagerRootType = 'testroot';
const volumeManager = /** @type {!VolumeManager} */ ({
  getLocationInfo: (entry) => {
    return {rootType: volumeManagerRootType};
  },
});


const crostini = new Crostini();
crostini.init(volumeManager);

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

function testCanSharePath() {
  crostini.setEnabled(true);

  const mockFileSystem = new MockFileSystem('test');
  const root = new MockDirectoryEntry(mockFileSystem, '/');
  const rootFile = new MockEntry(mockFileSystem, '/file');
  const rootFolder = new MockDirectoryEntry(mockFileSystem, '/folder');
  const fooFile = new MockEntry(mockFileSystem, '/foo/file');
  const fooFolder = new MockDirectoryEntry(mockFileSystem, '/foo/folder');

  window.loadTimeData.data['DRIVE_FS_ENABLED'] = false;
  const disallowed = new Map(Crostini.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE);
  disallowed.set('test', 'test');
  for (let type of disallowed.keys()) {
    volumeManagerRootType = type;
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

  window.loadTimeData.data['DRIVE_FS_ENABLED'] = true;
  const allowed = new Map([
    ...Crostini.VALID_ROOT_TYPES_FOR_SHARE,
    ...Crostini.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE
  ]);
  for (let type of allowed.keys()) {
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
}
