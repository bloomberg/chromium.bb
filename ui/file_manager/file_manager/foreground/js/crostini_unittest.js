// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.metrics = {
  recordSmallCount: function() {},
};

window.loadTimeData = {
  getBoolean: function() {
    return true;
  }
};

let volumeManagerRootType = 'testroot';
const volumeManager = {
  getLocationInfo: (entry) => {
    return {rootType: volumeManagerRootType};
  },
};

function testIsPathShared() {
  const mockFileSystem = new MockFileSystem('volumeId');
  const root = new MockDirectoryEntry(mockFileSystem, '/');
  const foo1 = new MockDirectoryEntry(mockFileSystem, '/foo1');
  const foobar1 = new MockDirectoryEntry(mockFileSystem, '/foo1/bar1');
  const foo2 = new MockDirectoryEntry(mockFileSystem, '/foo2');
  const foobar2 = new MockDirectoryEntry(mockFileSystem, '/foo2/bar2');

  assertFalse(Crostini.isPathShared(foo1, volumeManager));

  Crostini.registerSharedPath(foo1, volumeManager);
  assertFalse(Crostini.isPathShared(root, volumeManager));
  assertTrue(Crostini.isPathShared(foo1, volumeManager));
  assertTrue(Crostini.isPathShared(foobar1, volumeManager));

  Crostini.registerSharedPath(foobar2, volumeManager);
  assertFalse(Crostini.isPathShared(foo2, volumeManager));
  assertTrue(Crostini.isPathShared(foobar2, volumeManager));

  Crostini.unregisterSharedPath(foobar2, volumeManager);
  assertFalse(Crostini.isPathShared(foobar2, volumeManager));
}


function testCanSharePath() {
  Crostini.IS_CROSTINI_FILES_ENABLED = true;

  const mockFileSystem = new MockFileSystem('volumeId');
  const root = new MockDirectoryEntry(mockFileSystem, '/');
  const rootFile = new MockEntry(mockFileSystem, '/file');
  const rootFolder = new MockDirectoryEntry(mockFileSystem, '/folder');
  const fooFile = new MockEntry(mockFileSystem, '/foo/file');
  const fooFolder = new MockDirectoryEntry(mockFileSystem, '/foo/folder');

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

  for (type of
           ['downloads', 'drive', 'team_drive', 'computers_grand_root',
            'removable']) {
    volumeManagerRootType = type;
    assertFalse(Crostini.canSharePath(root, true, volumeManager));
    assertFalse(Crostini.canSharePath(root, false, volumeManager));
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
