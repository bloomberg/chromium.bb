// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Info for the source or destination of a transfer.
 */
class TransferLocationInfo {
  /*
   * Create a new TransferLocationInfo.
   *
   * @param{{
         volumeName: !string,
         breadcrumbsPath: !string,
         isTeamDrive: boolean,
         initialEntries: !Array<TestEntryInfo>
     }} opts Options for creating TransferLocationInfo.
   */
  constructor(opts) {
    /**
     * The volume type (e.g. downloads, drive, drive_recent,
     * drive_shared_with_me, drive_offline) or team drive name.
     * @type {string}
     */
    this.volumeName = opts.volumeName;

    /** @type {string} */
    this.breadcrumbsPath = opts.breadcrumbsPath;

    /**
     * Whether this transfer location is a team drive. Defaults to false.
     * @type {boolean}
     */
    this.isTeamDrive = opts.isTeamDrive || false;

    /**
     * Expected initial contents in the volume.
     * @type {Array<TestEntryInfo>}
     */
    this.initialEntries = opts.initialEntries;
  }
}

/**
 * Info for the transfer operation.
 */
class TransferInfo {
  /*
   * Create a new TransferInfo.
   *
   * @param{{
         fileToTransfer: !TestEntryInfo,
         source: !TransferLocationInfo,
         destination: !TransferLocationInfo,
         expectedDialogText: string,
         isMove: boolean,
         expectFailure: boolean,
     }} opts Options for creating TransferInfo.
   */
  constructor(opts) {
    /**
     * The file to copy or move. Must be in the source location.
     * @type {!TestEntryInfo}
     */
    this.fileToTransfer = opts.fileToTransfer;

    /**
     * The source location.
     * @type {!TransferLocationInfo}
     */
    this.source = opts.source;

    /**
     * The destination location.
     * @type {!TransferLocationInfo}
     */
    this.destination = opts.destination;

    /**
     * The expected content of the transfer dialog (including any buttons), or
     * undefined if no dialog is expected.
     * @type {string}
     */
    this.expectedDialogText = opts.expectedDialogText || undefined;

    /**
     * True if this transfer is for a move operation, false for a copy
     * operation.
     * @type {!boolean}
     */
    this.isMove = opts.isMove || false;

    /**
     * Whether the test is expected to fail, i.e. transferring to a folder
     * without correct permissions.
     * @type {!boolean}
     */
    this.expectFailure = opts.expectFailure || false;
  }
}

/**
 * Test function to copy from the specified source to the specified destination.
 * @param {TransferInfo} transferInfo Options for the transfer.
 */
