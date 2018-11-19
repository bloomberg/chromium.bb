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
         isTeamDrive: boolean,
         initialEntries: !Array<TestEntryInfo>
     }} opts Options for creating TransferLocationInfo.
   */
  constructor(opts) {
    /**
     * The volume type (e.g. downloads, drive, drive_recent,
     * drive_shared_with_me, drive_offline) or team drive name. This is used to
     * select the volume with selectVolume() or the team drive with
     * selectTeamDrive().
     * @type {string}
     */
    this.volumeName = opts.volumeName;

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
function transferBetweenVolumes(transferInfo) {
  let appId;

  let srcContents;
  if (transferInfo.source.isTeamDrive) {
    srcContents =
        TestEntryInfo.getExpectedRows(transferInfo.source.initialEntries.filter(
            entry => entry.type !== EntryType.TEAM_DRIVE &&
                entry.teamDriveName === transferInfo.source.volumeName));
  } else {
    srcContents =
        TestEntryInfo.getExpectedRows(transferInfo.source.initialEntries.filter(
            entry => entry.type !== EntryType.TEAM_DRIVE &&
                entry.teamDriveName === ''));
  }
  const myDriveContent =
      TestEntryInfo.getExpectedRows(transferInfo.source.initialEntries.filter(
          entry => entry.type !== EntryType.TEAM_DRIVE &&
              entry.teamDriveName === ''));

  let dstContents;
  if (transferInfo.destination.isTeamDrive) {
    dstContents = TestEntryInfo.getExpectedRows(
        transferInfo.destination.initialEntries.filter(
            entry => entry.type !== EntryType.TEAM_DRIVE &&
                entry.teamDriveName === transferInfo.destination.volumeName));
  } else {
    dstContents = TestEntryInfo.getExpectedRows(
        transferInfo.destination.initialEntries.filter(
            entry => entry.type !== EntryType.TEAM_DRIVE &&
                entry.teamDriveName === ''));
  }

  const localFiles = BASIC_LOCAL_ENTRY_SET;
  const driveFiles = (transferInfo.source.isTeamDrive ||
                      transferInfo.destination.isTeamDrive) ?
      TEAM_DRIVE_ENTRY_SET :
      BASIC_DRIVE_ENTRY_SET;

  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, localFiles, driveFiles);
    },
    // Expand Drive root if either src or dst is within Drive.
    function(results) {
      appId = results.windowId;
      if (transferInfo.source.isTeamDrive ||
          transferInfo.destination.isTeamDrive) {
        // Select + expand + wait for its content.
        remoteCall
            .callRemoteTestUtil('selectFolderInTree', appId, ['Google Drive'])
            .then(result => {
              chrome.test.assertTrue(result);
              return remoteCall.callRemoteTestUtil(
                  'expandSelectedFolderInTree', appId, []);
            })
            .then(result => {
              chrome.test.assertTrue(result);
              return remoteCall.waitForFiles(appId, myDriveContent);
            })
            .then(this.next);
      } else {
        // If isn't drive source, just move on.
        this.next();
      }
    },
    // Select the source volume.
    function() {
      remoteCall.callRemoteTestUtil(
          transferInfo.source.isTeamDrive ? 'selectTeamDrive' : 'selectVolume',
          appId, [transferInfo.source.volumeName], this.next);
    },
    // Wait for the expected files to appear in the file list.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, srcContents).then(this.next);
    },
    // Focus the file list.
    function() {
      remoteCall.callRemoteTestUtil(
          'focus', appId, ['#file-list:not([hidden])'], this.next);
    },
    // Select the source file.
    function() {
      remoteCall.callRemoteTestUtil(
          'selectFile', appId, [transferInfo.fileToTransfer.nameText],
          this.next);
    },
    // Copy the file.
    function(result) {
      chrome.test.assertTrue(result);
      let transferCommand = transferInfo.isMove ? 'move' : 'copy';
      remoteCall.callRemoteTestUtil(
          'execCommand', appId, [transferCommand], this.next);
    },
    // Select the destination volume.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil(
          transferInfo.destination.isTeamDrive ? 'selectTeamDrive' :
                                                 'selectVolume',
          appId, [transferInfo.destination.volumeName], this.next);
    },
    // Wait for the expected files to appear in the file list.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, dstContents).then(this.next);
    },
    // Paste the file.
    function() {
      remoteCall.callRemoteTestUtil('execCommand', appId, ['paste'], this.next);
    },
    // Wait for a dialog if one is expected, or just continue.
    function(result) {
      chrome.test.assertTrue(result);
      // If we're expecting a confirmation dialog, confirm that it is shown.
      if (transferInfo.expectedDialogText !== undefined) {
        return remoteCall.waitForElement(appId, '.cr-dialog-container.shown')
            .then(this.next);
      } else {
        setTimeout(this.next, 0);
      }
    },
    // Click OK if the dialog appears.
    function(element) {
      if (transferInfo.expectedDialogText !== undefined) {
        chrome.test.assertEq(transferInfo.expectedDialogText, element.text);
        // Press OK button.
        remoteCall
            .callRemoteTestUtil(
                'fakeMouseClick', appId, ['button.cr-dialog-ok'])
            .then(this.next);
      } else {
        this.next();
      }
    },
    // Wait for the file list to change, if the test is expected to pass.
    function(result) {
      if (transferInfo.expectedDialogText !== undefined) {
        chrome.test.assertTrue(result);
      }

      const dstContentsAfterPaste = dstContents.slice();
      var ignoreFileSize =
          transferInfo.source.volumeName == 'drive_shared_with_me' ||
          transferInfo.source.volumeName == 'drive_offline' ||
          transferInfo.destination.volumeName == 'drive_shared_with_me' ||
          transferInfo.destination.volumeName == 'drive_offline';

      // If we expected the transfer to succeed, add the pasted file to the list
      // of expected rows.
      if (!transferInfo.expectFailure) {
        var pasteFile = transferInfo.fileToTransfer.getExpectedRow();
        // Check if we need to add (1) to the filename, in the case of a
        // duplicate file.
        for (var i = 0; i < dstContentsAfterPaste.length; i++) {
          if (dstContentsAfterPaste[i][0] === pasteFile[0]) {
            // Replace the last '.' in filename with ' (1).'.
            // e.g. 'my.note.txt' -> 'my.note (1).txt'
            pasteFile[0] = pasteFile[0].replace(/\.(?=[^\.]+$)/, ' (1).');
            break;
          }
        }
        dstContentsAfterPaste.push(pasteFile);
      }
      remoteCall.waitForFiles(appId, dstContentsAfterPaste, {
        ignoreFileSize: ignoreFileSize,
        ignoreLastModifiedTime: true
      }).then(this.next);
    },
    // Check the last contents of file list.
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * A list of transfer locations, for use with transferBetweenVolumes.
 * @enum{TransferLocationInfo}
 */
