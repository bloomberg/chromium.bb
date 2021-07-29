// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {isSinglePartitionFormat, navigateWithDirectoryTree, remoteCall, setupAndWaitUntilReady, waitForMediaApp} from './background.js';
import {BASIC_DRIVE_ENTRY_SET, FILE_MANAGER_EXTENSIONS_ID, OFFLINE_ENTRY_SET, SHARED_WITH_ME_ENTRY_SET} from './test_data.js';

/**
 * Expected autocomplete results for 'hello'.
 * @type {Array<string>}
 * @const
 */
const EXPECTED_AUTOCOMPLETE_LIST = [
  '\'hello\' - search Drive',
  'hello.txt',
];

/**
 * Expected files shown in the search results for 'hello'
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
const SEARCH_RESULTS_ENTRY_SET = [
  ENTRIES.hello,
];

/**
 * Expected text shown in the Enable Docs Offline dialog.
 *
 * @type {string}
 * @const
 */
const ENABLE_DOCS_OFFLINE_MESSAGE =
    'Enable Google Docs Offline to make Docs, Sheets and Slides ' +
    'available offline.';

/**
 * Returns the steps to start a search for 'hello' and wait for the
 * autocomplete results to appear.
 */
async function startDriveSearchWithAutoComplete() {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));

  // Input a text.
  await remoteCall.inputText(appId, '#search-box cr-input', 'hello');

  // Notify the element of the input.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'input']));

  // Wait for the auto complete list getting the expected contents.
  const caller = getCaller();
  await repeatUntil(async () => {
    const elements = await remoteCall.callRemoteTestUtil(
        'queryAllElements', appId, ['#autocomplete-list li']);
    const list = elements.map((element) => element.text);
    return chrome.test.checkDeepEq(EXPECTED_AUTOCOMPLETE_LIST, list) ?
        undefined :
        pending(caller, 'Current auto complete list: %j.', list);
  });
  return appId;
}

/**
 * Opens the Enable Docs Offline dialog and waits for it to appear in the given
 * |appId| window.
 *
 * @param {string} appId
 */
async function openAndWaitForEnableDocsOfflineDialog(appId) {
  // Simulate Drive signalling Files App to open a dialog.
  await sendTestMessage({name: 'displayEnableDocsOfflineDialog'});

  // Check: the Enable Docs Offline dialog should appear.
  const dialogText = await remoteCall.waitForElement(
      appId, '.cr-dialog-container.shown .cr-dialog-text');
  chrome.test.assertEq(ENABLE_DOCS_OFFLINE_MESSAGE, dialogText.text);
}

/**
 * Waits for getLastDriveDialogResult to return the given |expectedResult|.
 *
 * @param {string} expectedResult
 */
async function waitForLastDriveDialogResult(expectedResult) {
  const caller = getCaller();
  await repeatUntil(async () => {
    const result = await sendTestMessage({name: 'getLastDriveDialogResult'});
    if (result !== expectedResult) {
      return pending(
          caller,
          'Waiting for getLastDriveDialogResult: expected %s, actual %s',
          expectedResult, result);
    }
  });
}

/**
 * Waits for a given notification to appear.
 *
 * @param {string} notification_id ID of notification to wait for.
 */
async function waitForNotification(notification_id) {
  const caller = getCaller();
  await repeatUntil(async () => {
    const idSet =
        await remoteCall.callRemoteTestUtil('getNotificationIDs', null, []);
    return !idSet[notification_id] ?
        pending(
            caller, 'Waiting for notification "%s" to appear.',
            notification_id) :
        null;
  });
}

/**
 * Tests opening the "Offline" on the sidebar navigation by clicking the icon,
 * and checks contents of the file list. Only the entries "available offline"
 * should be shown. "Available offline" entries are hosted documents and the
 * entries cached by DriveCache.
 */
testcase.driveOpenSidebarOffline = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Click the icon of the Offline volume.
  chrome.test.assertFalse(!await remoteCall.callRemoteTestUtil(
      'selectVolume', appId, ['drive_offline']));

  // Check: the file list should display the offline file set.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(OFFLINE_ENTRY_SET));
};

/**
 * Tests opening the "Shared with me" on the sidebar navigation by clicking the
 * icon, and checks contents of the file list. Only the entries labeled with
 * "shared-with-me" should be shown.
 */
