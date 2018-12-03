// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * Tests if MyFiles is displayed when flag is true.
 */
testcase.showMyFiles = async function() {
  const expectedElementLabels = [
    'Recent: FakeItem',
    'My files: EntryListItem',
    'Downloads: SubDirectoryItem',
    'Play files: SubDirectoryItem',
    'Linux files: FakeItem',
    'Google Drive: DriveVolumeItem',
    'My Drive: SubDirectoryItem',
    'Shared with me: SubDirectoryItem',
    'Offline: SubDirectoryItem',
  ];

  // Open Files app on local Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Get the directory tree elements.
  const dirTreeQuery = ['#directory-tree [dir-type]'];
  const elements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, dirTreeQuery);

  // Check tree elements for the correct order and label/element type.
  var visibleElements = [];
  for (let element of elements) {
    if (!element.hidden) {  // Ignore hidden elements.
      visibleElements.push(
          element.attributes['entry-label'] + ': ' +
          element.attributes['dir-type']);
    }
  }
  chrome.test.assertEq(expectedElementLabels, visibleElements);

  // Select Downloads folder.
  await remoteCall.callRemoteTestUtil('selectVolume', appId, ['downloads']);

  // Get the breadcrumbs elements.
  const breadcrumbsQuery = ['#location-breadcrumbs .breadcrumb-path'];
  const breadcrumbs = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, breadcrumbsQuery);

  // Check that My Files is displayed on breadcrumbs.
  const expectedBreadcrumbs = 'My files > Downloads';
  const resultBreadscrubms = breadcrumbs.map(crumb => crumb.text).join(' > ');
  chrome.test.assertEq(expectedBreadcrumbs, resultBreadscrubms);
};

/**
 * Tests search button hidden when selected My Files.
 */
testcase.hideSearchButton = async function() {
  // Open Files app on local Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Select Downloads folder.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectVolume', appId, ['downloads']));

  // Get the search button element.
  const buttonQuery = ['#search-button'];
  let buttonElements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, buttonQuery);

  // Check that search button is visible on Downloads.
  chrome.test.assertEq(1, buttonElements.length);
  chrome.test.assertFalse(buttonElements[0].hidden);

  // Select My Files folder.
  const myFilesQuery = '#directory-tree [entry-label="My files"]';
  const isDriveQuery = false;
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectInDirectoryTree', appId, [myFilesQuery, isDriveQuery]));

  // Get the search button element.
  buttonElements = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, buttonQuery);

  // Check that search button is hidden on My Files.
  chrome.test.assertEq(1, buttonElements.length);
  chrome.test.assertTrue(buttonElements[0].hidden);
};

/**
 * Tests directory tree refresh doesn't hide Downloads folder.
 *
 * This tests a regression where Downloads folder would disappear because
 * MyFiles model and entry were being recreated on every update and
 * DirectoryTree expects NavigationModelItem to be the same instance through
 * updates.
 */
testcase.directoryTreeRefresh = async function() {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Open Files app on local Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB volume to mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Select Downloads folder.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectVolume', appId, ['downloads']));
};

/**
 * Tests My Files displaying Downloads on file list (RHS) and opening Downloads
 * from file list.
 */
