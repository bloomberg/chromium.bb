// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**  Test constructor and default public attributes. */
function testEntryList(testReportCallback) {
  const entryList = new EntryList('My files', 'my_files');
  assertEquals('My files', entryList.label);
  assertEquals('entry-list://my_files', entryList.toURL());
  assertEquals('my_files', entryList.rootType);
  assertFalse(entryList.isNativeType);
  assertEquals(0, entryList.children.length);
  assertTrue(entryList.isDirectory);
  assertFalse(entryList.isFile);

  entryList.addEntry(new EntryList('Child Entry', 'child_entry'));
  assertEquals(1, entryList.children.length);

  const reader = entryList.createReader();
  // How many times the reader callback |accumulateResults| has been called?
  let callCounter = 0;
  // How many times it was called with results?
  let resultCouter = 0;
  const accumulateResults = (readerResult) => {
    // It's called with readerResult==[] a last time to indicate no more files.
    callCounter++;
    if (readerResult.length > 0) {
      resultCouter++;
      reader.readEntries(accumulateResults);
    }
  };

  reader.readEntries(accumulateResults);
  // readEntries runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        // accumulateResults should be called 2x in normal conditions;
        return callCounter >= 2;
      }).then(() => {
        // Now we can check the final result.
        assertEquals(2, callCounter);
        assertEquals(1, resultCouter);
      }),
      testReportCallback);
}

/** Tests method EntryList.getParent. */
function testEntryListGetParent(testReportCallback) {
  const entryList = new EntryList('My files', 'my_files');
  let callbackTriggered = false;
  entryList.getParent(parentEntry => {
    // EntryList should return itself since it's a root and that's what the web
    // spec says.
    callbackTriggered = true;
    assertEquals(parentEntry, entryList);
  });
  reportPromise(waitUntil(() => callbackTriggered), testReportCallback);
}

/** Tests method EntryList.addEntry. */
function testEntryListAddEntry() {
  const entryList = new EntryList('My files');
  assertEquals(0, entryList.children.length);

  const fakeRootEntry = createFakeDisplayRoot();
  const fakeVolumeInfo = {
    displayRoot: fakeRootEntry,
    label: 'Fake Filesystem',
    volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
  };
  const childEntry = new VolumeEntry(fakeVolumeInfo);
  entryList.addEntry(childEntry);
  assertEquals(1, entryList.children.length);
  assertEquals(childEntry, entryList.children[0]);
}

/** Tests methods to remove entries. */
function testEntryListRemoveEntry() {
  const entryList = new EntryList('My files');

  const fakeRootEntry = createFakeDisplayRoot();
  const fakeVolumeInfo = {
    displayRoot: fakeRootEntry,
    label: 'Fake Filesystem',
  };
  const childEntry = new VolumeEntry(fakeVolumeInfo);
  entryList.addEntry(childEntry);
  assertTrue(entryList.removeEntry(childEntry));
  assertEquals(0, entryList.children.length);
}

/**
 * Tests methods findIndexByVolumeInfo, removeByVolumeType, removeByRootType.
 */
function testEntryFindIndex() {
  const entryList = new EntryList('My files');

  const fakeRootEntry = createFakeDisplayRoot();
  const downloadsVolumeInfo = {
    displayRoot: fakeRootEntry,
    label: 'Fake Filesystem',
    volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
  };
  const downloads = new VolumeEntry(downloadsVolumeInfo);

  const crostiniRootEntry = createFakeDisplayRoot();
  const crostiniVolumeInfo = {
    displayRoot: crostiniRootEntry,
    label: 'Fake Filesystem',
    volumeType: VolumeManagerCommon.VolumeType.CROSTINI,
  };
  const crostini = new VolumeEntry(crostiniVolumeInfo);

  const fakeEntry = {
    isDirectory: true,
    rootType: VolumeManagerCommon.RootType.CROSTINI,
    name: 'Linux files',
    toURL: function() {
      return 'fake-entry://linux-files';
    }
  };

  entryList.addEntry(downloads);
  entryList.addEntry(crostini);

  // Test findIndexByVolumeInfo.
  assertEquals(0, entryList.findIndexByVolumeInfo(downloadsVolumeInfo));
  assertEquals(1, entryList.findIndexByVolumeInfo(crostiniVolumeInfo));

  // Test removeByVolumeType.
  assertTrue(
      entryList.removeByVolumeType(VolumeManagerCommon.VolumeType.CROSTINI));
  assertEquals(1, entryList.children.length);
  // Now crostini volume doesn't exist anymore, so should return False.
  assertFalse(
      entryList.removeByVolumeType(VolumeManagerCommon.VolumeType.CROSTINI));

  // Test removeByRootType.
  entryList.addEntry(fakeEntry);
  assertTrue(entryList.removeByRootType(VolumeManagerCommon.RootType.CROSTINI));
  assertEquals(1, entryList.children.length);
}

/** Tests method EntryList.getMetadata. */
function testEntryListGetMetadata(testReportCallback) {
  const entryList = new EntryList('My files');

  let modificationTime = null;
  entryList.getMetadata(metadata => {
    modificationTime = metadata.modificationTime;
  });

  // getMetadata runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        return modificationTime !== null;
      }).then(() => {
        // Now we can check the final result, it returns "now", so let's just
        // check the type and 1 attribute here.
        assertTrue(modificationTime instanceof Date);
        assertTrue(!!modificationTime.getUTCFullYear());
      }),
      testReportCallback);
}