testcase.driveOpenSidebarSharedWithMe = async () => {
  // Open Files app on Drive containing "Shared with me" file entries.
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.sharedWithMeDirectory,
        ENTRIES.sharedWithMeDirectoryFile,
      ]));

  // Click the icon of the Shared With Me volume.
  // Use the icon for a click target.
  chrome.test.assertFalse(!await remoteCall.callRemoteTestUtil(
      'selectVolume', appId, ['drive_shared_with_me']));

  // Wait until the breadcrumb path is updated.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Shared with me');

  // Verify the file list.
  await remoteCall.waitForFiles(
      appId,
      TestEntryInfo.getExpectedRows(
          SHARED_WITH_ME_ENTRY_SET.concat([ENTRIES.sharedWithMeDirectory])));

  // Navigate to the directory within Shared with me.
  chrome.test.assertFalse(!await remoteCall.callRemoteTestUtil(
      'openFile', appId, ['Shared Directory']));

  // Wait until the breadcrumb path is updated.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/Shared with me/Shared Directory');

  // Verify the file list.
  await remoteCall.waitForFiles(
      appId,
      TestEntryInfo.getExpectedRows([ENTRIES.sharedWithMeDirectoryFile]));
};

/**
 * Tests autocomplete with a query 'hello'.
 */
testcase.driveAutoCompleteQuery = async () => {
  return startDriveSearchWithAutoComplete();
};

/**
 * Tests that clicking the first option in the autocomplete box shows all of
 * the results for that query.
 */
testcase.driveClickFirstSearchResult = async () => {
  const appId = await startDriveSearchWithAutoComplete();
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId,
      ['#autocomplete-list', 'ArrowDown', false, false, false]));

  await remoteCall.waitForElement(appId, ['#autocomplete-list li[selected]']);
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeMouseDown', appId, ['#autocomplete-list li[selected]']));

  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(SEARCH_RESULTS_ENTRY_SET));

  // Fetch A11y messages.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq('Showing results for hello.', a11yMessages[0]);
};

/**
 * Tests that pressing enter after typing a search shows all of
 * the results for that query.
 */
testcase.drivePressEnterToSearch = async () => {
  const appId = await startDriveSearchWithAutoComplete();
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#search-box cr-input', 'focus']));
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId,
      ['#search-box cr-input', 'Enter', false, false, false]));
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(SEARCH_RESULTS_ENTRY_SET));

  // Fetch A11y messages.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq('Showing results for hello.', a11yMessages[0]);

  return appId;
};

/**
 * Tests that pressing the clear search button announces an a11y message and
 * shows all files/folders.
 */
testcase.drivePressClearSearch = async () => {
  const appId = await testcase.drivePressEnterToSearch();

  // Click on the clear search button.
  await remoteCall.waitAndClickElement(appId, '#search-box cr-input .clear');

  // Wait for fil list to display all files.
  await remoteCall.waitForFiles(
      appId, TestEntryInfo.getExpectedRows(BASIC_DRIVE_ENTRY_SET));

  // Check that a11y message for clearing the search term has been issued.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
  chrome.test.assertEq(2, a11yMessages.length, 'Missing a11y message');
  chrome.test.assertEq(
      'Search text cleared, showing all files and folders.', a11yMessages[1]);
};

/**
 * Tests that pinning multiple files affects the pin action of individual files.
 */
testcase.drivePinMultiple = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Select world.ogv.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="world.ogv"]');
  await remoteCall.waitForElement(appId, '[file-name="world.ogv"][selected]');

  // Open the context menu once the file is selected.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Check that the pin action is unticked, i.e. the action will pin the file.
  await remoteCall.waitForElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"]:not([checked])');

  // Additionally select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]', {shift: true});
  await remoteCall.waitForElement(appId, '[file-name="hello.txt"][selected]');

  // Open the context menu with both files selected.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Pin both files.
  await remoteCall.waitAndClickElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"]:not([checked])');

  // Wait for the toggle pinned async action to finish, so the next call to
  // display context menu is after the action has finished.
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');

  // Wait for the pinned action to finish, it's flagged in the file list by
  // removing CSS class "dim-offline" and adding class "pinned".
  await remoteCall.waitForElementLost(
      appId, '#file-list .dim-offline[file-name="world.ogv"]');
  await remoteCall.waitForElement(
      appId, '#file-list .pinned[file-name="world.ogv"] .detail-pinned');

  // Select world.ogv by itself.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="world.ogv"]');

  // Wait for hello.txt to be unselected.
  await remoteCall.waitForElement(
      appId, '[file-name="hello.txt"]:not([selected])');

  // Open the context menu for world.ogv.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Check that the pin action is ticked, i.e. the action will unpin the file.
  await remoteCall.waitForElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"][checked]');
};