testcase.myFilesDisplaysAndOpensEntries = async function() {
  // Open Files app on local Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Select My Files folder.
  const myFilesQuery = '#directory-tree [entry-label="My files"]';
  const isDriveQuery = false;
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectInDirectoryTree', appId, [myFilesQuery, isDriveQuery]));

  // Wait for file list to display Downloads and Crostini.
  const downloadsRow = ['Downloads', '--', 'Folder'];
  const playFilesRow = ['Play files', '--', 'Folder'];
  const crostiniRow = ['Linux files', '--', 'Folder'];
  await remoteCall.waitForFiles(
      appId, [downloadsRow, playFilesRow, crostiniRow],
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Double click on Download on file list.
  const downloadsFileListQuery = '#file-list [file-name="Downloads"]';
  await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId, [downloadsFileListQuery]);

  // Wait for file list to Downloads' content.
  await remoteCall.waitForFiles(
      appId, [ENTRIES.beautiful.getExpectedRow()],
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Get the selected navigation tree item.
  chrome.test.assertEq(
      'Downloads',
      await remoteCall.callRemoteTestUtil('getSelectedTreeItem', appId, []));
};

/**
 * Tests My files updating its children recursively.
 *
 * If it doesn't update its children recursively it can cause directory tree to
 * not show or hide sub-folders crbug.com/864453.
 */
testcase.myFilesUpdatesChildren = async function() {
  const downloadsQuery = '#directory-tree [entry-label="Downloads"]';
  const hiddenFolder = new TestEntryInfo({
    type: EntryType.DIRECTORY,
    targetPath: '.hidden-folder',
    lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
    nameText: '.hidden-folder',
    sizeText: '--',
    typeText: 'Folder'
  });

  // Add a hidden folder.
  // It can't be added via setupAndWaitUntilReady, because it isn't
  // displayed and that function waits all entries to be displayed.
  await addEntries(['local'], [hiddenFolder]);

  // Open Files app on local Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Select Downloads folder.
  const isDriveQuery = false;
  await remoteCall.callRemoteTestUtil(
      'selectInDirectoryTree', appId, [downloadsQuery, isDriveQuery]);

  // Wait for gear menu to be displayed.
  await remoteCall.waitForElement(appId, '#gear-button');

  // Open the gear menu by clicking the gear button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-button']));

  // Wait for menu to not be hidden.
  await remoteCall.waitForElement(appId, '#gear-menu:not([hidden])');

  // Wait for menu item to appear.
  await remoteCall.waitForElement(
      appId, '#gear-menu-toggle-hidden-files:not([disabled])');

  // Wait for menu item to appear.
  await remoteCall.waitForElement(
      appId, '#gear-menu-toggle-hidden-files:not([checked])');

  // Click the menu item.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#gear-menu-toggle-hidden-files']);

  // Check the hidden folder to be displayed in RHS.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([hiddenFolder, ENTRIES.beautiful]),
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Check the hidden folder to be displayed in LHS.
  // Children of Downloads and named ".hidden-folder".
  const hiddenFolderTreeQuery = downloadsQuery +
      ' .tree-children .tree-item[entry-label=".hidden-folder"]';
  await remoteCall.waitForElement(appId, hiddenFolderTreeQuery);
};

/**
 * Check naming a folder after navigating inside MyFiles using file list (RHS).
 * crbug.com/889636.
 */
testcase.myFilesFolderRename = async function() {
  const textInput = '#file-list .table-row[renaming] input.rename';

  // Open Files app on local Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Select "My files" folder via directory tree.
  const myFilesQuery = '#directory-tree [entry-label="My files"]';
  const isDriveQuery = false;
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'selectInDirectoryTree', appId, [myFilesQuery, isDriveQuery]),
      'selectInDirectoryTree failed');

  // Wait for Downloads to load.
  const expectedRows = [
    ['Downloads', '--', 'Folder'],
    ['Play files', '--', 'Folder'],
    ['Linux files', '--', 'Folder'],
  ];
  await remoteCall.waitForFiles(
      appId, expectedRows,
      {ignoreFileSize: true, ignoreLastModifiedTime: true});

  // Select Downloads via file list.
  const downloads = ['Downloads'];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, downloads),
      'selectFile failed');

  // Open Downloads via file list.
  const fileListItem = '#file-list .table-row';
  const key = [fileListItem, 'Enter', false, false, false];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key),
      'fakeKeyDown failed');

  // Wait for Downloads to load.
  await remoteCall.waitForFiles(appId, [ENTRIES.photos.getExpectedRow()]);

  // Select photos via file list.
  const folder = ['photos'];
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, folder),
      'selectFile failed');

  // Press Ctrl+Enter for start renaming.
  const key2 = [fileListItem, 'Enter', true, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key2),
      'fakeKeyDown ctrl+Enter failed');

  // Wait for input for renaming to appear.
  // Check: the renaming text input should be shown in the file list.
  await remoteCall.waitForElement(appId, textInput);

  // Type new name.
  await remoteCall.callRemoteTestUtil(
      'inputText', appId, [textInput, 'new name']);

  // Send Enter key to the text input.
  const key3 = [textInput, 'Enter', false, false, false];
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key3),
      'fakeKeyDown failed');

  // Wait for new name to appear on the file list.
  const expectedRows2 = [['new name', '--', 'Folder', '']];
  await remoteCall.waitForFiles(
      appId, expectedRows2,
      {ignoreFileSize: true, ignoreLastModifiedTime: true});
};
