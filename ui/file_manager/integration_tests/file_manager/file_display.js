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
async function fileDisplay(path, defaultEntries) {
  const defaultList = TestEntryInfo.getExpectedRows(defaultEntries).sort();

  // Open Files app on the given |path| with default file entries.
  const appId = await setupAndWaitUntilReady(path);

  // Verify the default file list is present in |result|.
  await remoteCall.waitForFiles(appId, defaultList);

  // Add new file entries.
  await addEntries(['local', 'drive'], [ENTRIES.newlyAdded]);

  // Verify the newly added entries appear in the file list.
  const expectedList =
      defaultList.concat([ENTRIES.newlyAdded.getExpectedRow()]);
  await remoteCall.waitForFiles(appId, expectedList);
}

/**
 * Tests files display in Downloads.
 */
testcase.fileDisplayDownloads = function() {
  return fileDisplay(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);
};

/**
 * Tests files display in Google Drive.
 */
testcase.fileDisplayDrive = function() {
  return fileDisplay(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET);
};

/**
 * Tests file display rendering in offline Google Drive.
 */
testcase.fileDisplayDriveOffline = async function() {
  const driveFiles =
      [ENTRIES.hello, ENTRIES.pinned, ENTRIES.photos, ENTRIES.testDocument];

  // Open Files app on Drive with the given test files.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, [], driveFiles);

  // Retrieve all file list entries that could be rendered 'offline'.
  const offlineEntry = '#file-list .table-row.file.dim-offline';
  let elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, [offlineEntry, ['opacity']]);

  // Check: the hello.txt file only should be rendered 'offline'.
  chrome.test.assertEq(1, elements.length);
  chrome.test.assertEq(0, elements[0].text.indexOf('hello.txt'));

  // Check: hello.txt must have 'offline' CSS render style (opacity).
  chrome.test.assertEq('0.4', elements[0].styles.opacity);

  // Retrieve file entries that are 'available offline' (not dimmed).
  const availableEntry = '#file-list .table-row:not(.dim-offline)';
  elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, [availableEntry, ['opacity']]);

  // Check: these files should have 'available offline' CSS style.
  chrome.test.assertEq(3, elements.length);

  function checkRenderedInAvailableOfflineStyle(element, fileName) {
    chrome.test.assertEq(0, element.text.indexOf(fileName));
    chrome.test.assertEq('1', element.styles.opacity);
  }

  // Directories are shown as 'available offline'.
  checkRenderedInAvailableOfflineStyle(elements[0], 'photos');

  // Hosted documents are shown as 'available offline'.
  checkRenderedInAvailableOfflineStyle(elements[1], 'Test Document.gdoc');

  // Pinned files are shown as 'available offline'.
  checkRenderedInAvailableOfflineStyle(elements[2], 'pinned');
};

/**
 * Tests file display rendering in online Google Drive.
 */
testcase.fileDisplayDriveOnline = async function() {
  // Open Files app on Drive.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET);

  // Retrieve all file list row entries.
  const fileEntry = '#file-list .table-row';
  const elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, [fileEntry, ['opacity']]);

  // Check: all files must have 'online' CSS style (not dimmed).
  chrome.test.assertEq(BASIC_DRIVE_ENTRY_SET.length, elements.length);
  for (let i = 0; i < elements.length; ++i) {
    chrome.test.assertEq('1', elements[i].styles.opacity);
  }
};

/**
 * Tests files display in the "Computers" section of Google Drive. Testing that
 * we can navigate to folders inside /Computers also has the side effect of
 * testing that the breadcrumbs are working.
 */
testcase.fileDisplayComputers = async function() {
  // Open Files app on Drive with Computers registered.
  const appId =
      await setupAndWaitUntilReady(RootPath.DRIVE, [], COMPUTERS_ENTRY_SET);

  // Navigate to Comuter Grand Root.
  await remoteCall.navigateWithDirectoryTree(
      appId, '/Computers', 'Computers', 'drive');

  // Navigiate to a Computer Root.
  await remoteCall.navigateWithDirectoryTree(
      appId, '/Computers/Computer A', 'Computers', 'drive');

  // Navigiate to a subdirectory under a Computer Root.
  await remoteCall.navigateWithDirectoryTree(
      appId, '/Computers/Computer A/A', 'Computers', 'drive');
};


/**
 * Tests files display in an MTP volume.
 */