/**
 * Tests that pinning hosted files without the required extensions is disabled,
 * and that it does not affect multiple selections with non-hosted files.
 */
testcase.drivePinHosted = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Select Test Document.gdoc.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="Test Document.gdoc"]');
  await remoteCall.waitForElement(
      appId, '[file-name="Test Document.gdoc"][selected]');

  // Open the context menu once the file is selected.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Check that the pin action is disabled and unticked.
  await remoteCall.waitForElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"][disabled]:not([checked])');

  // Additionally select hello.txt.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]', {shift: true});
  await remoteCall.waitForElement(appId, '[file-name="hello.txt"][selected]');

  // Open the context menu with both files selected.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // The pin action should be enabled to pin only hello.txt, so select it.
  await remoteCall.waitAndClickElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"]:not([checked]):not([disabled])');

  // Wait for the toggle pinned async action to finish, so the next call to
  // display context menu is after the action has finished.
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');

  // Wait for the pinned action to finish, it's flagged in the file list by
  // removing CSS class "dim-offline" and adding class "pinned".
  await remoteCall.waitForElementLost(
      appId, '#file-list .dim-offline[file-name="hello.txt"]');
  await remoteCall.waitForElement(
      appId, '#file-list .pinned[file-name="hello.txt"] .detail-pinned');

  // Test Document.gdoc should not be pinned however.
  await remoteCall.waitForElement(
      appId, '#file-list [file-name="Test Document.gdoc"]:not(.pinned)');


  // Open the context menu with both files selected.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Check that the pin action is ticked, i.e. the action will unpin the file.
  await remoteCall.waitForElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"][checked]:not([disabled])');
};

/**
 * Tests pinning a file to a mobile network.
 */
testcase.drivePinFileMobileNetwork = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);
  const caller = getCaller();
  await sendTestMessage({name: 'useCellularNetwork'});
  await remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt']);
  await repeatUntil(() => {
    return navigator.connection.type != 'cellular' ?
        pending(caller, 'Network state is not changed to cellular.') :
        null;
  });
  await remoteCall.waitForElement(appId, ['.table-row[selected]']);
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['.table-row[selected]']));

  // Wait for the menu to appear and click on toggle pinned.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');
  await remoteCall.waitAndClickElement(
      appId, '[command="#toggle-pinned"]:not([hidden]):not([disabled])');

  // Wait for the toggle pinned async action to finish, so the next call to
  // display context menu is after the action has finished.
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');

  // Wait for the pinned action to finish, it's flagged in the file list by
  // removing CSS class "dim-offline".
  await remoteCall.waitForElementLost(
      appId, '#file-list .dim-offline[file-name="hello.txt"]');


  // Open context menu again.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['#file-list', 'contextmenu']));

  // Check: File is pinned.
  await remoteCall.waitForElement(appId, '[command="#toggle-pinned"][checked]');
  await remoteCall.waitForElement(
      appId, '#file-list .pinned[file-name="hello.txt"] .detail-pinned');
  await waitForNotification('disabled-mobile-sync');
  await sendTestMessage({
    name: 'clickNotificationButton',
    extensionId: FILE_MANAGER_EXTENSIONS_ID,
    notificationId: 'disabled-mobile-sync',
    index: 0
  });
  await repeatUntil(async () => {
    const preferences =
        await remoteCall.callRemoteTestUtil('getPreferences', null, []);
    return preferences.cellularDisabled ?
        pending(caller, 'Drive sync is still disabled.') :
        null;
  });
};

/**
 * Tests that the pinned toggle in the toolbar updates on pinned state changes
 * within fake entries.
 */
