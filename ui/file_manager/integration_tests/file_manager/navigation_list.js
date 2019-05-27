// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
(() => {
  /**
   * Tests that the directory tree area can be horizontally scrolled.
   * TODO(crbug.com/966807) add a vertical scroll case.
   */
  testcase.navigationScrollsWhenClipped = async () => {
    // Open FilesApp with the Downloads folder visible.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
    await expandRoot(appId, TREEITEM_DOWNLOADS);

    // Get the navigationList width property and make sure it has non-zero size.
    const navigationList = '.dialog-navigation-list';
    let list =
        await remoteCall.waitForElementStyles(appId, navigationList, ['width']);
    const originalWidth = Number(list.styles['width'].match(/[0-9]*/));
    chrome.test.assertTrue(originalWidth > 0);

    // Shrink the navigation list tree area to 100px.
    await remoteCall.callRemoteTestUtil(
        'setElementStyles', appId, [navigationList, {width: '50px'}]);

    // Check the navigation list area is scrolled completely to the left side.
    chrome.test.assertTrue(list.scrollLeft == 0);

    // Attempt to scroll the navigation list area to the right.
    await remoteCall.callRemoteTestUtil(
        'setScrollLeft', appId, [navigationList, 100]);

    // Get the current state of the NavigationWidth width and left scroll
    // offset.
    list =
        await remoteCall.waitForElementStyles(appId, navigationList, ['width']);

    // Check that the width has been reduced.
    const newWidth = Number(list.styles['width'].match(/[0-9]*/));
    chrome.test.assertTrue(newWidth < originalWidth);

    // Check the navigation list area has scrolled.
    chrome.test.assertTrue(list.scrollLeft > 0);
  };
})();