testcase.fileDisplayMtp = async function() {
  const MTP_VOLUME_QUERY = '#directory-tree [volume-type-icon="mtp"]';

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount MTP volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeMtp'});

  // Wait for the MTP mount.
  await remoteCall.waitForElement(appId, MTP_VOLUME_QUERY);

  // Click to open the MTP volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [MTP_VOLUME_QUERY]);

  // Verify the MTP file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Tests files display in a removable USB volume.
 */
testcase.fileDisplayUsb = async function() {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount USB volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Click to open the USB volume.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [USB_VOLUME_QUERY]);

  // Verify the USB file list.
  const files = TestEntryInfo.getExpectedRows(BASIC_FAKE_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Tests files display for partitions on a removable USB volume.
 */
testcase.fileDisplayUsbPartition = async function() {
  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount USB volume.
  await sendTestMessage({name: 'mountFakePartitions'});

  // Wait for removable partition-1 to appear in the directory tree.
  const partitionOne = await remoteCall.waitForElement(
      appId, '#directory-tree [entry-label="partition-1"]');
  chrome.test.assertEq(
      'removable', partitionOne.attributes['volume-type-for-testing']);

  // Wait for removable partition-2 to appear in the directory tree.
  const partitionTwo = await remoteCall.waitForElement(
      appId, '#directory-tree [entry-label="partition-2"]');
  chrome.test.assertEq(
      'removable', partitionTwo.attributes['volume-type-for-testing']);

  // Wait for removable volume to appear in the directory tree.
  const singleUSB = await remoteCall.waitForElement(
      appId, '#directory-tree [entry-label="singleUSB"]');
  chrome.test.assertEq(
      'removable', singleUSB.attributes['volume-type-for-testing']);

  // Check whether the drive label is shared by the partitions.
  chrome.test.assertEq(
      'PARTITION_DRIVE_LABEL', partitionOne.attributes['drive-label']);
  chrome.test.assertEq(
      'PARTITION_DRIVE_LABEL', partitionTwo.attributes['drive-label']);
  chrome.test.assertEq(
      'SINGLE_DRIVE_LABEL', singleUSB.attributes['drive-label']);
};

/**
 * Searches for a string in Downloads and checks that the correct results
 * are displayed.
 *
 * @param {string} searchTerm The string to search for.
 * @param {Array<Object>} expectedResults The results set.
 *
 */
async function searchDownloads(searchTerm, expectedResults) {
  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));

  // Input a text.
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, ['#search-box cr-input', searchTerm]);

  // Notify the element of the input.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'input']));


  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(expectedResults));
}

/**
 * Tests case-senstive search for an entry in Downloads.
 */
testcase.fileSearch = function() {
  return searchDownloads('hello', [ENTRIES.hello]);
};

/**
 * Tests case-insenstive search for an entry in Downloads.
 */
testcase.fileSearchCaseInsensitive = function() {
  return searchDownloads('HELLO', [ENTRIES.hello]);
};

/**
 * Tests searching for a string doesn't match anything in Downloads and that
 * there are no displayed items that match the search string.
 */
testcase.fileSearchNotFound = async function() {
  var searchTerm = 'blahblah';

  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));

  // Input a text.
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, ['#search-box cr-input', searchTerm]);

  // Notify the element of the input.
  await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'input']);
  const element =
      await remoteCall.waitForElement(appId, ['#empty-folder-label b']);
  chrome.test.assertEq(element.text, '\"' + searchTerm + '\"');
};

/**
 * Tests Files app opening without errors when there isn't Downloads which is
 * the default volume.
 */
testcase.fileDisplayWithoutDownloadsVolume = async function() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Mount Drive.
  await sendTestMessage({name: 'mountDrive'});

  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Open Files app without specifying the initial directory/root.
  const appId = await openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
};

/**
 * Tests Files app opening without errors when there are no volumes at all.
 */
testcase.fileDisplayWithoutVolumes = async function() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Open Files app without specifying the initial directory/root.
  const appId = await openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);
};

/**
 * Tests Files app opening without errors when there are no volumes at all and
 * then mounting Downloads volume which should appear and be able to display its
 * files.
 */
testcase.fileDisplayWithoutVolumesThenMountDownloads = async function() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Open Files app without specifying the initial directory/root.
  const appId = await openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Mount Downloads.
  await sendTestMessage({name: 'mountDownloads'});

  // Wait until Downloads is mounted.
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Downloads should appear in My files in the directory tree.
  await remoteCall.waitForElement(appId, '[volume-type-icon="downloads"]');
  const downloadsRow = ['Downloads', '--', 'Folder'];
  const crostiniRow = ['Linux files', '--', 'Folder'];
  await remoteCall.waitForFiles(
      appId, [downloadsRow, crostiniRow],
      {ignoreFileSize: true, ignoreLastModifiedTime: true});
};

/**
 * Tests Files app opening without errors when there are no volumes at all and
 * then mounting Drive volume which should appear and be able to display its
 * files.
 */
testcase.fileDisplayWithoutVolumesThenMountDrive = async function() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Open Files app without specifying the initial directory/root.
  const appId = await openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Navigate to the Drive FakeItem.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[root-type-icon=\'drive\']']);

  // The fake Google Drive should be empty.
  await remoteCall.waitForFiles(appId, []);

  // Remount Drive. The curent directory should be changed from the Google
  // Drive FakeItem to My Drive.
  await sendTestMessage({name: 'mountDrive'});

  // Wait until Drive is mounted.
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Add an entry to Drive.
  await addEntries(['drive'], [ENTRIES.newlyAdded]);

  // Wait for "My Drive" files to display in the file list.
  await remoteCall.waitForFiles(appId, [ENTRIES.newlyAdded.getExpectedRow()]);
};

