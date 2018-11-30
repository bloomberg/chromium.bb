// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Sends a key event to an open file dialog, after selecting the file |name|
 * entry in the file list.
 *
 * @param {!string} name File name shown in the dialog.
 * @param {!Array} key Key detail for fakeKeyDown event.
 * @param {!string} dialog ID of the file dialog window.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function sendOpenFileDialogKey(name, key, dialog) {
  await remoteCall.callRemoteTestUtil('selectFile', dialog, [name]);
  await remoteCall.callRemoteTestUtil('fakeKeyDown', dialog, key);
}

/**
 * Clicks a button in the open file dialog, after selecting the file |name|
 * entry in the file list and checking that |button| exists.
 *
 * @param {!string} name File name shown in the dialog.
 * @param {!string} button Selector of the dialog button.
 * @param {!string} dialog ID of the file dialog window.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function clickOpenFileDialogButton(name, button, dialog) {
  await remoteCall.callRemoteTestUtil('selectFile', dialog, [name]);
  await remoteCall.waitForElement(dialog, button);
  const event = [button, 'click'];
  await remoteCall.callRemoteTestUtil('fakeEvent', dialog, event);
}

/**
 * Sends an unload event to an open file dialog (after it is drawn) causing
 * the dialog to shut-down and close.
 *
 * @param {!string} dialog ID of the file dialog window.
 * @param {string} element Element to query for drawing.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function unloadOpenFileDialog(
    dialog, element = '.button-panel button.ok') {
  await remoteCall.waitForElement(dialog, element);
  await remoteCall.callRemoteTestUtil('unload', dialog, []);
  const errorCount =
      await remoteCall.callRemoteTestUtil('getErrorCount', dialog, []);
  chrome.test.assertEq(0, errorCount);
}

/**
 * Adds basic file entry sets for both 'local' and 'drive', and returns the
 * entry set of the given |volume|.
 *
 * @param {!string} volume Name of the volume.
 * @return {!Promise} Promise to resolve({Array<TestEntryInfo>}) on success,
 *    the Array being the basic file entry set of the |volume|.
 */
