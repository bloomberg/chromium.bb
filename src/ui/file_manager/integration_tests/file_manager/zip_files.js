// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchive file list entry.
 */
function getUnzippedFileListRowEntries() {
  return [
    ['image.png', '272 bytes', 'PNG image', 'Sep 2, 2013, 10:01 PM'],
    ['text.txt', '51 bytes', 'Plain text', 'Sep 2, 2013, 10:01 PM']
  ];
}

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchiveSJIS file list entry.
 */
function getUnzippedFileListRowEntriesSjisRoot() {
  return [
    // Folder name in Japanese language.
    ['新しいフォルダ', '--', 'Folder', 'Dec 31, 1980, 12:00 AM']
  ];
}

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchiveSJIS file list entry and moving into the subdirectory.
 */
function getUnzippedFileListRowEntriesSjisSubdir() {
  return [
    // ソ(SJIS:835C) contains backslash code on the 2nd byte. The app and the
    // extension should not confuse it with an escape characater.
    ['SJIS_835C_ソ.txt', '113 bytes', 'Plain text', 'Dec 31, 1980, 12:00 AM'],
    // Another file containing SJIS Japanese characters.
    [
      '新しいテキスト ドキュメント.txt', '52 bytes', 'Plain text',
      'Oct 2, 2001, 12:34 PM'
    ]
  ];
}

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchiveMacOs file list entry.
 */
function getUnzippedFileListRowEntriesMacOsRoot() {
  return [
    // File name in non-ASCII (UTF-8) characters.
    ['ファイル.dat', '16 bytes', 'DAT file', 'Jul 8, 2001, 12:34 PM']
  ];
}

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchiveWithAbsolutePaths file list entry.
 */
function getUnzippedFileListRowEntriesAbsolutePathsRoot() {
  return [
    ['foo', '--', 'Folder', 'Oct 11, 2018, 9:44 AM'],
    ['hello.txt', '13 bytes', 'Plain text', 'Oct 11, 2018, 9:44 AM']
  ];
}

/**
 * Returns the expected file list row entries after opening (unzipping) the
 * ENTRIES.zipArchiveWithAbsolutePaths file list entry and moving into the
 * subdirectory.
 */
function getUnzippedFileListRowEntriesAbsolutePathsSubdir() {
  return [['bye.txt', '9 bytes', 'Plain text', 'Oct 11, 2018, 9:44 AM']];
}

/**
 * Tests zip file open (aka unzip) from Downloads.
 */
testcase.zipFileOpenDownloads = async function() {
  // Open Files app on Downloads containing a zip file.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.zipArchive], []);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['archive.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntries();
  await remoteCall.waitForFiles(appId, files);
};

/**
 * Tests zip file, with absolute paths, open (aka unzip) from Downloads.
 */
testcase.zipFileOpenDownloadsWithAbsolutePaths = async function() {
  // Open Files app on Downloads containing a zip file.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.zipArchiveWithAbsolutePaths],
      []);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['absolute_paths.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntriesAbsolutePathsRoot();
  await remoteCall.waitForFiles(appId, files);

  // Select the directory in the ZIP file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['foo']),
      'selectFile failed');

  // Press the Enter key.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files2 = getUnzippedFileListRowEntriesAbsolutePathsSubdir();
  await remoteCall.waitForFiles(appId, files2);
};

/**
 * Tests zip file open (aka unzip) from Google Drive.
 */
testcase.zipFileOpenDrive = async function() {
  // Open Files app on Drive containing a zip file.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DRIVE, null, [], [ENTRIES.zipArchive]);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['archive.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntries();
  await remoteCall.waitForFiles(appId, files);
};

/**
 * Tests zip file open (aka unzip) from a removable USB volume.
 */
