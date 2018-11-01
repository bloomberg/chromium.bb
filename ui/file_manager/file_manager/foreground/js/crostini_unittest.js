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

function testIsPathShared() {
  const mockFileSystem = new MockFileSystem('volumeId');
  const root = new MockDirectoryEntry(mockFileSystem, '/');
  const a = new MockDirectoryEntry(mockFileSystem, '/a');
  const aa = new MockDirectoryEntry(mockFileSystem, '/a/a');
  const ab = new MockDirectoryEntry(mockFileSystem, '/a/b');
  const b = new MockDirectoryEntry(mockFileSystem, '/b');
  const bb = new MockDirectoryEntry(mockFileSystem, '/b/b');

  assertFalse(Crostini.isPathShared(a, volumeManager));

  Crostini.registerSharedPath(a, volumeManager);
  assertFalse(Crostini.isPathShared(root, volumeManager));
  assertTrue(Crostini.isPathShared(a, volumeManager));
  assertTrue(Crostini.isPathShared(aa, volumeManager));

  Crostini.registerSharedPath(bb, volumeManager);
  assertFalse(Crostini.isPathShared(b, volumeManager));
  assertTrue(Crostini.isPathShared(bb, volumeManager));

  Crostini.unregisterSharedPath(bb, volumeManager);
  assertFalse(Crostini.isPathShared(bb, volumeManager));

  // Test collapsing.  Setup with /a/a, /a/b, /b
  Crostini.unregisterSharedPath(a, volumeManager);
  Crostini.registerSharedPath(aa, volumeManager);
  Crostini.registerSharedPath(ab, volumeManager);
  Crostini.registerSharedPath(b, volumeManager);
  assertFalse(Crostini.isPathShared(a, volumeManager));
  assertTrue(Crostini.isPathShared(aa, volumeManager));
  assertTrue(Crostini.isPathShared(ab, volumeManager));
  assertTrue(Crostini.isPathShared(b, volumeManager));
  // Add /a, collapses /a/a, /a/b
  Crostini.registerSharedPath(a, volumeManager);
  assertTrue(Crostini.isPathShared(a, volumeManager));
  assertTrue(Crostini.isPathShared(aa, volumeManager));
  assertTrue(Crostini.isPathShared(ab, volumeManager));
  assertTrue(Crostini.isPathShared(b, volumeManager));
  // Unregister /a, /a/a and /a/b should be lost.
  Crostini.unregisterSharedPath(a, volumeManager);
  assertFalse(Crostini.isPathShared(a, volumeManager));
  assertFalse(Crostini.isPathShared(aa, volumeManager));
  assertFalse(Crostini.isPathShared(ab, volumeManager));
  assertTrue(Crostini.isPathShared(b, volumeManager));
  // Register root, collapses all.
  Crostini.registerSharedPath(root, volumeManager);
  assertTrue(Crostini.isPathShared(a, volumeManager));
  assertTrue(Crostini.isPathShared(aa, volumeManager));
  assertTrue(Crostini.isPathShared(ab, volumeManager));
  assertTrue(Crostini.isPathShared(b, volumeManager));
  // Unregister root, all should be lost.
  Crostini.unregisterSharedPath(root, volumeManager);
  assertFalse(Crostini.isPathShared(a, volumeManager));
  assertFalse(Crostini.isPathShared(aa, volumeManager));
  assertFalse(Crostini.isPathShared(ab, volumeManager));
  assertFalse(Crostini.isPathShared(b, volumeManager));
}


function testCanSharePath() {
  Crostini.IS_CROSTINI_FILES_ENABLED = true;

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
    assertFalse(Crostini.canSharePath(root, true, volumeManager));
    assertFalse(Crostini.canSharePath(root, false, volumeManager));
    assertFalse(Crostini.canSharePath(rootFile, true, volumeManager));
    assertFalse(Crostini.canSharePath(rootFile, false, volumeManager));
    assertFalse(Crostini.canSharePath(rootFolder, true, volumeManager));
    assertFalse(Crostini.canSharePath(rootFolder, false, volumeManager));
    assertFalse(Crostini.canSharePath(fooFile, true, volumeManager));
    assertFalse(Crostini.canSharePath(fooFile, false, volumeManager));
    assertFalse(Crostini.canSharePath(fooFolder, true, volumeManager));
    assertFalse(Crostini.canSharePath(fooFolder, false, volumeManager));
  }

  window.loadTimeData.data['DRIVE_FS_ENABLED'] = true;
  const allowed = new Map([
    ...Crostini.VALID_ROOT_TYPES_FOR_SHARE,
    ...Crostini.VALID_DRIVE_FS_ROOT_TYPES_FOR_SHARE
  ]);
  for (let type of allowed.keys()) {
    volumeManagerRootType = type;
    assertTrue(Crostini.canSharePath(root, true, volumeManager));
    assertTrue(Crostini.canSharePath(root, false, volumeManager));
    assertFalse(Crostini.canSharePath(rootFile, true, volumeManager));
    assertTrue(Crostini.canSharePath(rootFile, false, volumeManager));
    assertTrue(Crostini.canSharePath(rootFolder, true, volumeManager));
    assertTrue(Crostini.canSharePath(rootFolder, false, volumeManager));
    assertFalse(Crostini.canSharePath(fooFile, true, volumeManager));
    assertTrue(Crostini.canSharePath(fooFile, false, volumeManager));
    assertTrue(Crostini.canSharePath(fooFolder, true, volumeManager));
    assertTrue(Crostini.canSharePath(fooFolder, false, volumeManager));
  }
}

function task(id) {
  return /** @type{!chrome.fileManagerPrivate.FileTask} */ ({taskId: id});
}

function testTaskRequiresSharing() {
  assertTrue(Crostini.taskRequiresSharing(task('app|crostini|open-with')));
  assertTrue(
      Crostini.taskRequiresSharing(task('appId|x|install-linux-package')));
  assertFalse(Crostini.taskRequiresSharing(task('appId|x|open-with')));
}