/** Tests StaticReader.readEntries. */
function testStaticReader(testReportCallback) {
  const reader = new StaticReader(['file1', 'file2']);
  const testResults = [];
  // How many times the reader callback |accumulateResults| has been called?
  let callCounter = 0;
  const accumulateResults = (readerResult) => {
    callCounter++;
    // merge on testResults.
    readerResult.map(f => testResults.push(f));
    if (readerResult.length > 0)
      reader.readEntries(accumulateResults);
  };

  reader.readEntries(accumulateResults);
  // readEntries runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        // accumulateResults should be called 2x in normal conditions;
        return callCounter >= 2;
      }).then(() => {
        // Now we can check the final result.
        assertEquals(2, callCounter);
        assertEquals(2, testResults.length);
        assertEquals('file1', testResults[0]);
        assertEquals('file2', testResults[1]);
      }),
      testReportCallback);
}

/**
 * Returns an object that can be used as displayRoot on a FakeVolumeInfo.
 * VolumeEntry delegates many attributes and methods to displayRoot.
 */
function createFakeDisplayRoot() {
  const fakeRootEntry = {
    filesystem: 'fake-filesystem://',
    fullPath: '/fake/full/path',
    isDirectory: true,
    isFile: false,
    name: 'fs-name',
    toURL: () => {
      return 'fake-filesystem://fake/full/path';
    },
    createReader: () => {
      return 'FAKE READER';
    },
    getMetadata: (success, error) => {
      // Returns static date as modificationTime for testing.
      setTimeout(
          () => success({modificationTime: new Date(Date.UTC(2018, 6, 27))}));
    },
  };
  return fakeRootEntry;
}

/**
 * Tests VolumeEntry constructor and default public attributes/getter/methods.
 */
function testVolumeEntry() {
  const fakeRootEntry = createFakeDisplayRoot();
  const fakeVolumeInfo = {
    displayRoot: fakeRootEntry,
    label: 'Fake Filesystem',
    volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
  };

  const volumeEntry = new VolumeEntry(fakeVolumeInfo);
  assertEquals(fakeRootEntry, volumeEntry.rootEntry);
  assertEquals(VolumeManagerCommon.VolumeType.DOWNLOADS, volumeEntry.iconName);
  assertEquals('fake-filesystem://', volumeEntry.filesystem);
  assertEquals('/fake/full/path', volumeEntry.fullPath);
  assertEquals('fake-filesystem://fake/full/path', volumeEntry.toURL());
  assertEquals('Fake Filesystem', volumeEntry.name);
  assertEquals('FAKE READER', volumeEntry.createReader());
  assertTrue(volumeEntry.isNativeType);
  assertTrue(volumeEntry.isDirectory);
  assertFalse(volumeEntry.isFile);
}

/**
 * Tests VolumeEntry which initially doesn't have displayRoot.
 */
function testVolumeEntryDelayedDisplayRoot(testReportCallback) {
  let callbackTriggered = false;
  const fakeRootEntry = createFakeDisplayRoot();
  // A VolumeInfo without displayRoot.
  const fakeVolumeInfo = {
    displayRoot: null,
    label: 'Fake Filesystem',
    resolveDisplayRoot: function(successCallback, errorCallback) {
      setTimeout(() => {
        successCallback(fakeRootEntry);
        callbackTriggered = true;
      }, 0);
    },
  };

  const volumeEntry = new VolumeEntry(fakeVolumeInfo);
  // rootEntry starts as null.
  assertEquals(null, volumeEntry.rootEntry);
  reportPromise(
      waitUntil(() => callbackTriggered).then(() => {
        // Eventually rootEntry gets the value.
        assertEquals(fakeRootEntry, volumeEntry.rootEntry);
      }),
      testReportCallback);
}
/** Tests VolumeEntry.getParent */
function testVolumeEntryGetParent(testReportCallback) {
  const fakeRootEntry = createFakeDisplayRoot();
  const fakeVolumeInfo = {
    displayRoot: fakeRootEntry,
    label: 'Fake Filesystem',
  };

  const volumeEntry = new VolumeEntry(fakeVolumeInfo);
  let callbackTriggered = false;
  volumeEntry.getParent(parentEntry => {
    callbackTriggered = true;
    // VolumeEntry should return itself since it's a root and that's what the
    // web spec says.
    assertEquals(parentEntry, volumeEntry);
  });
  reportPromise(waitUntil(() => callbackTriggered), testReportCallback);
}

/**  Tests VolumeEntry.getMetadata */
function testVolumeEntryGetMetadata(testReportCallback) {
  const fakeRootEntry = createFakeDisplayRoot();
  const fakeVolumeInfo = {
    displayRoot: fakeRootEntry,
    label: 'Fake Filesystem',
  };
  const volumeEntry = new VolumeEntry(fakeVolumeInfo);

  let modificationTime = null;
  volumeEntry.getMetadata(metadata => {
    modificationTime = metadata.modificationTime;
  });

  // getMetadata runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        return modificationTime !== null;
      }).then(() => {
        // Now we can check the final result.
        assertEquals(2018, modificationTime.getUTCFullYear());
        // Date() month is 0-based, so 6 == July. :-(
        assertEquals(6, modificationTime.getUTCMonth());
        assertEquals(27, modificationTime.getUTCDate());
      }),
      testReportCallback);
}

/**
 * Test EntryList.addEntry sets prefix on VolumeEntry.
 */
function testEntryListAddEntrySetsPrefix() {
  const fakeRootEntry = createFakeDisplayRoot();
  const fakeVolumeInfo = {
    displayRoot: fakeRootEntry,
    label: 'Fake Filesystem',
  };
  const volumeEntry = new VolumeEntry(fakeVolumeInfo);
  const entryList = new EntryList('My files', 'my_files');

  entryList.addEntry(volumeEntry);
  assertEquals(1, entryList.children.length);
  // entryList is parent of volumeEntry so it should be its prefix.
  assertEquals(entryList, volumeEntry.volumeInfo.prefixEntry);
}