testcase.drivePinToggleUpdatesInFakeEntries = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Navigate to the Offline fake entry.
  await navigateWithDirectoryTree(appId, '/Offline');

  // Bring up the context menu for test.txt.
  await remoteCall.waitAndRightClick(
      appId, '#file-list [file-name="test.txt"]');

  // The pinned toggle should update to be checked.
  await remoteCall.waitForElement(appId, '#pinned-toggle[checked]');

  // Unpin the file.
  await remoteCall.waitAndClickElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"][checked]');

  // The pinned toggle should change to be unchecked.
  await remoteCall.waitForElement(appId, '#pinned-toggle:not([checked])');

  // Navigate to the Shared with me fake entry.
  await navigateWithDirectoryTree(appId, '/Shared with me');

  // Bring up the context menu for test.txt.
  await remoteCall.waitAndRightClick(
      appId, '#file-list [file-name="test.txt"]');

  // The pinned toggle should remain unchecked.
  await remoteCall.waitForElement(appId, '#pinned-toggle:not([checked])');

  // Pin the file.
  await remoteCall.waitAndClickElement(
      appId,
      '#file-context-menu:not([hidden]) ' +
          '[command="#toggle-pinned"]:not([checked])');

  // The pinned toggle should change to be checked.
  await remoteCall.waitForElement(appId, '#pinned-toggle[checked]');
};

/**
 * Tests that pressing Ctrl+A (select all files) from the search box doesn't put
 * the Files App into check-select mode (crbug.com/849253).
 */
testcase.drivePressCtrlAFromSearch = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Focus the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#search-button']));

  // Wait for the search box to be visible.
  await remoteCall.waitForElement(
      appId, ['#search-box cr-input:not([hidden])']);

  // Press Ctrl+A inside the search box.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeKeyDown', appId, ['#search-box cr-input', 'A', true, false, false]));

  // Check we didn't enter check-select mode.
  await remoteCall.waitForElement(appId, ['body:not(.check-select)']);
};

// Match the way the production version formats dates.
function formatDate(date) {
  const padAndConvert = i => {
    return (i < 10 ? '0' : '') + i.toString();
  };

  const year = date.getFullYear().toString();
  // Months are 0-based, but days aren't.
  const month = padAndConvert(date.getMonth() + 1);
  const day = padAndConvert(date.getDate());

  return `${year}-${month}-${day}`;
}

/**
 * Test that a images within a DCIM directory on removable media is backed up to
 * Drive, in the Chrome OS Cloud backup/<current date> directory.
 */
testcase.driveBackupPhotos = async () => {
  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

  // Open Files app on local downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Mount USB volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeUsbDcim'});

  // Wait for the USB mount.
  await remoteCall.waitForElement(appId, USB_VOLUME_QUERY);

  if (await isSinglePartitionFormat(appId)) {
    // Navigate to the DCIM directory.
    await navigateWithDirectoryTree(appId, '/FAKEUSB/fake-usb/DCIM');
  } else {
    // Navigate to the DCIM directory.
    await remoteCall.navigateWithDirectoryTree(
        appId, '/DCIM', 'fake-usb', 'removable');
  }

  // Wait for the import button to be ready.
  await remoteCall.waitForElement(
      appId, '#cloud-import-button [icon="files:cloud-upload"]');

  // Start the import.
  const date = new Date();
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#cloud-import-button']));

  // Wait for the image to be marked as imported.
  await remoteCall.waitForElement(
      appId, '.status-icon[file-status-icon="imported"]');

  // Navigate to today's backup directory in Drive.
  const formattedDate = formatDate(date);
  await remoteCall.navigateWithDirectoryTree(
      appId, `/root/Chrome OS Cloud backup/${formattedDate}`, 'My Drive',
      'drive');

  // Verify the backed-up file list contains only a copy of the image within
  // DCIM in the removable storage.
  const files = TestEntryInfo.getExpectedRows([ENTRIES.image3]);
  await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});
};

/**
 * Verify that "Available Offline" is not available from the gear menu for a
 * drive file.
 */
