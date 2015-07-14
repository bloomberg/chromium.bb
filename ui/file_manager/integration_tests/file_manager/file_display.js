// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests if the files initially added by the C++ side are displayed, and
 * that a subsequently added file shows up.
 *
 * @param {string} path Directory path to be tested.
 */
function fileDisplay(path) {
  var appId;

  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == RootPath.DRIVE ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET).sort();

  var expectedFilesAfter =
      expectedFilesBefore.concat([ENTRIES.newlyAdded.getExpectedRow()]).sort();

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Notify that the list has been verified and a new file can be added
    // in file_manager_browsertest.cc.
    function(results) {
      appId = results.windowId;
      var actualFilesBefore = results.fileList;
      chrome.test.assertEq(expectedFilesBefore, actualFilesBefore);
      addEntries(['local', 'drive'], [ENTRIES.newlyAdded], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFileListChange(appId, expectedFilesBefore.length).
          then(this.next);
    },
    // Confirm the file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(expectedFilesAfter, actualFilesAfter);
      checkIfNoErrorsOccured(this.next);
    },
  ]);
}

testcase.fileDisplayDownloads = function() {
  fileDisplay(RootPath.DOWNLOADS);
};

testcase.fileDisplayDrive = function() {
  fileDisplay(RootPath.DRIVE);
};

testcase.fileDisplayMtp = function() {
  var appId;
  var MTP_VOLUME_QUERY = '#directory-tree > .tree-item > .tree-row > ' +
    '.item-icon[volume-type-icon="mtp"]';

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Mount a fake MTP volume.
    function(results) {
      appId = results.windowId;
      chrome.test.sendMessage(JSON.stringify({name: 'mountFakeMtp'}),
                              this.next);
    },
    // Wait for the mount.
    function(result) {
      remoteCall.waitForElement(appId, MTP_VOLUME_QUERY).then(this.next);
    },
    // Click the MTP volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [MTP_VOLUME_QUERY], this.next);
    },
    // Wait for the file list to change.
    function(appIds) {
      remoteCall.waitForFiles(
          appId,
          TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET),
          {ignoreLastModifiedTime: true}).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Searches for a string in Downloads and checks the correct results are
 * being displayed.
 *
 * @param {string} searchTerm The string to search for.
 * @param {Array<Object>} expectedResults The results set.
 *
 */
function searchDownloads(searchTerm, expectedResults) {
  var appId;

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Focus the search box.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil('fakeEvent',
                                    appId,
                                    ['#search-box input', 'focus'],
                                    this.next);
    },
    // Input a text.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil('inputText',
                                    appId,
                                    ['#search-box input', searchTerm],
                                    this.next);
    },
    // Notify the element of the input.
    function() {
      remoteCall.callRemoteTestUtil('fakeEvent',
                                    appId,
                                    ['#search-box input', 'input'],
                                    this.next);
    },
    function(result) {
      remoteCall.waitForFileListChange(appId, BASIC_LOCAL_ENTRY_SET.length).
      then(this.next);
    },
    function(actualFilesAfter) {
      chrome.test.assertEq(
          TestEntryInfo.getExpectedRows(expectedResults).sort(),
          actualFilesAfter);

      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

testcase.searchNormal = function() {
  searchDownloads('hello', [ENTRIES.hello]);
};

testcase.searchCaseInsensitive = function() {
  searchDownloads('HELLO', [ENTRIES.hello]);
};

/**
 * Searches for a string that doesn't match anything in Downloads
 * and checks that the no items match string is displayed.
 *
 */
testcase.searchNotFound = function() {
  var appId;
  var searchTerm = 'blahblah';

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Focus the search box.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil('fakeEvent',
                                    appId,
                                    ['#search-box input', 'focus'],
                                    this.next);
    },
    // Input a text.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil('inputText',
                                    appId,
                                    ['#search-box input', searchTerm],
                                    this.next);
    },
    // Notify the element of the input.
    function() {
      remoteCall.callRemoteTestUtil('fakeEvent',
                                    appId,
                                    ['#search-box input', 'input'],
                                    this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId, ['#no-search-results b']).
          then(this.next);
    },
    function(element) {
      chrome.test.assertEq(element.text, '\"' + searchTerm + '\"');
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
