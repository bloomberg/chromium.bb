// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Checks if the files initially added by the C++ side are displayed, and
 * that files subsequently added are also displayed.
 *
 * @param {string} path Path to be tested, Downloads or Drive.
 * @param {Array<TestEntryInfo>} defaultEntries Default file entries.
 */
function fileDisplay(path, defaultEntries) {
  var appId;

  const defaultList = TestEntryInfo.getExpectedRows(defaultEntries).sort();

  StepsRunner.run([
    // Open Files app on the given |path| with default file entries.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Verify the default file list.
    function(result) {
      appId = result.windowId;
      chrome.test.assertEq(defaultList, result.fileList);
      this.next();
    },
    // Add new file entries.
    function() {
      addEntries(['local', 'drive'], [ENTRIES.newlyAdded], this.next);
    },
    // Wait for the new file entries.
    function() {
      remoteCall.waitForFileListChange(appId, defaultList.length)
          .then(this.next);
    },
    // Verify the new file list.
    function(fileList) {
      const expectedList =
          defaultList.concat([ENTRIES.newlyAdded.getExpectedRow()]).sort();
      chrome.test.assertEq(expectedList, fileList);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests files display in Downloads.
 */
testcase.fileDisplayDownloads = function() {
  fileDisplay(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);
};

/**
 * Tests files display in Google Drive.
 */
testcase.fileDisplayDrive = function() {
  fileDisplay(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET);
};

/**
 * Tests file display rendering in offline Google Drive.
 */
testcase.fileDisplayDriveOffline = function() {
  var appId;

  const driveFiles = [ENTRIES.hello, ENTRIES.pinned, ENTRIES.photos];

  StepsRunner.run([
    // Open Files app on Drive with the given test files.
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next, [], driveFiles);
    },
    // Retrieve all file list entries that could be rendered 'offline'.
    function(result) {
      appId = result.windowId;
      const offlineEntry = '#file-list .table-row.file.dim-offline';
      remoteCall.callRemoteTestUtil(
          'queryAllElements', appId, [offlineEntry, ['opacity']], this.next);
    },
    // Check: the hello.txt file only should be rendered 'offline'.
    function(elements) {
      chrome.test.assertEq(1, elements.length);
      chrome.test.assertEq(0, elements[0].text.indexOf('hello.txt'));
      this.next(elements[0].styles);
    },
    // Check: hello.txt must have 'offline' CSS render style (opacity).
    function(style) {
      chrome.test.assertEq('0.4', style.opacity);
      this.next();
    },
    // Retrieve file entries that are 'available offline' (not dimmed).
    function() {
      const availableEntry = '#file-list .table-row:not(.dim-offline)';
      remoteCall.callRemoteTestUtil(
          'queryAllElements', appId, [availableEntry, ['opacity']], this.next);
    },
    // Check: these files should have 'available offline' CSS style.
    function(elements) {
      chrome.test.assertEq(2, elements.length);

      function checkRenderedInAvailableOfflineStyle(element, fileName) {
        chrome.test.assertEq(0, element.text.indexOf(fileName));
        chrome.test.assertEq('1', element.styles.opacity);
      }

      // Directories are shown as 'available offline'.
      checkRenderedInAvailableOfflineStyle(elements[0], 'photos');

      // Pinned files are shown as 'available offline'.
      checkRenderedInAvailableOfflineStyle(elements[1], 'pinned');

      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests file display rendering in online Google Drive.
 */
testcase.fileDisplayDriveOnline = function() {
  var appId;

  StepsRunner.run([
    // Open Files app on Drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], BASIC_DRIVE_ENTRY_SET);
    },
    // Retrieve all file list row entries.
    function(result) {
      appId = result.windowId;
      const fileEntry = '#file-list .table-row';
      remoteCall.callRemoteTestUtil(
          'queryAllElements', appId, [fileEntry, ['opacity']], this.next);
    },
    // Check: all files must have 'online' CSS style (not dimmed).
    function(elements) {
      chrome.test.assertEq(BASIC_DRIVE_ENTRY_SET.length, elements.length);
      for (let i = 0; i < elements.length; ++i)
        chrome.test.assertEq('1', elements[i].styles.opacity);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests files display in an MTP volume.
 */
testcase.fileDisplayMtp = function() {
  var appId;

  const MTP_VOLUME_QUERY = '#directory-tree [volume-type-icon="mtp"]';

  StepsRunner.run([
    // Open Files app on local downloads.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Mount MTP volume in the Downloads window.
    function(results) {
      appId = results.windowId;
      chrome.test.sendMessage(JSON.stringify({name: 'mountFakeMtp'}),
                              this.next);
    },
    // Wait for the MTP mount.
    function() {
      remoteCall.waitForElement(appId, MTP_VOLUME_QUERY).then(this.next);
    },
    // Click to open the MTP volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [MTP_VOLUME_QUERY], this.next);
    },
    // Verify the MTP file list.
    function() {
      const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests files display in a removable USB volume.
 */
testcase.fileDisplayUsb = function() {
  var appId;

  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  StepsRunner.run([
    // Open Files app on local downloads.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Mount USB volume in the Downloads window.
    function(results) {
      appId = results.windowId;
      chrome.test.sendMessage(
          JSON.stringify({name: 'mountFakeUsb'}), this.next);
    },
    // Wait for the USB mount.
    function() {
      remoteCall.waitForElement(appId, USB_VOLUME_QUERY).then(this.next);
    },
    // Click to open the USB volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [USB_VOLUME_QUERY], this.next);
    },
    // Verify the USB file list.
    function() {
      const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Searches for a string in Downloads and checks that the correct results
 * are displayed.
 *
 * @param {string} searchTerm The string to search for.
 * @param {Array<Object>} expectedResults The results set.
 *
 */
function searchDownloads(searchTerm, expectedResults) {
  var appId;

  StepsRunner.run([
    // Open Files app on local downloads.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Focus the search box.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'fakeEvent', appId, ['#search-box cr-input', 'focus'], this.next);
    },
    // Input a text.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil(
          'inputText', appId, ['#search-box cr-input', searchTerm], this.next);
    },
    // Notify the element of the input.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeEvent', appId, ['#search-box cr-input', 'input'], this.next);
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

/**
 * Tests case-senstive search for an entry in Downloads.
 */
testcase.fileSearch = function() {
  searchDownloads('hello', [ENTRIES.hello]);
};

/**
 * Tests case-insenstive search for an entry in Downloads.
 */
testcase.fileSearchCaseInsensitive = function() {
  searchDownloads('HELLO', [ENTRIES.hello]);
};

/**
 * Tests searching for a string doesn't match anything in Downloads and that
 * there are no displayed items that match the search string.
 */
testcase.fileSearchNotFound = function() {
  var appId;
  var searchTerm = 'blahblah';

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Focus the search box.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'fakeEvent', appId, ['#search-box cr-input', 'focus'], this.next);
    },
    // Input a text.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil(
          'inputText', appId, ['#search-box cr-input', searchTerm], this.next);
    },
    // Notify the element of the input.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeEvent', appId, ['#search-box cr-input', 'input'], this.next);
    },
    function(result) {
      remoteCall.waitForElement(appId, ['#empty-folder-label b']).
          then(this.next);
    },
    function(element) {
      chrome.test.assertEq(element.text, '\"' + searchTerm + '\"');
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