async function transferBetweenVolumes(transferInfo) {
  let srcContents;
  if (transferInfo.source.isTeamDrive) {
    srcContents =
        TestEntryInfo.getExpectedRows(transferInfo.source.initialEntries.filter(
            entry => entry.type !== EntryType.SHARED_DRIVE &&
                entry.teamDriveName === transferInfo.source.volumeName));
  } else {
    srcContents =
        TestEntryInfo.getExpectedRows(transferInfo.source.initialEntries.filter(
            entry => entry.type !== EntryType.SHARED_DRIVE &&
                entry.teamDriveName === ''));
  }

  let dstContents;
  if (transferInfo.destination.isTeamDrive) {
    dstContents = TestEntryInfo.getExpectedRows(
        transferInfo.destination.initialEntries.filter(
            entry => entry.type !== EntryType.SHARED_DRIVE &&
                entry.teamDriveName === transferInfo.destination.volumeName));
  } else {
    dstContents = TestEntryInfo.getExpectedRows(
        transferInfo.destination.initialEntries.filter(
            entry => entry.type !== EntryType.SHARED_DRIVE &&
                entry.teamDriveName === ''));
  }

  const localFiles = BASIC_LOCAL_ENTRY_SET;
  const driveFiles = (transferInfo.source.isTeamDrive ||
                      transferInfo.destination.isTeamDrive) ?
      SHARED_DRIVE_ENTRY_SET :
      BASIC_DRIVE_ENTRY_SET;

  // Open files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, localFiles, driveFiles);

  // Expand Drive root if either src or dst is within Drive.
  if (transferInfo.source.isTeamDrive || transferInfo.destination.isTeamDrive) {
    const myDriveContent = TestEntryInfo.getExpectedRows(
        driveFiles.filter(e => e.teamDriveName === '' && e.computerName === ''));
    // Select + expand + wait for its content.
    await navigateWithDirectoryTree(appId, '/My Drive');
    await remoteCall.waitForFiles(appId, myDriveContent);
  }

  // Select the source folder.
  await navigateWithDirectoryTree(appId, transferInfo.source.breadcrumbsPath);

  // Wait for the expected files to appear in the file list.
  await remoteCall.waitForFiles(appId, srcContents);

  // Focus the file list.
  await remoteCall.callRemoteTestUtil(
      'focus', appId, ['#file-list:not([hidden])']);

  // Select the source file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [transferInfo.fileToTransfer.nameText]));

  // Copy the file.
  const transferCommand = transferInfo.isMove ? 'cut' : 'copy';
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'execCommand', appId, [transferCommand]));

  // Select the destination folder.
  await navigateWithDirectoryTree(
      appId, transferInfo.destination.breadcrumbsPath);

  // Wait for the expected files to appear in the file list.
  await remoteCall.waitForFiles(
      appId, dstContents, {ignoreFileSize: true, ignoreLastModifiedTime: true});
  // Paste the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // If we're expecting a confirmation dialog, confirm that it is shown.
  if (transferInfo.expectedDialogText !== undefined) {
    const {text} =
        await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');
    chrome.test.assertEq(transferInfo.expectedDialogText, text);

    // Press OK button.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseClick', appId, ['button.cr-dialog-ok']));
  }

  // Wait for the file list to change, if the test is expected to pass.
  const dstContentsAfterPaste = dstContents.slice();
  const ignoreFileSize =
      transferInfo.source.volumeName == 'drive_shared_with_me' ||
      transferInfo.source.volumeName == 'drive_offline' ||
      transferInfo.destination.volumeName == 'drive_shared_with_me' ||
      transferInfo.destination.volumeName == 'drive_offline' ||
      transferInfo.destination.volumeName == 'my_files';

  // If we expected the transfer to succeed, add the pasted file to the list
  // of expected rows.
  if (!transferInfo.expectFailure &&
      transferInfo.source !== transferInfo.destination) {
    const pasteFile = transferInfo.fileToTransfer.getExpectedRow();
    // Check if we need to add (1) to the filename, in the case of a
    // duplicate file.
    for (let i = 0; i < dstContentsAfterPaste.length; i++) {
      if (dstContentsAfterPaste[i][0] === pasteFile[0]) {
        // Replace the last '.' in filename with ' (1).'.
        // e.g. 'my.note.txt' -> 'my.note (1).txt'
        pasteFile[0] = pasteFile[0].replace(/\.(?=[^\.]+$)/, ' (1).');
        break;
      }
    }
    dstContentsAfterPaste.push(pasteFile);
  }

  // Check the last contents of file list.
  await remoteCall.waitForFiles(
      appId, dstContentsAfterPaste,
      {ignoreFileSize: ignoreFileSize, ignoreLastModifiedTime: true});

  return appId;
}

/**
 * A list of transfer locations, for use with transferBetweenVolumes.
 * @enum{TransferLocationInfo}
 */
