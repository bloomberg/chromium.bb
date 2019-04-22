// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Tests the order is sorted correctly for each of the columns.
 */
testcase.sortColumns = async () => {
  const NAME_ASC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.beautiful,
    ENTRIES.hello,
    ENTRIES.desktop,
    ENTRIES.world,
  ]);

  const NAME_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.world,
    ENTRIES.desktop,
    ENTRIES.hello,
    ENTRIES.beautiful,
  ]);

  const SIZE_ASC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.desktop,
    ENTRIES.beautiful,
    ENTRIES.world,
  ]);

  const SIZE_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.world,
    ENTRIES.beautiful,
    ENTRIES.desktop,
    ENTRIES.hello,
  ]);

  const TYPE_ASC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.beautiful,
    ENTRIES.world,
    ENTRIES.hello,
    ENTRIES.desktop,
  ]);

  const TYPE_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.desktop,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.beautiful,
  ]);

  const DATE_ASC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.desktop,
    ENTRIES.beautiful,
  ]);

  const DATE_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.beautiful,
    ENTRIES.desktop,
    ENTRIES.world,
    ENTRIES.hello,
  ]);

  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Click the 'Name' column header and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(1)']);
  await remoteCall.waitForElement(appId, '.table-header-sort-image-asc');
  await remoteCall.waitForFiles(appId, NAME_ASC, {orderCheck: true});

  // Click the 'Name' again and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(1)']);
  await remoteCall.waitForElement(appId, '.table-header-sort-image-desc');
  await remoteCall.waitForFiles(appId, NAME_DESC, {orderCheck: true});

  // Click the 'Size' column header and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(2)']);
  await remoteCall.waitForElement(appId, '.table-header-sort-image-desc');
  await remoteCall.waitForFiles(appId, SIZE_DESC, {orderCheck: true});

  // 'Size' should be checked in the sort menu.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-button']);
  await remoteCall.waitForElement(appId, '#sort-menu-sort-by-size[checked]');

  // Click the 'Size' column header again and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(2)']);
  await remoteCall.waitForElement(appId, '.table-header-sort-image-asc');
  await remoteCall.waitForFiles(appId, SIZE_ASC, {orderCheck: true});

  // 'Size' should still be checked in the sort menu, even when the sort order
  // is reversed.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-button']);
  await remoteCall.waitForElement(appId, '#sort-menu-sort-by-size[checked]');

  // Click the 'Type' column header and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(4)']);
  await remoteCall.waitForElement(appId, '.table-header-sort-image-asc');
  await remoteCall.waitForFiles(appId, TYPE_ASC, {orderCheck: true});

  // Click the 'Type' column header again and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(4)']);
  await remoteCall.waitForElement(appId, '.table-header-sort-image-desc');
  await remoteCall.waitForFiles(appId, TYPE_DESC, {orderCheck: true});

  // 'Type' should still be checked in the sort menu, even when the sort order
  // is reversed.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-button']);
  await remoteCall.waitForElement(appId, '#sort-menu-sort-by-type[checked]');

  // Click the 'Date modified' column header and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(5)']);
  await remoteCall.waitForElement(appId, '.table-header-sort-image-desc');
  await remoteCall.waitForFiles(appId, DATE_DESC, {orderCheck: true});

  // Click the 'Date modified' column header again and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(5)']);
  await remoteCall.waitForElement(appId, '.table-header-sort-image-asc');
  await remoteCall.waitForFiles(appId, DATE_ASC, {orderCheck: true});

  // 'Date modified' should still be checked in the sort menu.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-button']);
  await remoteCall.waitForElement(appId, '#sort-menu-sort-by-date[checked]');

  // Click 'Name' in the sort menu and check the result.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-menu-sort-by-name']);
  await remoteCall.waitForElement(appId, '.table-header-sort-image-asc');
  await remoteCall.waitForFiles(appId, NAME_ASC, {orderCheck: true});

  // Click the 'Name' again to reverse the order (to descending order).
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(1)']);
  await remoteCall.waitForElement(appId, '.table-header-sort-image-desc');
  await remoteCall.waitForFiles(appId, NAME_DESC, {orderCheck: true});

  // Click 'Name' in the sort menu again should get the order back to
  // ascending order.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-menu-sort-by-name']);
  await remoteCall.waitForElement(appId, '.table-header-sort-image-asc');
  await remoteCall.waitForFiles(appId, NAME_ASC, {orderCheck: true});
};