testcase.driveAvailableOfflineGearMenu = async () => {
  const pinnedMenuQuery = '#file-context-menu:not([hidden]) ' +
      'cr-menu-item[command="#toggle-pinned"]:not([disabled])';

  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Select a file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['hello.txt']),
      'selectFile failed');

  // Wait for the entry to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Click on the icon of the file to check select it
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['#file-list .table-row[selected] .detail-checkmark']);

  // Ensure gear button is available
  await remoteCall.waitForElement(appId, '#selection-menu-button');

  // Click on gear menu and ensure "Available Offline" is not shown.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#selection-menu-button']));

  // Check that "Available Offline" is not shown in the menu. This element is
  // hidden via a display:none css rule, so check that.
  const e = await remoteCall.waitForElementStyles(
      appId, pinnedMenuQuery, ['display']);
  chrome.test.assertEq('none', e.styles.display);
};

/**
 * Verify that "Available Offline" is not available from the gear menu for a
 * drive directory.
 */
testcase.driveAvailableOfflineDirectoryGearMenu = async () => {
  const pinnedMenuQuery = '#file-context-menu:not([hidden]) ' +
      'cr-menu-item[command="#toggle-pinned"]:not([disabled])';

  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Select a file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['photos']),
      'selectFile failed');

  // Wait for the entry to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Click on the icon of the file to check select it
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['#file-list .table-row[selected] .detail-checkmark']);

  // Ensure gear button is available
  await remoteCall.waitForElement(appId, '#selection-menu-button');

  // Click on gear menu and ensure "Available Offline" is not shown.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#selection-menu-button']));

  // Check that "Available Offline" is not shown in the menu. This element is
  // hidden via a display:none css rule, so check that.
  const e = await remoteCall.waitForElementStyles(
      appId, pinnedMenuQuery, ['display']);
  chrome.test.assertEq('none', e.styles.display);
};

/**
 * Verify that the "Available Offline" toggle in the action bar appears and
 * changes according to the selection.
 */
testcase.driveAvailableOfflineActionBar = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Check the "Available Offline" toggle is currently hidden as no file is
  // currently selected.
  await remoteCall.waitForElement(
      appId, '#action-bar #pinned-toggle-wrapper[hidden]');

  // Select a hosted file.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="Test Document.gdoc"]');

  // Wait for the entry to be selected.
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Check the "Available Offline" toggle is shown in the action bar, but
  // disabled.
  await remoteCall.waitForElement(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle[disabled]:not([checked])');

  // Now select a non-hosted file.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Check the "Available Offline" toggle is now enabled, and pin the file.
  await remoteCall.waitAndClickElement(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle:not([disabled]):not([checked])');

  // Wait for the file to be pinned.
  await remoteCall.waitForElement(
      appId, '#file-list .pinned[file-name="hello.txt"]');

  // Check the "Available Offline" toggle is enabled and checked.
  await remoteCall.waitForElement(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle[checked]:not([disabled])');

  // Select another file that is not pinned.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="world.ogv"]');

  // Check the "Available Offline" toggle is enabled and unchecked.
  await remoteCall.waitForElement(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle:not([disabled]):not([checked])');

  // Reselect the previously pinned file.
  await remoteCall.waitAndClickElement(
      appId, '#file-list [file-name="hello.txt"]');

  // Check the "Available Offline" toggle is enabled and checked.
  await remoteCall.waitForElement(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle[checked]:not([disabled])');

  // Focus on the directory tree.
  await remoteCall.focus(appId, ['#directory-tree']);

  // Check the "Available Offline" toggle is still available in the action bar.
  await remoteCall.waitForElement(
      appId,
      '#action-bar #pinned-toggle-wrapper:not([hidden]) ' +
          '#pinned-toggle[checked]:not([disabled])');
};

/**
 * Tests following links to folders.
 */
testcase.driveLinkToDirectory = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.directoryA,
        ENTRIES.directoryB,
        ENTRIES.directoryC,
        ENTRIES.deeplyBurriedSmallJpeg,
        ENTRIES.linkGtoB,
        ENTRIES.linkHtoFile,
        ENTRIES.linkTtoTransitiveDirectory,
      ]));

  // Select the link
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('selectFile', appId, ['G']),
      'selectFile failed');
  await remoteCall.waitForElement(appId, '.table-row[selected]');

  // Open the link
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['G']));

  // Check the contents of current directory.
  await remoteCall.waitForFiles(appId, [ENTRIES.directoryC.getExpectedRow()]);
};

/**
 * Tests opening files through folder links.
 */
