// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  /**
   * Send Tab keys to Files app until #file-list gets a selected item.
   *
   * Raise test failure if #file-list doesn't get anything selected in 20 tabs.
   *
   * NOTE:
   *   1. Sends a maximum of 20 tabs.
   *   2. The focused element after this function might NOT be #file-list, since
   *   updating the selected item which is async can be detected after an extra
   *   Tab has been sent.
   */
  async function tabUntilFileSelected(appId) {
    // Check: the file-list should have nothing selected.
    const selectedRows = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, ['#file-list li[selected]']);
    chrome.test.assertEq(0, selectedRows.length);

    // Press the tab until file-list gets focus and select some file.
    for (let i = 0; i < 20; i++) {
      // Send Tab key.
      const result = await sendTestMessage({name: 'dispatchTabKey'});
      chrome.test.assertEq(
          result, 'tabKeyDispatched', 'Tab key dispatch failed');

      // Check if there is a file selected, and return if so.
      const selectedRows = await remoteCall.callRemoteTestUtil(
          'deepQueryAllElements', appId, ['#file-list li[selected]']);
      if (selectedRows.length > 0) {
        return;
      }
    }

    chrome.test.assertTrue(false, 'File list has no selection');
  }

  /**
   * Tests that file list column header have ARIA attributes.
   */
  testcase.fileListAriaAttributes = async () => {
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

    // Fetch column header.
    const columnHeadersQuery =
        ['#detail-table .table-header [aria-describedby]'];
    const columnHeaders = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [columnHeadersQuery, ['display']]);

    chrome.test.assertTrue(columnHeaders.length > 0);
    for (const header of columnHeaders) {
      // aria-describedby is used to tell users that they can click and which
      // type of sort asc/desc will happen.
      chrome.test.assertTrue('aria-describedby' in header.attributes);
      // role button is used so users know that it's clickable.
      chrome.test.assertEq('button', header.attributes.role);
    }
  };

  /**
   * Tests using tab to focus the file list will select the first item, if
   * nothing is selected.
   */
  testcase.fileListFocusFirstItem = async () => {
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

    // Send Tab keys to make file selected.
    await tabUntilFileSelected(appId);

    // Check: The first file only is selected in the file entry rows.
    const fileRows = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, ['#file-list li']);
    chrome.test.assertEq(5, fileRows.length);
    const selectedRows = fileRows.filter(item => 'selected' in item.attributes);
    chrome.test.assertEq(1, selectedRows.length);
    chrome.test.assertEq(0, fileRows.indexOf(selectedRows[0]));
  };

  /**
   * Tests that after a multiple selection, canceling the selection and using
   * Tab to focus the files list it selects the item that was last focused.
   */
  testcase.fileListSelectLastFocusedItem = async () => {
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

    // Check: the file-list should have nothing selected.
    let selectedRows = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, ['#file-list li[selected]']);
    chrome.test.assertEq(0, selectedRows.length);

    // Move to item 2.
    const downKey = ['#file-list', 'ArrowDown', false, false, false];
    for (let i = 0; i < 2; i++) {
      chrome.test.assertTrue(
          !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, downKey),
          'ArrowDown failed');
    }

    // Select item 2 with Ctrl+Space.
    const ctrlSpace = ['#file-list', ' ', true, false, false];
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
        'Ctrl+Space failed');

    // Move to item 3 and Cltr+Space to add it to multi-selection.
    const ctrlDown = ['#file-list', 'ArrowDown', true, false, false];
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlDown),
        'Ctrl+ArrowDown failed');
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlSpace),
        'Ctrl+Space failed');

    // Check that items 2 and 3 are selected.
    selectedRows = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, ['#file-list li[selected]']);
    chrome.test.assertEq(2, selectedRows.length);

    // Cancel the selection.
    const button = ['#cancel-selection-button'];
    await remoteCall.waitAndClickElement(appId, '#cancel-selection-button');

    // Wait until selection is removed.
    await remoteCall.waitForElementLost(appId, '#file-list li[selected]');

    // Send Tab keys until a file item is selected.
    await tabUntilFileSelected(appId);

    // Check: The 3rd item only is selected.
    const fileRows = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, ['#file-list li']);
    chrome.test.assertEq(5, fileRows.length);
    selectedRows = fileRows.filter(item => 'selected' in item.attributes);
    chrome.test.assertEq(1, selectedRows.length);
    chrome.test.assertEq(2, fileRows.indexOf(selectedRows[0]));
  };
})();
