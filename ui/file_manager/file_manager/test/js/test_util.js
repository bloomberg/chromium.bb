// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Stores Blobs loaded from src/chrome/test/data/chromeos/file_manager.
var DATA = {
  'archive.zip': null,
  'image.png': null,
  'image2.png': null,
  'image3.jpg': null,
  'music.ogg': null,
  'random.bin': null,
  'text.txt': null,
  'video.ogv': null,
};

// Load DATA from local filesystem.
function loadData() {
  return Promise.all(Object.keys(DATA).map(filename => {
    return new Promise(resolve => {
      var req = new XMLHttpRequest();
      req.responseType = 'blob';
      req.onload = () => {
        DATA[filename] = req.response;
        resolve();
      };
      req.open(
          'GET', '../../../chrome/test/data/chromeos/file_manager/' + filename);
      req.send();
    });
  }));
}

/**
 * @enum {string}
 * @const
 */
var EntryType = Object.freeze({
  FILE: 'file',
  DIRECTORY: 'directory'
});

/**
 * @enum {string}
 * @const
 */
var SharedOption = Object.freeze({
  NONE: 'none',
  SHARED: 'shared'
});

/**
 * File system entry information for tests.
 *
 * @param {EntryType} type Entry type.
 * @param {string} sourceFileName Source file name that provides file contents.
 * @param {string} targetName Name of entry on the test file system.
 * @param {string} mimeType Mime type.
 * @param {SharedOption} sharedOption Shared option.
 * @param {string} lastModifiedTime Last modified time as a text to be shown in
 *     the last modified column.
 * @param {string} nameText File name to be shown in the name column.
 * @param {string} sizeText Size text to be shown in the size column.
 * @param {string} typeText Type name to be shown in the type column.
 * @constructor
 */
function TestEntryInfo(type,
                       sourceFileName,
                       targetPath,
                       mimeType,
                       sharedOption,
                       lastModifiedTime,
                       nameText,
                       sizeText,
                       typeText) {
  this.type = type;
  this.sourceFileName = sourceFileName || '';
  this.targetPath = targetPath;
  this.mimeType = mimeType || '';
  this.sharedOption = sharedOption;
  this.lastModifiedTime = lastModifiedTime;
  this.nameText = nameText;
  this.sizeText = sizeText;
  this.typeText = typeText;
  Object.freeze(this);
}

TestEntryInfo.getExpectedRows = function(entries) {
  return entries.map(function(entry) { return entry.getExpectedRow(); });
};

/**
 * Returns 4-typle name, size, type, date as shown in file list.
 */
TestEntryInfo.prototype.getExpectedRow = function() {
  return [this.nameText, this.sizeText, this.typeText, this.lastModifiedTime];
};

TestEntryInfo.getMockFileSystemPopulateRows = function(entries, prefix) {
  return entries.map(function(entry) {
    return entry.getMockFileSystemPopulateRow(prefix);
  });
};

/**
 * Returns object {fullPath: ..., metadata: {...}, content: ...} as used in
 * MockFileSystem.populate.
 */
TestEntryInfo.prototype.getMockFileSystemPopulateRow = function(prefix) {
  var suffix = this.type == EntryType.DIRECTORY ? '/' : '';
  var content = DATA[this.sourceFileName];
  var size = content && content.size || 0;
  return {
    fullPath: prefix + this.nameText + suffix,
    metadata: {
      size: size,
      modificationTime: new Date(Date.parse(this.lastModifiedTime)),
      contentMimeType: this.mimeType,
    },
    content: content
  };
};

/**
 * Filesystem entries used by the test cases.
 * @type {Object<TestEntryInfo>}
 * @const
 */
