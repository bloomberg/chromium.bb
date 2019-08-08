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
 * Waits 250ms and fail if toast shows in this time.
 *
 * @param {string} appId The app's window id.
 */
async function checkHiddenToast(appId) {
  const start = new Date();
  const caller = getCaller();
  await repeatUntil(async () => {
    if (new Date() - start > 250 /* ms */) {
      // Waited long enough. End with success.
      return;
    }

    const visibleQuery = ['#toast', '#container:not([hidden])'];
    const visibleToast = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [visibleQuery]);

    // Fails if finds a non-hidden toast.
    chrome.test.assertTrue(
        visibleToast.length == 0, 'Toast is visible when it shouldn\'t');

    return pending(caller, 'Waiting to see if toast will show.');
  });
}

/**
 * Tests files display in Downloads.
 */
testcase.fileDisplayDownloads = () => {
  return fileDisplay(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);
};

/**
 * Tests files display in Google Drive.
 */
testcase.fileDisplayDrive = () => {
  return fileDisplay(RootPath.DRIVE, BASIC_DRIVE_ENTRY_SET);
};

/**
 * Tests file display rendering in offline Google Drive.
 */
testcase.fileDisplayDriveOffline = async () => {
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
testcase.fileDisplayDriveOnline = async () => {
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
testcase.fileDisplayComputers = async () => {
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
testcase.fileDisplayMtp = async () => {
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
testcase.fileDisplayUsb = async () => {
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
 * Tests files display on a removable USB volume with and without partitions.
 */
testcase.fileDisplayUsbPartition = async () => {
  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount USB device containing partitions.
  await sendTestMessage({name: 'mountUsbWithPartitions'});
  // Mount unpartitioned USB device.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for removable root to appear in the directory tree.
  const removableRoot = await remoteCall.waitForElement(
      appId, '#directory-tree [entry-label="Drive Label"]');

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

  // Check partitions are children of the root label.
  const childEntriesQuery =
      ['[entry-label="Drive Label"] .tree-children .tree-item'];
  const childEntries = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, childEntriesQuery);
  const childEntryLabels =
      childEntries.map(child => child.attributes['entry-label']);
  chrome.test.assertEq(['partition-1', 'partition-2'], childEntryLabels);

  // Wait for USB to appear in the directory tree.
  const fakeUsb = await remoteCall.waitForElement(
      appId, '#directory-tree [entry-label="fake-usb"]');
  chrome.test.assertEq(
      'removable', fakeUsb.attributes['volume-type-for-testing']);

  // Check unpartitioned USB does not have partitions as tree children.
  const itemEntriesQuery =
      ['[entry-label="fake-usb"] .tree-children .tree-item'];
  const itemEntries = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, itemEntriesQuery);
  chrome.test.assertEq(1, itemEntries.length);
  const childVolumeType = itemEntries[0].attributes['volume-type-for-testing'];
  chrome.test.assertTrue('removable' !== childVolumeType);
};

/**
 * Tests USB toast display for ARC on clicking a USB volume.
 */
testcase.fileDisplayUsbToast = async () => {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount USB volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Enable ARC, but disallow ARC from accessing the USB volume so that the
  // toast UI will be shown.
  const preferences = {
    arcEnabled: true,
    arcRemovableMediaAccessEnabled: false,
  };
  await remoteCall.callRemoteTestUtil('setPreferences', null, [preferences]);

  // Click to open the USB volume.
  await remoteCall.waitAndClickElement(appId, USB_VOLUME_QUERY);

  // Verify the toast UI.
  await remoteCall.waitForElement(
      appId, ['#toast', '#container:not([hidden])']);

  // Wait for the toast to disappear.
  await remoteCall.waitForElement(appId, ['#toast', '#container[hidden]']);

  // Navigate to My files.
  await remoteCall.waitAndClickElement(appId, '[root-type-icon=\'my_files\']');

  // Verify the toast UI never shows up.
  await checkHiddenToast(appId);

  // Navigate to the USB volume again.
  await remoteCall.waitAndClickElement(appId, USB_VOLUME_QUERY);

  // Verify the toast UI never shows up again.
  await checkHiddenToast(appId);
};

/**
 * Tests USB toast won't show up when ARC is disabled or the access has already
 * been granted.
 */
testcase.fileDisplayUsbNoToast = async () => {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount USB volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Disable ARC so that the toast UI won't be shown.
  const preferences = {
    arcEnabled: false,
  };
  await remoteCall.callRemoteTestUtil('setPreferences', null, [preferences]);

  // Click to open the USB volume.
  await remoteCall.waitAndClickElement(appId, USB_VOLUME_QUERY);

  // Verify the toast UI never shows up.
  await checkHiddenToast(appId);

  // Navigate to My files.
  await remoteCall.waitAndClickElement(appId, '[root-type-icon=\'my_files\']');

  // Enable ARC and its storage access so that the toast UI won't be shown.
  const preferences2 = {
    arcEnabled: true,
    arcRemovableMediaAccessEnabled: true,
  };
  await remoteCall.callRemoteTestUtil('setPreferences', null, [preferences2]);

  // Navigate to the USB volume again.
  await remoteCall.waitAndClickElement(appId, USB_VOLUME_QUERY);

  // Verify the toast UI is still not shown.
  await checkHiddenToast(appId);
};

/**
 * Tests partitions display in the file table when root removable entry
 * is selected. Checks file system type is displayed.
 */
testcase.fileDisplayPartitionFileTable = async () => {
  const removableGroup = '#directory-tree [root-type-icon="removable"]';

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount removable partitions.
  await sendTestMessage({name: 'mountUsbWithPartitions'});

  // Wait for removable group to appear in the directory tree.
  await remoteCall.waitForElement(appId, removableGroup);

  // Select the first removable group by clicking the label.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [removableGroup]));

  // Wait for removable partitions to appear in the file table.
  const partitionOne = await remoteCall.waitForElement(
      appId, '#file-list [file-name="partition-1"] .type');
  chrome.test.assertEq('ext4', partitionOne.text);

  const partitionTwo = await remoteCall.waitForElement(
      appId, '#file-list [file-name="partition-2"] .type');
  chrome.test.assertEq('ext4', partitionOne.text);
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
testcase.fileSearch = () => {
  return searchDownloads('hello', [ENTRIES.hello]);
};

/**
 * Tests case-insenstive search for an entry in Downloads.
 */
testcase.fileSearchCaseInsensitive = () => {
  return searchDownloads('HELLO', [ENTRIES.hello]);
};

/**
 * Tests searching for a string doesn't match anything in Downloads and that
 * there are no displayed items that match the search string.
 */
testcase.fileSearchNotFound = async () => {
  const searchTerm = 'blahblah';

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
testcase.fileDisplayWithoutDownloadsVolume = async () => {
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
testcase.fileDisplayWithoutVolumes = async () => {
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
testcase.fileDisplayWithoutVolumesThenMountDownloads = async () => {
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
testcase.fileDisplayWithoutVolumesThenMountDrive = async () => {
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
testcase.fileDisplayWithoutDrive = async () => {
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
testcase.fileDisplayWithoutDriveThenDisable = async () => {
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
testcase.fileDisplayMountWithFakeItemSelected = async () => {
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
testcase.fileDisplayUnmountDriveWithSharedWithMeSelected = async () => {
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

/**
 * Navigates to a removable volume, then unmounts it. Check to see whether
 * Files App switches away to the default Downloads directory.
 *
 * @param {string} removableDirectory The removable directory to be inside
 *    before unmounting the USB.
 */
async function unmountRemovableVolume(removableDirectory) {
  const removableRootQuery = '#directory-tree [root-type-icon="removable"]';

  // Open Files app on Downloads containing ENTRIES.photos.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Mount a device containing two partitions.
  await sendTestMessage({name: 'mountUsbWithPartitions'});

  // Wait for the removable root to appear in the directory tree.
  await remoteCall.waitForElement(appId, removableRootQuery);

  // Navigate to the removable root directory.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, [removableRootQuery]);

  // Wait for the navigation to complete.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Drive Label');

  // Wait for partition entries to appear in the directory.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="partition-1"]');
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="partition-2"]');

  if (removableDirectory === 'partition-1' ||
      removableDirectory === 'partition-2') {
    const partitionQuery = `#file-list [file-name="${removableDirectory}"]`;
    const partitionFiles = [
      ENTRIES.hello.getExpectedRow(),
      ['Folder', '--', 'Folder', Date()],
    ];
    await remoteCall.callRemoteTestUtil(
        'fakeMouseDoubleClick', appId, [partitionQuery]);
    await remoteCall.waitUntilCurrentDirectoryIsChanged(
        appId, `/Drive Label/${removableDirectory}`);
    await remoteCall.waitForFiles(
        appId, partitionFiles, {ignoreLastModifiedTime: true});
  }

  // Unmount partitioned device.
  await sendTestMessage({name: 'unmountPartitions'});

  // We should navigate to Downloads or MyFiles.
  // TODO(crbug.com/880130): Remove this conditional.
  let defaultFolder = '/My files/Downloads';
  let expectedRows = [ENTRIES.photos.getExpectedRow()];
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

  // And contains the expected files.
  await remoteCall.waitForFiles(
      appId, expectedRows, {ignoreLastModifiedTime: true});
}

/**
 * Tests Files app switches away from a removable device root after the USB is
 * unmounted.
 */
testcase.fileDisplayUnmountRemovableRoot = () => {
  return unmountRemovableVolume('Drive Label');
};

/**
 * Tests Files app switches away from a partition inside the USB after the USB
 * is unmounted.
 */
testcase.fileDisplayUnmountFirstPartition = () => {
  return unmountRemovableVolume('partition-1');
};

/**
 * Tests Files app switches away from a partition inside the USB after the USB
 * is unmounted. Partition-1 will be ejected first.
 */
testcase.fileDisplayUnmountLastPartition = () => {
  return unmountRemovableVolume('partition-2');
};

/**
 * Tests files display in Downloads while the default blocking file I/O task
 * runner is blocked.
 */
testcase.fileDisplayDownloadsWithBlockedFileTaskRunner = async () => {
  await sendTestMessage({name: 'blockFileTaskRunner'});
  await fileDisplay(RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET);
  await sendTestMessage({name: 'unblockFileTaskRunner'});
};

/**
 * Tests to make sure check-select mode enables when selecting one item
 */
testcase.fileDisplayCheckSelectWithFakeItemSelected = async () => {
  // Open files app on Downloads containing ENTRIES.hello.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Select ENTRIES.hello.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt']));

  // Select all.
  const ctrlA = ['#file-list', 'a', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA));

  // Make sure check-select is enabled.
  await remoteCall.waitForElement(appId, 'body.check-select');
};