async function setUpFileEntrySet(volume) {
  let localEntryPromise = addEntries(['local'], BASIC_LOCAL_ENTRY_SET);
  let driveEntryPromise = addEntries(
      ['drive'], [ENTRIES.hello, ENTRIES.pinned, ENTRIES.testDocument]);

  await Promise.all([localEntryPromise, driveEntryPromise]);
  if (volume == 'drive')
    return [ENTRIES.hello, ENTRIES.pinned, ENTRIES.testDocument];
  return BASIC_LOCAL_ENTRY_SET;
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and click the Ok button.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function openFileDialogClickOkButton(
    volume, name, expectedUrl = undefined) {
  const type = {type: 'openFile'};

  const okButton = '.button-panel button.ok:enabled';
  let closer = clickOpenFileDialogButton.bind(null, name, okButton);

  const entrySet = await setUpFileEntrySet(volume);
  const result = await openAndWaitForClosingDialog(
      type, volume, entrySet, closer, !!expectedUrl);
  if (expectedUrl) {
    chrome.test.assertEq(expectedUrl, result);
  } else {
    chrome.test.assertEq(name, result.name);
  }
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and verify that the Ok button is
 * disabled.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function openFileDialogExpectOkButtonDisabled(volume, name, pinnedName) {
  const type = {type: 'openFile'};

  const okButton = '.button-panel button.ok:enabled';
  const disabledOkButton = '.button-panel button.ok:disabled';
  const cancelButton = '.button-panel button.cancel';
  let closer = async (dialog) => {
    await remoteCall.callRemoteTestUtil('selectFile', dialog, [pinnedName]);
    await remoteCall.waitForElement(dialog, okButton);
    await remoteCall.callRemoteTestUtil('selectFile', dialog, [name]);
    await remoteCall.waitForElement(dialog, disabledOkButton);
    clickOpenFileDialogButton(name, cancelButton, dialog);
  };

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(type, volume, entrySet, closer));
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and click the Cancel button.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function openFileDialogClickCancelButton(volume, name) {
  const type = {type: 'openFile'};

  const cancelButton = '.button-panel button.cancel';
  let closer = clickOpenFileDialogButton.bind(null, name, cancelButton);

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(type, volume, entrySet, closer));
}

/**
 * Adds the basic file entry sets then opens the file dialog on the volume.
 * Once file |name| is shown, select it and send an Escape key.
 *
 * @param {!string} volume Volume name for openAndWaitForClosingDialog.
 * @param {!string} name File name to select in the dialog.
 * @return {!Promise} Promise to be fulfilled on success.
 */
async function openFileDialogSendEscapeKey(volume, name) {
  const type = {type: 'openFile'};

  const escapeKey = ['#file-list', 'Escape', false, false, false];
  let closer = sendOpenFileDialogKey.bind(null, name, escapeKey);

  const entrySet = await setUpFileEntrySet(volume);
  chrome.test.assertEq(
      undefined,
      await openAndWaitForClosingDialog(type, volume, entrySet, closer));
}

/**
 * Test file present in Downloads.
 * @{!string}
 */
const TEST_LOCAL_FILE = BASIC_LOCAL_ENTRY_SET[0].targetPath;

/**
 * Tests opening file dialog on Downloads and closing it with Ok button.
 */
testcase.openFileDialogDownloads = function() {
  return openFileDialogClickOkButton('downloads', TEST_LOCAL_FILE);
};

/**
 * Tests opening file dialog on Downloads and closing it with Cancel button.
 */
testcase.openFileDialogCancelDownloads = function() {
  return openFileDialogClickCancelButton('downloads', TEST_LOCAL_FILE);
};

/**
 * Tests opening file dialog on Downloads and closing it with ESC key.
 */
testcase.openFileDialogEscapeDownloads = function() {
  return openFileDialogSendEscapeKey('downloads', TEST_LOCAL_FILE);
};

/**
 * Test file present in Drive only.
 * @{!string}
 */
const TEST_DRIVE_FILE = ENTRIES.hello.targetPath;

/**
 * Test file present in Drive only.
 * @{!string}
 */
const TEST_DRIVE_PINNED_FILE = ENTRIES.pinned.targetPath;

/**
 * Tests opening file dialog on Drive and closing it with Ok button.
 */
testcase.openFileDialogDrive = function() {
  return openFileDialogClickOkButton('drive', TEST_DRIVE_FILE);
};

/**
 * Tests opening file dialog on Drive and closing it with Ok button.
 */
testcase.openFileDialogDriveOffline = function() {
  return openFileDialogExpectOkButtonDisabled(
      'drive', TEST_DRIVE_FILE, TEST_DRIVE_PINNED_FILE);
};

/**
 * Tests opening file dialog on Drive and closing it with Ok button.
 */
testcase.openFileDialogDriveOfflinePinned = function() {
  return openFileDialogClickOkButton('drive', TEST_DRIVE_PINNED_FILE);
};

/**
 * Tests opening a hosted doc in the browser, ensuring it correctly navigates to
 * the doc's URL.
 */
testcase.openFileDialogDriveHostedDoc = function() {
  return openFileDialogClickOkButton(
      'drive', ENTRIES.testDocument.nameText,
      'https://document_alternate_link/Test%20Document');
};

/**
 * Tests opening file dialog on Drive and closing it with Cancel button.
 */
testcase.openFileDialogCancelDrive = function() {
  return openFileDialogClickCancelButton('drive', TEST_DRIVE_FILE);
};

/**
 * Tests opening file dialog on Drive and closing it with ESC key.
 */
testcase.openFileDialogEscapeDrive = function() {
  return openFileDialogSendEscapeKey('drive', TEST_DRIVE_FILE);
};

/**
 * Tests opening file dialog, then closing it with an 'unload' event.
 */
testcase.openFileDialogUnload = async function() {
  chrome.fileSystem.chooseEntry({type: 'openFile'}, (entry) => {});
  const dialog = await remoteCall.waitForWindow('dialog#');
  await unloadOpenFileDialog(dialog);
};