var ENTRIES = {
  hello: new TestEntryInfo(
      EntryType.FILE, 'text.txt', 'hello.txt',
      'text/plain', SharedOption.NONE, 'Sep 4, 1998, 12:34 PM',
      'hello.txt', '51 bytes', 'Plain text'),

  world: new TestEntryInfo(
      EntryType.FILE, 'video.ogv', 'world.ogv',
      'video/ogg', SharedOption.NONE, 'Jul 4, 2012, 10:35 AM',
      'world.ogv', '59 KB', 'OGG video'),

  unsupported: new TestEntryInfo(
      EntryType.FILE, 'random.bin', 'unsupported.foo',
      'application/x-foo', SharedOption.NONE, 'Jul 4, 2012, 10:36 AM',
      'unsupported.foo', '8 KB', 'FOO file'),

  desktop: new TestEntryInfo(
      EntryType.FILE, 'image.png', 'My Desktop Background.png',
      'image/png', SharedOption.NONE, 'Jan 18, 2038, 1:02 AM',
      'My Desktop Background.png', '272 bytes', 'PNG image'),

  // An image file without an extension, to confirm that file type detection
  // using mime types works fine.
  image2: new TestEntryInfo(
      EntryType.FILE, 'image2.png', 'image2',
      'image/png', SharedOption.NONE, 'Jan 18, 2038, 1:02 AM',
      'image2', '4 KB', 'PNG image'),

  image3: new TestEntryInfo(
      EntryType.FILE, 'image3.jpg', 'image3.jpg',
      'image/jpeg', SharedOption.NONE, 'Jan 18, 2038, 1:02 AM',
      'image3.jpg', '3 KB', 'JPEG image'),

  // An ogg file without a mime type, to confirm that file type detection using
  // file extensions works fine.
  beautiful: new TestEntryInfo(
      EntryType.FILE, 'music.ogg', 'Beautiful Song.ogg',
      null, SharedOption.NONE, 'Nov 12, 2086, 12:00 PM',
      'Beautiful Song.ogg', '14 KB', 'OGG audio'),

  photos: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'photos',
      null, SharedOption.NONE, 'Jan 1, 1980, 11:59 PM',
      'photos', '--', 'Folder'),

  testDocument: new TestEntryInfo(
      EntryType.FILE, null, 'Test Document',
      'application/vnd.google-apps.document',
      SharedOption.NONE, 'Apr 10, 2013, 4:20 PM',
      'Test Document.gdoc', '--', 'Google document'),

  testSharedDocument: new TestEntryInfo(
      EntryType.FILE, null, 'Test Shared Document',
      'application/vnd.google-apps.document',
      SharedOption.SHARED, 'Mar 20, 2013, 10:40 PM',
      'Test Shared Document.gdoc', '--', 'Google document'),

  newlyAdded: new TestEntryInfo(
      EntryType.FILE, 'music.ogg', 'newly added file.ogg',
      'audio/ogg', SharedOption.NONE, 'Sep 4, 1998, 12:00 AM',
      'newly added file.ogg', '14 KB', 'OGG audio'),

  directoryA: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'A', '--', 'Folder'),

  directoryB: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A/B',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'B', '--', 'Folder'),

  directoryC: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A/B/C',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'C', '--', 'Folder'),

  directoryD: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'D',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'D', '--', 'Folder'),

  directoryE: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'D/E',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'E', '--', 'Folder'),

  directoryF: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'D/E/F',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'F', '--', 'Folder'),

  zipArchive: new TestEntryInfo(
      EntryType.FILE, 'archive.zip', 'archive.zip',
      'application/x-zip', SharedOption.NONE, 'Jan 1, 2014, 1:00 AM',
      'archive.zip', '533 bytes', 'Zip archive'),

  hiddenFile: new TestEntryInfo(
    EntryType.FILE, 'text.txt', '.hiddenfile.txt',
    'text/plain', SharedOption.NONE, 'Sep 30, 2014, 3:30 PM',
    '.hiddenfile.txt', '51 bytes', 'Plain text')
};

/**
 * Basic entry set for the local volume.
 * @type {Array<TestEntryInfo>}
 * @const
 */
var BASIC_LOCAL_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos
];

/**
 * Basic entry set for the drive volume.
 *
 * TODO(hirono): Add a case for an entry cached by FileCache. For testing
 *               Drive, create more entries with Drive specific attributes.
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
var BASIC_DRIVE_ENTRY_SET = [
  ENTRIES.hello,
  ENTRIES.world,
  ENTRIES.desktop,
  ENTRIES.beautiful,
  ENTRIES.photos,
  ENTRIES.unsupported,
  ENTRIES.testDocument,
  ENTRIES.testSharedDocument
];

var INTERVAL_FOR_WAIT_UNTIL = 100;  // ms
var INTERVAL_FOR_WAIT_LOGGING = 10000; // 10s

/**
 * Waits until testFunction becomes true.
 * @param {function(): Object} testFunction A function which is tested.
 * @return {!Promise} A promise which is fullfilled when the testFunction
 *     becomes true.
 */
