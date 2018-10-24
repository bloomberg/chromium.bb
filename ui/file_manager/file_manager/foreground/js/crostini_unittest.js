// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.metrics = {
  recordSmallCount: function() {},
};

const volumeManagerTest = {
  getLocationInfo: (entry) => {
    return {rootType: 'testroot'};
  },
};

function testIsPathShared() {
  const mockFileSystem = new MockFileSystem('volumeId');
  const root = new MockDirectoryEntry(mockFileSystem, '/');
  const foo1 = new MockDirectoryEntry(mockFileSystem, '/foo1');
  const foobar1 = new MockDirectoryEntry(mockFileSystem, '/foo1/bar1');
  const foo2 = new MockDirectoryEntry(mockFileSystem, '/foo2');
  const foobar2 = new MockDirectoryEntry(mockFileSystem, '/foo2/bar2');

  assertFalse(Crostini.isPathShared(foo1, volumeManagerTest));

  Crostini.registerSharedPath(foo1, volumeManagerTest);
  assertFalse(Crostini.isPathShared(root, volumeManagerTest));
  assertTrue(Crostini.isPathShared(foo1, volumeManagerTest));
  assertTrue(Crostini.isPathShared(foobar1, volumeManagerTest));

  Crostini.registerSharedPath(foobar2, volumeManagerTest);
  assertFalse(Crostini.isPathShared(foo2, volumeManagerTest));
  assertTrue(Crostini.isPathShared(foobar2, volumeManagerTest));

  Crostini.unregisterSharedPath(foobar2, volumeManagerTest);
  assertFalse(Crostini.isPathShared(foobar2, volumeManagerTest));
}

const volumeManagerDownloads = {
  getLocationInfo: (entry) => {
    return {rootType: 'downloads'};
  },
};

function testCanSharePath() {
  Crostini.IS_CROSTINI_FILES_ENABLED = true;

  const mockFileSystem = new MockFileSystem('volumeId');
  const root = new MockDirectoryEntry(mockFileSystem, '/');
  const rootFile = new MockEntry(mockFileSystem, '/file');
  const rootFolder = new MockDirectoryEntry(mockFileSystem, '/folder');
  const fooFile = new MockEntry(mockFileSystem, '/foo/file');
  const fooFolder = new MockDirectoryEntry(mockFileSystem, '/foo/folder');

  assertFalse(Crostini.canSharePath(root, true, volumeManagerTest));
  assertFalse(Crostini.canSharePath(root, false, volumeManagerTest));
  assertFalse(Crostini.canSharePath(rootFile, true, volumeManagerTest));
  assertFalse(Crostini.canSharePath(rootFile, false, volumeManagerTest));
  assertFalse(Crostini.canSharePath(rootFolder, true, volumeManagerTest));
  assertFalse(Crostini.canSharePath(rootFolder, false, volumeManagerTest));
  assertFalse(Crostini.canSharePath(fooFile, true, volumeManagerTest));
  assertFalse(Crostini.canSharePath(fooFile, false, volumeManagerTest));
  assertFalse(Crostini.canSharePath(fooFolder, true, volumeManagerTest));
  assertFalse(Crostini.canSharePath(fooFolder, false, volumeManagerTest));

  assertFalse(Crostini.canSharePath(root, true, volumeManagerDownloads));
  assertFalse(Crostini.canSharePath(root, false, volumeManagerDownloads));
  assertFalse(Crostini.canSharePath(rootFile, true, volumeManagerDownloads));
  assertTrue(Crostini.canSharePath(rootFile, false, volumeManagerDownloads));
  assertTrue(Crostini.canSharePath(rootFolder, true, volumeManagerDownloads));
  assertTrue(Crostini.canSharePath(rootFolder, false, volumeManagerDownloads));
  assertFalse(Crostini.canSharePath(fooFile, true, volumeManagerDownloads));
  assertTrue(Crostini.canSharePath(fooFile, false, volumeManagerDownloads));
  assertTrue(Crostini.canSharePath(fooFolder, true, volumeManagerDownloads));
  assertTrue(Crostini.canSharePath(fooFolder, false, volumeManagerDownloads));
}