const TRANSFER_LOCATIONS = Object.freeze({
  drive: new TransferLocationInfo({
    breadcrumbsPath: '/My Drive',
    volumeName: 'drive',
    initialEntries: BASIC_DRIVE_ENTRY_SET
  }),

  driveWithTeamDriveEntries: new TransferLocationInfo({
    breadcrumbsPath: '/My Drive',
    volumeName: 'drive',
    initialEntries: SHARED_DRIVE_ENTRY_SET
  }),

  downloads: new TransferLocationInfo({
    breadcrumbsPath: '/My files/Downloads',
    volumeName: 'downloads',
    initialEntries: BASIC_LOCAL_ENTRY_SET
  }),

  sharedWithMe: new TransferLocationInfo({
    breadcrumbsPath: '/Shared with me',
    volumeName: 'drive_shared_with_me',
    initialEntries: SHARED_WITH_ME_ENTRY_SET
  }),

  driveOffline: new TransferLocationInfo({
    breadcrumbsPath: '/Offline',
    volumeName: 'drive_offline',
    initialEntries: OFFLINE_ENTRY_SET
  }),

  driveTeamDriveA: new TransferLocationInfo({
    breadcrumbsPath: '/Shared drives/Team Drive A',
    volumeName: 'Team Drive A',
    isTeamDrive: true,
    initialEntries: SHARED_DRIVE_ENTRY_SET
  }),

  driveTeamDriveB: new TransferLocationInfo({
    breadcrumbsPath: '/Shared drives/Team Drive B',
    volumeName: 'Team Drive B',
    isTeamDrive: true,
    initialEntries: SHARED_DRIVE_ENTRY_SET
  }),

  my_files: new TransferLocationInfo({
    breadcrumbsPath: '/My files',
    volumeName: 'my_files',
    initialEntries: [
      new TestEntryInfo({
        type: EntryType.DIRECTORY,
        targetPath: 'Play files',
        nameText: 'Play files',
        lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
        sizeText: '--',
        typeText: 'Folder'
      }),
      new TestEntryInfo({
        type: EntryType.DIRECTORY,
        targetPath: 'Downloads',
        nameText: 'Downloads',
        lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
        sizeText: '--',
        typeText: 'Folder'
      }),
      new TestEntryInfo({
        type: EntryType.DIRECTORY,
        targetPath: 'Linux files',
        nameText: 'Linux files',
        lastModifiedTime: '...',
        sizeText: '--',
        typeText: 'Folder'
      }),
    ]
  }),
});

/**
 * Tests copying from Drive to Downloads.
 */
testcase.transferFromDriveToDownloads = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.drive,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
};

/**
 * Tests moving files from MyFiles/Downloads to MyFiles crbug.com/925175.
 */
testcase.transferFromDownloadsToMyFilesMove = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.my_files,
    isMove: true,
  }));
};

/**
 * Tests copying files from MyFiles/Downloads to MyFiles crbug.com/925175.
 */
testcase.transferFromDownloadsToMyFiles = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.my_files,
    isMove: false,
  }));
};

/**
 * Tests copying from Downloads to Drive.
 */
testcase.transferFromDownloadsToDrive = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.drive,
  }));
};

/**
 * Tests copying from Drive shared with me to Downloads.
 */
testcase.transferFromSharedToDownloads = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testSharedFile,
    source: TRANSFER_LOCATIONS.sharedWithMe,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
};

/**
 * Tests copying from Drive shared with me to Drive.
 */
testcase.transferFromSharedToDrive = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testSharedDocument,
    source: TRANSFER_LOCATIONS.sharedWithMe,
    destination: TRANSFER_LOCATIONS.drive,
  }));
};

/**
 * Tests copying from Drive offline to Downloads.
 */
testcase.transferFromOfflineToDownloads = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testSharedFile,
    source: TRANSFER_LOCATIONS.driveOffline,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
};

/**
 * Tests copying from Drive offline to Drive.
 */
testcase.transferFromOfflineToDrive = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testDocument,
    source: TRANSFER_LOCATIONS.driveOffline,
    destination: TRANSFER_LOCATIONS.drive,
  }));
};

/**
 * Tests copying from a Team Drive to Drive.
 */
testcase.transferFromTeamDriveToDrive = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.teamDriveAFile,
    source: TRANSFER_LOCATIONS.driveTeamDriveA,
    destination: TRANSFER_LOCATIONS.driveWithTeamDriveEntries,
  }));
};

/**
 * Tests copying from Drive to a Team Drive.
 */
testcase.transferFromDriveToTeamDrive = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.driveWithTeamDriveEntries,
    destination: TRANSFER_LOCATIONS.driveTeamDriveA,
    expectedDialogText:
        'Members of \'Team Drive A\' will gain access to the copy of these ' +
        'items.CopyCancel',
  }));
};

/**
 * Tests copying from a Team Drive to Downloads.
 */
