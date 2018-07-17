// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Info for the source or destination of a transfer.
 * @struct
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

    /**
     * Whether the test is expected to fail, i.e. transferring to a folder
     * without correct permissions.
     */
    this.expectFailure = opts.expectFailure || false;
  }
}

/**
 * Test function to copy from the specified source to the specified destination.
 * @param {TestEntryInfo} targetFile TestEntryInfo of target file to be copied.
 * @param {TransferLocationInfo} src The source of the transfer.
 * @param {TransferLocationInfo} dst The destination of the transfer.
 * @param {string|undefined} opt_dstExpectedDialogText The expected content of
 *     the transfer dialog (including any buttons), or undefined if no dialog is
 *     expected.
 */
function copyBetweenVolumes(targetFile, src, dst, opt_dstExpectedDialogText) {
  let appId;

  const localFiles = BASIC_LOCAL_ENTRY_SET;
  const driveFiles = (src.isTeamDrive || dst.isTeamDrive) ?
      TEAM_DRIVE_ENTRY_SET :
      BASIC_DRIVE_ENTRY_SET;
  let srcContents;
  if (src.isTeamDrive) {
    srcContents = TestEntryInfo
                      .getExpectedRows(src.initialEntries.filter(
                          entry => entry.type !== EntryType.TEAM_DRIVE &&
                              entry.teamDriveName === src.volumeName))
                      .sort();
  } else {
    srcContents = TestEntryInfo
                      .getExpectedRows(src.initialEntries.filter(
                          entry => entry.type !== EntryType.TEAM_DRIVE &&
                              entry.teamDriveName === ''))
                      .sort();
  }
  const myDriveContent = TestEntryInfo
                             .getExpectedRows(src.initialEntries.filter(
                                 entry => entry.type !== EntryType.TEAM_DRIVE &&
                                     entry.teamDriveName === ''))
                             .sort();

  let dstContents;
  if (dst.isTeamDrive) {
    dstContents = TestEntryInfo
                      .getExpectedRows(dst.initialEntries.filter(
                          entry => entry.type !== EntryType.TEAM_DRIVE &&
                              entry.teamDriveName === dst.volumeName))
                      .sort();
  } else {
    dstContents = TestEntryInfo
                      .getExpectedRows(dst.initialEntries.filter(
                          entry => entry.type !== EntryType.TEAM_DRIVE &&
                              entry.teamDriveName === ''))
                      .sort();
  }

  StepsRunner.run([
    // Set up File Manager.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, localFiles, driveFiles);
    },
    // Expand Drive root if either src or dst is within Drive.
    function(results) {
      appId = results.windowId;
      if (src.isTeamDrive || dst.isTeamDrive) {
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
          src.isTeamDrive ? 'selectTeamDrive' : 'selectVolume', appId,
          [src.volumeName], this.next);
    },
    // Wait for the expected files to appear in the file list.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, srcContents).then(this.next);
    },
    // Set focus on the file list.
    function() {
      remoteCall.callRemoteTestUtil(
          'focus', appId, ['#file-list:not([hidden])'], this.next);
    },
    // Select the source file.
    function() {
      remoteCall.callRemoteTestUtil(
          'selectFile', appId, [targetFile.nameText], this.next);
    },
    // Copy the file.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil('execCommand', appId, ['copy'], this.next);
    },
    // Select the destination volume.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil(
          dst.isTeamDrive ? 'selectTeamDrive' : 'selectVolume', appId,
          [dst.volumeName], this.next);
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
    function() {
      // If we're expecting a confirmation dialog, confirm that it is shown.
      if (opt_dstExpectedDialogText !== undefined) {
        return remoteCall.waitForElement(appId, '.cr-dialog-container.shown')
            .then(this.next);
      } else {
        setTimeout(this.next, 0);
      }
    },
    // Click OK if the dialog appears.
    function(element) {
      if (opt_dstExpectedDialogText !== undefined) {
        chrome.test.assertEq(opt_dstExpectedDialogText, element.text);
        // Press OK button.
        remoteCall
            .callRemoteTestUtil(
                'fakeMouseClick', appId, ['button.cr-dialog-ok'])
            .then(this.next);
      } else {
        setTimeout(this.next, 0);
      }
    },
    // Wait for the file list to change, if the test is expected to pass.
    function(result) {
      if (opt_dstExpectedDialogText !== undefined) {
        chrome.test.assertTrue(result);
      }

      const dstContentsAfterPaste = dstContents.slice();
      var ignoreFileSize = src.volumeName == 'drive_shared_with_me' ||
          src.volumeName == 'drive_offline' ||
          dst.volumeName == 'drive_shared_with_me' ||
          dst.volumeName == 'drive_offline';

      // If we expected the transfer to succeed, add the pasted file to the list
      // of expected rows.
      if (!dst.expectFailure) {
        var pasteFile = targetFile.getExpectedRow();
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
 * Tests copying from Drive to Downloads.
 */
testcase.transferFromDriveToDownloads = function() {
  copyBetweenVolumes(
      ENTRIES.hello,
      new TransferLocationInfo(
          {volumeName: 'drive', initialEntries: BASIC_DRIVE_ENTRY_SET}),
      new TransferLocationInfo(
          {volumeName: 'downloads', initialEntries: BASIC_LOCAL_ENTRY_SET}));
};

/**
 * Tests copying from Downloads to Drive.
 */
testcase.transferFromDownloadsToDrive = function() {
  copyBetweenVolumes(
      ENTRIES.hello,
      new TransferLocationInfo(
          {volumeName: 'downloads', initialEntries: BASIC_LOCAL_ENTRY_SET}),
      new TransferLocationInfo(
          {volumeName: 'drive', initialEntries: BASIC_DRIVE_ENTRY_SET}));
};

/**
 * Tests copying from Drive shared with me to Downloads.
 */
testcase.transferFromSharedToDownloads = function() {
  copyBetweenVolumes(
      ENTRIES.testSharedDocument, new TransferLocationInfo({
        volumeName: 'drive_shared_with_me',
        initialEntries: SHARED_WITH_ME_ENTRY_SET
      }),
      new TransferLocationInfo(
          {volumeName: 'downloads', initialEntries: BASIC_LOCAL_ENTRY_SET}));
};

/**
 * Tests copying from Drive shared with me to Drive.
 */
testcase.transferFromSharedToDrive = function() {
  copyBetweenVolumes(
      ENTRIES.testSharedDocument, new TransferLocationInfo({
        volumeName: 'drive_shared_with_me',
        initialEntries: SHARED_WITH_ME_ENTRY_SET
      }),
      new TransferLocationInfo(
          {volumeName: 'drive', initialEntries: BASIC_DRIVE_ENTRY_SET}));
};

/**
 * Tests copying from Drive offline to Downloads.
 */
testcase.transferFromOfflineToDownloads = function() {
  copyBetweenVolumes(
      ENTRIES.testDocument,
      new TransferLocationInfo(
          {volumeName: 'drive_offline', initialEntries: OFFLINE_ENTRY_SET}),
      new TransferLocationInfo(
          {volumeName: 'downloads', initialEntries: BASIC_LOCAL_ENTRY_SET}));
};

/**
 * Tests copying from Drive offline to Drive.
 */
testcase.transferFromOfflineToDrive = function() {
  copyBetweenVolumes(
      ENTRIES.testDocument,
      new TransferLocationInfo(
          {volumeName: 'drive_offline', initialEntries: OFFLINE_ENTRY_SET}),
      new TransferLocationInfo(
          {volumeName: 'drive', initialEntries: BASIC_DRIVE_ENTRY_SET}));
};

/**
 * Tests copying from a Team Drive to Drive.
 */
testcase.transferFromTeamDriveToDrive = function() {
  copyBetweenVolumes(
      ENTRIES.teamDriveAFile, new TransferLocationInfo({
        volumeName: 'Team Drive A',
        isTeamDrive: true,
        initialEntries: TEAM_DRIVE_ENTRY_SET
      }),
      new TransferLocationInfo(
          {volumeName: 'drive', initialEntries: TEAM_DRIVE_ENTRY_SET}));
};

/**
 * Tests copying from Drive to a Team Drive.
 */
testcase.transferFromDriveToTeamDrive = function() {
  copyBetweenVolumes(
      ENTRIES.hello,
      new TransferLocationInfo(
          {volumeName: 'drive', initialEntries: TEAM_DRIVE_ENTRY_SET}),
      new TransferLocationInfo({
        volumeName: 'Team Drive A',
        isTeamDrive: true,
        initialEntries: TEAM_DRIVE_ENTRY_SET
      }),
      'Members of \'Team Drive A\' will gain access to the copy of these ' +
          'items.CopyCancel');
};

/**
 * Tests copying from a Team Drive to Downloads.
 */
testcase.transferFromTeamDriveToDownloads = function() {
  copyBetweenVolumes(
      ENTRIES.teamDriveAFile, new TransferLocationInfo({
        volumeName: 'Team Drive A',
        isTeamDrive: true,
        initialEntries: TEAM_DRIVE_ENTRY_SET
      }),
      new TransferLocationInfo(
          {volumeName: 'downloads', initialEntries: BASIC_LOCAL_ENTRY_SET}));
};

/**
 * Tests that a gdoc file cannot be transferred from a Team Drive to a local
 * drive (e.g. Downloads).
 */
testcase.transferHostedFileFromTeamDriveToDownloads = function() {
  copyBetweenVolumes(
      ENTRIES.teamDriveAHostedFile, new TransferLocationInfo({
        volumeName: 'Team Drive A',
        isTeamDrive: true,
        initialEntries: TEAM_DRIVE_ENTRY_SET
      }),
      new TransferLocationInfo({
        volumeName: 'downloads',
        initialEntries: BASIC_LOCAL_ENTRY_SET,
        expectFailure: true
      }));
};

/**
 * Tests copying from Downloads to a Team Drive.
 */
testcase.transferFromDownloadsToTeamDrive = function() {
  copyBetweenVolumes(
      ENTRIES.hello,
      new TransferLocationInfo(
          {volumeName: 'downloads', initialEntries: BASIC_LOCAL_ENTRY_SET}),
      new TransferLocationInfo({
        volumeName: 'Team Drive A',
        isTeamDrive: true,
        initialEntries: TEAM_DRIVE_ENTRY_SET
      }),
      'Members of \'Team Drive A\' will gain access to the copy of these ' +
          'items.CopyCancel');
};

/**
 * Tests copying between two Team Drives.
 */
testcase.transferBetweenTeamDrives = function() {
  copyBetweenVolumes(
      ENTRIES.teamDriveBFile, new TransferLocationInfo({
        volumeName: 'Team Drive B',
        isTeamDrive: true,
        initialEntries: TEAM_DRIVE_ENTRY_SET
      }),
      new TransferLocationInfo({
        volumeName: 'Team Drive A',
        isTeamDrive: true,
        initialEntries: TEAM_DRIVE_ENTRY_SET
      }),
      'Members of \'Team Drive A\' will gain access to the copy of these ' +
          'items.CopyCancel');
};
