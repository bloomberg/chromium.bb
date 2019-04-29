// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const uma = {};

uma.testClickBreadcrumb = async (done) => {
  await test.setupAndWaitUntilReady();

  // Reset metrics.
  chrome.metricsPrivate.userActions_ = [];
  // Click first row which is 'photos' dir, wait for breadcrumb to show.
  assertTrue(test.fakeMouseDoubleClick('#file-list li.table-row'));
  await test.waitForElement(
      '#location-breadcrumbs .breadcrumb-path:nth-of-type(2)');

  // Click breadcrumb to return to parent dir.
  assertTrue(test.fakeMouseClick(
      '#location-breadcrumbs .breadcrumb-path:nth-of-type(1)'));
  await test.waitForFiles(test.TestEntryInfo.getExpectedRows(
      test.BASIC_MY_FILES_ENTRY_SET_WITH_LINUX_FILES));

  assertArrayEquals(
      ['FileBrowser.ClickBreadcrumbs'], chrome.metricsPrivate.userActions_);
  done();
};