testcase.transferFromTeamDriveToDownloads = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.teamDriveAFile,
    source: TRANSFER_LOCATIONS.driveTeamDriveA,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
};

/**
 * Tests that a hosted file cannot be transferred from a Team Drive to a local
 * drive (e.g. Downloads). Hosted documents only make sense in the context of
 * Drive.
 */
testcase.transferHostedFileFromTeamDriveToDownloads = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.teamDriveAHostedFile,
    source: TRANSFER_LOCATIONS.driveTeamDriveA,
    destination: TRANSFER_LOCATIONS.downloads,
    expectFailure: true,
  }));
};

/**
 * Tests copying from Downloads to a Team Drive.
 */
testcase.transferFromDownloadsToTeamDrive = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.driveTeamDriveA,
    expectedDialogText:
        'Members of \'Team Drive A\' will gain access to the copy of these ' +
        'items.CopyCancel',
  }));
};

/**
 * Tests copying between Team Drives.
 */
testcase.transferBetweenTeamDrives = () => {
  return transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.teamDriveBFile,
    source: TRANSFER_LOCATIONS.driveTeamDriveB,
    destination: TRANSFER_LOCATIONS.driveTeamDriveA,
    expectedDialogText:
        'Members of \'Team Drive A\' will gain access to the copy of these ' +
        'items.CopyCancel',
  }));
};

/**
 * Tests that moving a file to its current location is a no-op.
 */
testcase.transferFromDownloadsToDownloads = async () => {
  const appId = await transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.downloads,
    isMove: true,
  }));
  chrome.test.assertEq(
      '',
      (await remoteCall.waitForElement(appId, '.progress-frame label')).text);
};

/**
 * Tests that we can drag a file from #file-list to #directory-tree.
 * It copies the file from Downloads to Downloads/photos.
 */
testcase.transferDragAndDrop = async () => {
  const entries = [ENTRIES.hello, ENTRIES.photos];

  // Open files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // Expand Downloads to display "photos" folder in the directory tree.
  await expandTreeItem(appId, '#directory-tree [entry-label="Downloads"]');

  // Drag has to start in the file list column "name" text content, otherwise it
  // starts a selection instead of a drag.
  const src =
      `#file-list li[file-name="${ENTRIES.hello.nameText}"] .entry-name`;
  const dst = '#directory-tree [entry-label="photos"]';

  // Select the file to be dragged.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [src]),
      'fakeMouseClick failed');

  // Drag and drop it.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeDragAndDrop', appId, [src, dst]),
      'fakeDragAndDrop failed');

  // Navigate to the dst folder.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [dst]),
      'fakeMouseClick failed');

  // Wait for navigation to finish.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads/photos');

  // Wait for the expected files to appear in the file list.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows([ENTRIES.hello]),
      {ignoreLastModifiedTime: true});
};

/**
 * Tests that we can drag a file from #file-list and hover above USB root as
 * EntryList without raising an error.
 */
testcase.transferDragAndHover = async () => {
  const entries = [ENTRIES.hello, ENTRIES.photos];

  await sendTestMessage({name: 'mountUsbWithPartitions'});
  await sendTestMessage({name: 'mountFakeUsb'});

  // Open files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // Drag has to start in the file list column "name" text content, otherwise it
  // starts a selection instead of a drag.
  const src =
      `#file-list li[file-name="${ENTRIES.hello.nameText}"] .entry-name`;
  const dst1 = '#directory-tree [entry-label="Drive Label"]';
  const dst2 = '#directory-tree [entry-label="fake-usb"]';

  // Wait for USB roots to be ready.
  await remoteCall.waitForElement(appId, dst1);
  await remoteCall.waitForElement(appId, dst2);

  // Drag and hover it.
  const skipDrop = true;
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil(
          'fakeDragAndDrop', appId, [src, dst1, skipDrop]),
      'fakeDragAndDrop failed');
};

/**
 * Tests that copying a deleted file shows an error.
 */