testcase.zipFileOpenUsb = async function() {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Open Files app on Drive.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DRIVE, null, [], [ENTRIES.beautiful]);

  // Mount empty USB volume in the Drive window.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [USB_VOLUME_QUERY]);

  // Add zip file to the USB volume.
  await addEntries(['usb'], [ENTRIES.zipArchive]);

  // Verify the USB file list.
  const archive = [ENTRIES.zipArchive.getExpectedRow()];
  await remoteCall.waitForFiles(appId, archive);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['archive.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntries();
  await remoteCall.waitForFiles(appId, files);
};

/**
 * Returns the expected file list rows after invoking the 'Zip selection' menu
 * command on the ENTRIES.photos file list item.
 */
function getZipSelectionFileListRowEntries() {
  return [
    ['photos', '--', 'Folder', 'Jan 1, 1980, 11:59 PM'],
    ['photos.zip', '206 bytes', 'Zip archive', 'Oct 21, 1983, 11:55 AM']
  ];
}

/**
 * Tests creating a zip file on Downloads.
 */
testcase.zipCreateFileDownloads = async function() {
  // Open Files app on Downloads containing ENTRIES.photos.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.photos], []);

  // Select the file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['photos']),
      'selectFile failed');

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Click the 'Zip selection' menu command.
  const zip = '[command="#zip-selection"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [zip]),
      'fakeMouseClick failed');

  // Check: a zip file should be created.
  const files = getZipSelectionFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Tests creating a zip file on Drive.
 */
testcase.zipCreateFileDrive = async function() {
  // Open Files app on Drive containing ENTRIES.photos.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DRIVE, null, [], [ENTRIES.photos]);

  // Select the file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['photos']),
      'selectFile failed');

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Click the 'Zip selection' menu command.
  const zip = '[command="#zip-selection"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [zip]),
      'fakeMouseClick failed');

  // Check: a zip file should be created.
  const files = getZipSelectionFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Tests creating a zip file on a removable USB volume.
 */
testcase.zipCreateFileUsb = async function() {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Open Files app on Drive.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DRIVE, null, [], [ENTRIES.beautiful]);

  // Mount empty USB volume in the Drive window.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [USB_VOLUME_QUERY]);

  // Add ENTRIES.photos to the USB volume.
  await addEntries(['usb'], [ENTRIES.photos]);

  // Verify the USB file list.
  const photos = [ENTRIES.photos.getExpectedRow()];
  await remoteCall.waitForFiles(appId, photos);

  // Select the photos file list entry.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['photos']),
      'selectFile failed');

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Check: the context menu should appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Click the 'Zip selection' menu command.
  const zip = '[command="#zip-selection"]';
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [zip]),
      'fakeMouseClick failed');

  // Check: a zip file should be created.
  const files = getZipSelectionFileListRowEntries();
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Tests zip file open (aka unzip) from Downloads.
 * The file names are encoded in SJIS.
 */
testcase.zipFileOpenDownloadsShiftJIS = async function() {
  // Open Files app on Downloads containing a zip file.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.zipArchiveSJIS], []);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['archive_sjis.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntriesSjisRoot();
  await remoteCall.waitForFiles(appId, files);

  // Select the directory in the ZIP file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['新しいフォルダ']),
      'selectFile failed');

  // Press the Enter key.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files2 = getUnzippedFileListRowEntriesSjisSubdir();
  await remoteCall.waitForFiles(appId, files2);
};

/**
 * Tests zip file open (aka unzip) from Downloads. The file name in the archive
 * is encoded in UTF-8, but the language encoding flag bit is set to 0.
 */
testcase.zipFileOpenDownloadsMacOs = async function() {
  // Open Files app on Downloads containing a zip file.
  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.zipArchiveMacOs], []);

  // Select the zip file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['archive_macos.zip']),
      'selectFile failed');

  // Press the Enter key.
  const key = ['#file-list', 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Check: the zip file content should be shown (unzip).
  const files = getUnzippedFileListRowEntriesMacOsRoot();
  await remoteCall.waitForFiles(appId, files);
};