testcase.driveLinkOpenFileThroughLinkedDirectory = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.directoryA,
        ENTRIES.directoryB,
        ENTRIES.directoryC,
        ENTRIES.deeplyBurriedSmallJpeg,
        ENTRIES.linkGtoB,
        ENTRIES.linkHtoFile,
        ENTRIES.linkTtoTransitiveDirectory,
      ]));

  // Navigate through link.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['G']));
  await remoteCall.waitForFiles(appId, [ENTRIES.directoryC.getExpectedRow()]);
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['C']));
  await remoteCall.waitForFiles(
      appId, [ENTRIES.deeplyBurriedSmallJpeg.getExpectedRow()]);

  await sendTestMessage(
      {name: 'expectFileTask', fileNames: ['deep.jpg'], openType: 'launch'});
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['deep.jpg']));

  // The MediaApp window should open for the image.
  await waitForMediaApp();
};

/**
 * Tests opening files through transitive links.
 */
testcase.driveLinkOpenFileThroughTransitiveLink = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET.concat([
        ENTRIES.directoryA,
        ENTRIES.directoryB,
        ENTRIES.directoryC,
        ENTRIES.deeplyBurriedSmallJpeg,
        ENTRIES.linkGtoB,
        ENTRIES.linkHtoFile,
        ENTRIES.linkTtoTransitiveDirectory,
      ]));

  // Navigate through link.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['T']));
  await remoteCall.waitForFiles(
      appId, [ENTRIES.deeplyBurriedSmallJpeg.getExpectedRow()]);

  await sendTestMessage(
      {name: 'expectFileTask', fileNames: ['deep.jpg'], openType: 'launch'});
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, ['deep.jpg']));

  // The MediaApp window should open for the image.
  await waitForMediaApp();
};

/**
 * Tests that the welcome banner appears when a Drive volume is opened.
 */
testcase.driveWelcomeBanner = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Open the Drive volume in the files-list.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.drive-volume']));

  // Check: the Drive welcome banner should appear.
  await remoteCall.waitForElement(appId, '.drive-welcome-wrapper');

  // Close the Drive welcome banner.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['cr-button.banner-close']));

  // Check: the Drive banner should close.
  const caller = getCaller();
  await repeatUntil(async () => {
    const banner = await remoteCall.waitForElementStyles(
        appId, '.drive-welcome', ['visibility']);

    if (banner.styles.visibility !== 'hidden') {
      return pending(caller, 'Welcome banner is still visible.');
    }
  });
};

/**
 * Tests that the Drive offline info banner appears when a Drive volume is
 * opened.
 */
testcase.driveOfflineInfoBanner = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Check: the Drive Offline info banner should appear.
  await remoteCall.waitForElement(appId, '#offline-info-banner:not([hidden])');

  // Click on the 'Learn more' button.
  await remoteCall.waitAndClickElement(appId, '#offline-learn-more');

  // Check: the Drive offline info banner should disappear.
  await remoteCall.waitForElement(appId, '#offline-info-banner[hidden]');

  // Navigate to a different directory within Drive.
  await navigateWithDirectoryTree(appId, '/My Drive/photos');

  // Check: the Drive offline info banner should stay hidden.
  await remoteCall.waitForElement(appId, '#offline-info-banner[hidden]');
};

/**
 * Tests that the Drive offline info banner does not show when the
 * DriveFsBidirectionalNativeMessaging flag is disabled.
 */
testcase.driveOfflineInfoBannerWithoutFlag = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Check: the Drive Offline info banner should not appear.
  await remoteCall.waitForElementLost(
      appId, '#offline-info-banner:not([hidden])');
};

/**
 * Tests that the Enable Docs Offline dialog appears in the Files App.
 */
testcase.driveEnableDocsOfflineDialog = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Open the Enable Docs Offline dialog.
  await openAndWaitForEnableDocsOfflineDialog(appId);

  // Click on the ok button.
  await remoteCall.waitAndClickElement(
      appId, '.cr-dialog-container.shown .cr-dialog-ok');

  // Check: the last dialog result should be 1 (accept).
  await waitForLastDriveDialogResult('1');

  // Open the Enable Docs Offline dialog.
  await openAndWaitForEnableDocsOfflineDialog(appId);

  // Click on the cancel button.
  await remoteCall.waitAndClickElement(
      appId, '.cr-dialog-container.shown .cr-dialog-cancel');

  // Check: the last dialog result should be 2 (reject).
  await waitForLastDriveDialogResult('2');

  // Open the Enable Docs Offline dialog.
  await openAndWaitForEnableDocsOfflineDialog(appId);

  // Close Files App.
  await remoteCall.closeWindowAndWait(appId);

  // Check: the last dialog result should be 3 (dismiss).
  await waitForLastDriveDialogResult('3');
};