/**
 * Tests Files app opening without Drive mounted.
 */
testcase.fileDisplayWithoutDrive = async function() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Mount Downloads.
  await sendTestMessage({name: 'mountDownloads'});

  // Wait until downloads is re-added
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Open the files app.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.newlyAdded], []);

  // Wait for the loading indicator blink to finish.
  await remoteCall.waitForElement(
      appId, '#list-container paper-progress[hidden]');

  // Navigate to the fake Google Drive.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[root-type-icon=\'drive\']']);
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Google Drive');

  // The fake Google Drive should be empty.
  await remoteCall.waitForFiles(appId, []);

  // The loading indicator should be visible and remain visible forever.
  await remoteCall.waitForElement(
      appId, '#list-container paper-progress:not([hidden])');
};

/**
 * Tests Files app opening without Drive mounted and then disabling and
 * re-enabling Drive.
 */
testcase.fileDisplayWithoutDriveThenDisable = async function() {
  // Ensure no volumes are mounted.
  chrome.test.assertEq(
      0, await remoteCall.callRemoteTestUtil('getVolumesCount', null, []));

  // Mount Downloads.
  await sendTestMessage({name: 'mountDownloads'});

  // Add a file to Downloads.
  await addEntries(['local'], [ENTRIES.newlyAdded]);

  // Wait until it mounts.
  await remoteCall.waitFor('getVolumesCount', null, (count) => count === 1, []);

  // Open Files app without specifying the initial directory/root.
  const appId = await openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Wait for Files app to finish loading.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // We should navigate to Downloads or MyFiles.
  // TODO(crbug.com/880130): Remove this conditional.
  let defaultFolder = '/My files/Downloads';
  let expectedRows = [ENTRIES.newlyAdded.getExpectedRow()];
  if (RootPath.DOWNLOADS_PATH === '/Downloads') {
    defaultFolder = '/My files';
    expectedRows = [
      ['Downloads', '--', 'Folder'],
      ['Linux files', '--', 'Folder'],
    ];
  }

  // Ensure MyFiles or Downloads has loaded.
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});

  // Navigate to Drive.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[root-type-icon=\'drive\']']);

  // The fake Google Drive should be empty.
  await remoteCall.waitForFiles(appId, []);

  // Disable Drive.
  await sendTestMessage({name: 'setDriveEnabled', enabled: false});

  // The current directory should change to the default (Downloads).
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, defaultFolder);

  // Ensure Downloads has loaded.
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});

  // Re-enabled Drive.
  await sendTestMessage({name: 'setDriveEnabled', enabled: true});

  // Wait for the fake drive to reappear.
  await remoteCall.waitForElement(appId, ['[root-type-icon=\'drive\']']);
};

/**
 * Tests Files app resisting the urge to switch to Downloads when mounts change.
 * re-enabling Drive.
 */
testcase.fileDisplayMountWithFakeItemSelected = async function() {
  // Open Files app on Drive with the given test files.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.newlyAdded], []);

  // Ensure Downloads has loaded.
  await remoteCall.waitForFiles(appId, [ENTRIES.newlyAdded.getExpectedRow()]);

  // Navigate to My files.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[root-type-icon=\'my_files\']']);

  // Wait for the navigation to complete.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Mount a USB drive.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the mount to appear.
  await remoteCall.waitForElement(
      appId, ['#directory-tree [volume-type-icon="removable"]']);
  chrome.test.assertEq(
      '/My files',
      await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []));
};

/**
 * Tests Files app switching away from Drive virtual folders when Drive is
 * unmounted.
 */
testcase.fileDisplayUnmountDriveWithSharedWithMeSelected = async function() {
  // Open Files app on Drive with the given test files.
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [ENTRIES.newlyAdded],
      [ENTRIES.testSharedDocument, ENTRIES.hello]);

  // Navigate to Shared with me.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[volume-type-icon=\'drive_shared_with_me\']']);

  // Wait for the navigation to complete.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Shared with me');

  // Check that the file is visible.
  await remoteCall.waitForFiles(
      appId, [ENTRIES.testSharedDocument.getExpectedRow()]);

  // Unmount drive.
  await sendTestMessage({name: 'unmountDrive'});

  // We should navigate to Downloads or MyFiles.
  // TODO(crbug.com/880130): Remove this conditional.
  let defaultFolder = '/My files/Downloads';
  let expectedRows = [ENTRIES.newlyAdded.getExpectedRow()];
  if (RootPath.DOWNLOADS_PATH === '/Downloads') {
    defaultFolder = '/My files';
    expectedRows = [
      ['Play files', '--', 'Folder'],
      ['Downloads', '--', 'Folder'],
      ['Linux files', '--', 'Folder'],
    ];
  }

  // Ensure MyFiles or Downloads has loaded.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, defaultFolder);

  // Which should contain a file.
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});
};
