// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
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
})();
