// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests that breadcrumbs work.
 */

'use strict';

(() => {
  testcase.breadcrumbsNavigate = async () => {
    const files = [ENTRIES.hello, ENTRIES.photos];
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

    // Navigate to Downloads/photos.
    await remoteCall.navigateWithDirectoryTree(
        appId, RootPath.DOWNLOADS_PATH + '/photos', 'My files/Downloads');

    // Use the breadcrumbs to navigate back to Downloads.
    await remoteCall.waitAndClickElement(
        appId, '#location-breadcrumbs .breadcrumb-path:nth-of-type(2)');

    // Wait for the contents of Downloads to load again.
    await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(files));

    // A user action should have been recorded for the breadcrumbs.
    chrome.test.assertEq(
        1, await getUserActionCount('FileBrowser.ClickBreadcrumbs'));
  };
})();
