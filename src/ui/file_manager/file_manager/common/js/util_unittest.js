// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let fileSystem;

function setUp() {
  fileSystem = new MockFileSystem('fake-volume');
  const filenames = [
    '/file_a.txt',
    '/file_b.txt',
    '/file_c.txt',
    '/file_d.txt',
    '/dir_a/file_e.txt',
    '/dir_a/file_f.txt',
    '/dir_a/dir_b/dir_c/file_g.txt',
  ];
  fileSystem.populate(filenames);
}

function testReadEntriesRecursively(callback) {
  let foundEntries = [];
  util.readEntriesRecursively(
      fileSystem.root,
      (entries) => {
        const fileEntries = entries.filter(entry => !entry.isDirectory);
        foundEntries = foundEntries.concat(fileEntries);
      },
      () => {
        // If all directories are read recursively, found files should be 6.
        assertEquals(7, foundEntries.length);
        callback();
      },
      () => {}, () => false);
}

function testReadEntriesRecursivelyLevel0(callback) {
  let foundEntries = [];
  util.readEntriesRecursively(
      fileSystem.root,
      (entries) => {
        const fileEntries = entries.filter(entry => !entry.isDirectory);
        foundEntries = foundEntries.concat(fileEntries);
      },
      () => {
        // If only the top directory is read, found entries should be 3.
        assertEquals(4, foundEntries.length);
        callback();
      },
      () => {}, () => false, 0 /* opt_maxDepth */);
}

function testReadEntriesRecursivelyLevel1(callback) {
  let foundEntries = [];
  util.readEntriesRecursively(
      fileSystem.root,
      (entries) => {
        const fileEntries = entries.filter(entry => !entry.isDirectory);
        foundEntries = foundEntries.concat(fileEntries);
      },
      () => {
        // If we delve directories only one level, found entries should be 5.
        assertEquals(6, foundEntries.length);
        callback();
      },
      () => {}, () => false, 1 /* opt_maxDepth */);
}