testcase.transferDeletedFile = async () => {
  const entry = ENTRIES.hello;

  // Open files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select the file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entry.nameText]));

  // Copy the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  // Delete the file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'deleteFile', appId, [entry.nameText]));

  // Confirm deletion.
  await waitAndAcceptDialog(appId);

  // Paste the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Check that the error appears in the feedback panel.
  const element = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  chrome.test.assertEq(
      `Whoops, ${entry.nameText} no longer exists.`,
      element.attributes['primary-text']);

  // Check that only one line of text is shown.
  chrome.test.assertFalse(!!element.attributes['secondary-text']);
};

/**
 * Tests that transfer source/destination persists if app window is re-opened.
 */
testcase.transferInfoIsRemembered = async () => {
  const entry = ENTRIES.hello;

  // Open files app.
  let appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select the file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entry.nameText]));

  // Copy the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  // Tell the background page to never finish the file copy.
  await remoteCall.callRemoteTestUtil(
      'progressCenterNeverNotifyCompleted', appId, []);

  // Paste the file to begin a copy operation.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // The feedback panel should appear: record the feedback panel text.
  let panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  const primaryText = panel.attributes['primary-text'];
  const secondaryText = panel.attributes['secondary-text'];

  // Close the Files app window.
  await remoteCall.closeWindowAndWait(appId);

  // Open a Files app window again.
  appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check the feedback panel text is remembered.
  panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  chrome.test.assertEq(primaryText, panel.attributes['primary-text']);
  chrome.test.assertEq(secondaryText, panel.attributes['secondary-text']);
};

/**
 * Tests that destination text line shows name for USB targets.
 */
testcase.transferToUsbHasDestinationText = async () => {
  const entry = ENTRIES.hello;

  // Open files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  // Select the file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entry.nameText]));

  // Copy the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  // Select USB volume.
  await navigateWithDirectoryTree(appId, '/fake-usb');

  // Tell the background page to never finish the file copy.
  await remoteCall.callRemoteTestUtil(
      'progressCenterNeverNotifyCompleted', appId, []);

  // Paste the file to begin a copy operation.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Check the feedback panel destination message contains the target device.
  const panel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  chrome.test.assertEq('To fake-usb', panel.attributes['secondary-text']);
};

/**
 * Tests that dismissing an error notification on the foreground
 * page is propagated to the background page.
 */
testcase.transferDismissedErrorIsRemembered = async () => {
  const DOWNLOADS_QUERY = '#directory-tree [entry-label="Downloads"]';
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  let appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Select a file to copy.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entry.nameText]));

  // Copy the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  // Force all file copy operations to trigger an error.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'forceErrorsOnFileOperations', appId, [true]));

  // Select Downloads.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectInDirectoryTree', appId, [DOWNLOADS_QUERY]));

  // Paste the file to begin a copy operation.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Check: an error feedback panel with failure status should appear.
  let errorPanel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  // If we've grabbed a reference to a progress panel, it will disappear
  // quickly and be replaced by the error panel, so loop and wait for it.
  while (errorPanel.attributes['indicator'] === 'progress') {
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'requestAnimationFrame', appId, []));
    errorPanel = await remoteCall.waitForElement(
        appId, ['#progress-panel', 'xf-panel-item']);
  }
  chrome.test.assertEq('failure', errorPanel.attributes['status']);

  // Press the dismiss button on the error feedback panel.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      [['#progress-panel', 'xf-panel-item', 'xf-button#secondary-action']]));

  // Close the Files app window.
  await remoteCall.closeWindowAndWait(appId);

  // Open a Files app window again.
  appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Turn off the error generation for file operations.
  chrome.test.assertFalse(await remoteCall.callRemoteTestUtil(
      'forceErrorsOnFileOperations', appId, [false]));

  // Tell the background page to never finish the file copy.
  await remoteCall.callRemoteTestUtil(
      'progressCenterNeverNotifyCompleted', appId, []);

  // Paste the file to begin a copy operation.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Check: the first feedback panel item should be a progress panel.
  // If the error persisted then we'd see a summary panel here.
  const progressPanel = await remoteCall.waitForElement(
      appId, ['#progress-panel', 'xf-panel-item']);
  chrome.test.assertEq('progress', progressPanel.attributes['indicator']);
};
