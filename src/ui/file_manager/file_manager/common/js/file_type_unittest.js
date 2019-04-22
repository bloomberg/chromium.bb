// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/*
 * Tests that Downloads icon is customized within Downloads root, but not in
 * others.
 */
function testDownloadsIcon() {
  const fileSystem = new MockFileSystem('fake-fs');
  const filenames = [
    '/folder/',
    '/folder/file_a.txt',
    '/Downloads/',
    '/Downloads/file_b.txt',
  ];
  fileSystem.populate(filenames);

  const folder = fileSystem.entries['/folder'];
  const fileA = fileSystem.entries['/folder/file_a.txt'];
  const downloads = fileSystem.entries['/Downloads'];
  const fileB = fileSystem.entries['/Downloads/file_b.txt'];

  const downloadsRoot = VolumeManagerCommon.RootType.DOWNLOADS;
  const driveRoot = VolumeManagerCommon.RootType.DRIVE;
  const androidRoot = VolumeManagerCommon.RootType.ANDROID_FILES;

  const mimetype = undefined;
  assertEquals('folder', FileType.getIcon(folder, mimetype, downloadsRoot));
  assertEquals('text', FileType.getIcon(fileA, mimetype, downloadsRoot));
  assertEquals('text', FileType.getIcon(fileB, mimetype, downloadsRoot));

  assertEquals(
      'downloads', FileType.getIcon(downloads, mimetype, downloadsRoot));
  assertEquals('folder', FileType.getIcon(downloads, mimetype, driveRoot));
  assertEquals('folder', FileType.getIcon(downloads, mimetype, androidRoot));
}