function waitUntil(testFunction) {
  return new Promise(function(resolve) {
    var tryTestFunction = function() {
      var result = testFunction();
      if (result) {
        resolve(result);
      } else {
        setTimeout(tryTestFunction, INTERVAL_FOR_WAIT_UNTIL);
      }
    };

    tryTestFunction();
  });
}

/**
 * Waits until specified element exists.
 */
function waitForElement(selectors) {
  return waitUntil(() => {
    return document.querySelector(selectors);
  });
}

/**
 * Adds specified TestEntryInfos to downloads and drive.
 *
 * @param {!Array<TestEntryInfo} downloads Entries for downloads.
 * @param {!Array<TestEntryInfo} drive Entries for drive.
 */
function addEntries(downloads, drive) {
  var fsDownloads =
      mockVolumeManager
          .getCurrentProfileVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS)
          .fileSystem;
  fsDownloads.populate(
      TestEntryInfo.getMockFileSystemPopulateRows(downloads, '/'), true);

  var fsDrive =
      mockVolumeManager
          .getCurrentProfileVolumeInfo(VolumeManagerCommon.VolumeType.DRIVE)
          .fileSystem;
  fsDrive.populate(
      TestEntryInfo.getMockFileSystemPopulateRows(drive, '/root/'), true);

  // Send onDirectoryChanged events.
  chrome.fileManagerPrivate.dispatchEvent_(
      'onDirectoryChanged', {eventType: 'changed', entry: fsDownloads.root});
  chrome.fileManagerPrivate.dispatchEvent_(
      'onDirectoryChanged',
      {eventType: 'changed', entry: fsDrive.entries['/root']});
}

/**
 * Waits for the file list turns to the given contents.
 * @param {Array<Array<string>>} expected Expected contents of file list.
 * @param {{orderCheck:boolean=, ignoreName:boolean=, ignoreSize:boolean=,
 *     ignoreType:boolean=,ignoreDate:boolean=}=} opt_options
 *     Options of the comparison. If orderCheck is true, it also compares the
 *     order of files. If ignore[Name|Size|Type|Date] is true, it compares
 *     the file without considering that field.
 * @return {Promise} Promise to be fulfilled when the file list turns to the
 *     given contents.
 */
function waitForFiles(expected, opt_options) {
  var options = opt_options || {};
  var nextLog = Date.now() + INTERVAL_FOR_WAIT_LOGGING;
  return waitUntil(function() {
    var files = test.util.sync.getFileList(window);
    if (Date.now() > nextLog) {
      console.debug('waitForFiles', expected, files);
      nextLog = Date.now() + INTERVAL_FOR_WAIT_LOGGING;
    }
    if (!options.orderCheck) {
      files.sort();
      expected.sort();
    }
    if (files.length != expected.length)
      return false;
    for (var i = 0; i < files.length; i++) {
      // Each row is [name, size, type, date].
      if ((!options.ignoreName && files[i][0] != expected[i][0]) ||
          (!options.ignoreSize && files[i][1] != expected[i][1]) ||
          (!options.ignoreType && files[i][2] != expected[i][2]) ||
          (!options.ignoreDate && files[i][3] != expected[i][3]))
        return false;
    }
    return true;
  });
}

/**
 * Opens a Files app's main window and waits until it is initialized. Fills
 * the window with initial files. Should be called for the first window only.
 *
 * @return {Promise} Promise to be fulfilled with the result object, which
 *     contains the file list.
 */
function setupAndWaitUntilReady() {
  return loadData()
      .then(result => {
        addEntries(BASIC_LOCAL_ENTRY_SET, BASIC_DRIVE_ENTRY_SET);
        return waitForElement('#directory-tree [volume-type-icon="downloads"]');
      })
      .then(() => {
        return test.util.sync.fakeMouseClick(
            window, '#directory-tree [volume-type-icon="downloads"]');
      })
      .then(result => {
        assertTrue(result);
        return waitForFiles(
            TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));
      });
}

// Shortcut for endTests with success.
function doneTests(opt_failed) {
  endTests(!opt_failed);
}