/**
 * Tests that the Enable Docs Offline dialog launches a Chrome notification if
 * there are no Files App windows open.
 */
testcase.driveEnableDocsOfflineDialogWithoutWindow = async () => {
  // Wait for the background page to listen to events from the browser.
  await remoteCall.callRemoteTestUtil('waitForBackgroundReady', null, []);

  // Simulate Drive signalling Files App to open a dialog.
  await sendTestMessage({name: 'displayEnableDocsOfflineDialog'});

  // Check: the Enable Docs Offline notification should appear.
  await waitForNotification('enable-docs-offline');

  // Click on the ok button.
  await sendTestMessage({
    name: 'clickNotificationButton',
    extensionId: FILE_MANAGER_EXTENSIONS_ID,
    notificationId: 'enable-docs-offline',
    index: 1
  });

  // Check: the last dialog result should be 1 (accept).
  await waitForLastDriveDialogResult('1');

  // Simulate Drive signalling Files App to open a dialog.
  await sendTestMessage({name: 'displayEnableDocsOfflineDialog'});

  // Check: the Enable Docs Offline notification should appear.
  await waitForNotification('enable-docs-offline');

  // Click on the cancel button.
  await sendTestMessage({
    name: 'clickNotificationButton',
    extensionId: FILE_MANAGER_EXTENSIONS_ID,
    notificationId: 'enable-docs-offline',
    index: 0
  });

  // Check: the last dialog result should be 2 (reject).
  await waitForLastDriveDialogResult('2');

  // Simulate Drive signalling Files App to open a dialog.
  await sendTestMessage({name: 'displayEnableDocsOfflineDialog'});

  // Check: the Enable Docs Offline notification should appear.
  await waitForNotification('enable-docs-offline');

  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Check: the Enable Docs Offline dialog should appear in Files app.
  const dialogText = await remoteCall.waitForElement(
      appId, '.cr-dialog-container.shown .cr-dialog-text');
  chrome.test.assertEq(ENABLE_DOCS_OFFLINE_MESSAGE, dialogText.text);

  // Check: the Enable Docs Offline notification should disappear.
  const caller = getCaller();
  await repeatUntil(async () => {
    const idSet =
        await remoteCall.callRemoteTestUtil('getNotificationIDs', null, []);
    return idSet['enable-docs-offline'] ?
        pending(
            caller, 'Waiting for Drive confirm notification to disappear.') :
        null;
  });
};

/**
 * Tests that the Enable Docs Offline dialog appears in the focused window if
 * there are more than one Files App windows open.
 */
testcase.driveEnableDocsOfflineDialogMultipleWindows = async () => {
  // Open two Files app windows on Drive, and the second one should be focused.
  const appId1 = await setupAndWaitUntilReady(RootPath.DRIVE, []);
  const appId2 = await setupAndWaitUntilReady(RootPath.DRIVE, []);

  // Open the Enable Docs Offline dialog and check that it appears in the second
  // window.
  await openAndWaitForEnableDocsOfflineDialog(appId2);

  // Check: there should be no dialog shown in the first window.
  await remoteCall.waitForElementLost(appId1, '.cr-dialog-container.shown');

  // Click on the ok button in the second window.
  await remoteCall.waitAndClickElement(
      appId2, '.cr-dialog-container.shown .cr-dialog-ok');

  // Check: the last dialog result should be 1 (accept).
  await waitForLastDriveDialogResult('1');
};

/**
 * Tests that the Enable Docs Offline dialog disappears when Drive is unmounted.
 */
testcase.driveEnableDocsOfflineDialogDisappearsOnUnmount = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Open the Enable Docs Offline dialog.
  await openAndWaitForEnableDocsOfflineDialog(appId);

  // Unmount Drive.
  await sendTestMessage({name: 'unmountDrive'});

  // Check: the Enable Docs Offline dialog should disappear.
  await remoteCall.waitForElementLost(appId, '.cr-dialog-container.shown');
};