const TRANSFER_LOCATIONS = Object.freeze({
  drive: new TransferLocationInfo(
      {volumeName: 'drive', initialEntries: BASIC_DRIVE_ENTRY_SET}),

  driveWithTeamDriveEntries: new TransferLocationInfo(
      {volumeName: 'drive', initialEntries: TEAM_DRIVE_ENTRY_SET}),

  downloads: new TransferLocationInfo(
      {volumeName: 'downloads', initialEntries: BASIC_LOCAL_ENTRY_SET}),

  sharedWithMe: new TransferLocationInfo({
    volumeName: 'drive_shared_with_me',
    initialEntries: SHARED_WITH_ME_ENTRY_SET
  }),

  driveOffline: new TransferLocationInfo(
      {volumeName: 'drive_offline', initialEntries: OFFLINE_ENTRY_SET}),

  driveTeamDriveA: new TransferLocationInfo({
    volumeName: 'Team Drive A',
    isTeamDrive: true,
    initialEntries: TEAM_DRIVE_ENTRY_SET
  }),

  driveTeamDriveB: new TransferLocationInfo({
    volumeName: 'Team Drive B',
    isTeamDrive: true,
    initialEntries: TEAM_DRIVE_ENTRY_SET
  }),
});


/**
 * Tests copying from Drive to Downloads.
 */
testcase.transferFromDriveToDownloads = function() {
  transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.drive,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
};

/**
 * Tests copying from Downloads to Drive.
 */
testcase.transferFromDownloadsToDrive = function() {
  transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.hello,
    source: TRANSFER_LOCATIONS.downloads,
    destination: TRANSFER_LOCATIONS.drive,
  }));
};

/**
 * Tests copying from Drive shared with me to Downloads.
 */
testcase.transferFromSharedToDownloads = function() {
  transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testSharedDocument,
    source: TRANSFER_LOCATIONS.sharedWithMe,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
};

/**
 * Tests copying from Drive shared with me to Drive.
 */
testcase.transferFromSharedToDrive = function() {
  transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testSharedDocument,
    source: TRANSFER_LOCATIONS.sharedWithMe,
    destination: TRANSFER_LOCATIONS.drive,
  }));
};

/**
 * Tests copying from Drive offline to Downloads.
 */
testcase.transferFromOfflineToDownloads = function() {
  transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testDocument,
    source: TRANSFER_LOCATIONS.driveOffline,
    destination: TRANSFER_LOCATIONS.downloads,
  }));
};

/**
 * Tests copying from Drive offline to Drive.
 */
testcase.transferFromOfflineToDrive = function() {
  transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.testDocument,
    source: TRANSFER_LOCATIONS.driveOffline,
    destination: TRANSFER_LOCATIONS.drive,
  }));
};

/**
 * Tests copying from a Team Drive to Drive.
 */
testcase.transferFromTeamDriveToDrive = function() {
  transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.teamDriveAFile,
    source: TRANSFER_LOCATIONS.driveTeamDriveA,
    destination: TRANSFER_LOCATIONS.driveWithTeamDriveEntries,
  }));
};

/**
 * Tests copying from Drive to a Team Drive.
 */
testcase.transferFromDriveToTeamDrive = function() {
  transferBetweenVolumes(new TransferInfo({
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
testcase.transferFromTeamDriveToDownloads = function() {
  transferBetweenVolumes(new TransferInfo({
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
testcase.transferHostedFileFromTeamDriveToDownloads = function() {
  transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.teamDriveAHostedFile,
    source: TRANSFER_LOCATIONS.driveTeamDriveA,
    destination: TRANSFER_LOCATIONS.driveWithTeamDriveEntries,
    expectFailure: true,
  }));
};

/**
 * Tests copying from Downloads to a Team Drive.
 */
testcase.transferFromDownloadsToTeamDrive = function() {
  transferBetweenVolumes(new TransferInfo({
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
testcase.transferBetweenTeamDrives = function() {
  transferBetweenVolumes(new TransferInfo({
    fileToTransfer: ENTRIES.teamDriveBFile,
    source: TRANSFER_LOCATIONS.driveTeamDriveB,
    destination: TRANSFER_LOCATIONS.driveTeamDriveA,
    expectedDialogText:
        'Members of \'Team Drive A\' will gain access to the copy of these ' +
        'items.CopyCancel',
  }));
};
