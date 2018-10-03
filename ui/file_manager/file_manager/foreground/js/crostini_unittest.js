// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const volumeManager = {
  getLocationInfo: (entry) => {
    return {root: 'testroot'};
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
