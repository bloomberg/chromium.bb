// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests the order is sorted correctly for each of the columns.
 */
testcase.sortColumns = function() {
  var appId;

  var NAME_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.world,
    ENTRIES.desktop,
    ENTRIES.hello,
    ENTRIES.beautiful
  ]);

  var SIZE_ASC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.desktop,
    ENTRIES.beautiful,
    ENTRIES.world
  ]);

  var SIZE_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.world,
    ENTRIES.beautiful,
    ENTRIES.desktop,
    ENTRIES.hello
  ]);

  var TYPE_ASC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.beautiful,
    ENTRIES.world,
    ENTRIES.hello,
    ENTRIES.desktop
  ]);

  var TYPE_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.desktop,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.beautiful
  ]);

  var DATE_ASC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.desktop,
    ENTRIES.beautiful
  ]);

  var DATE_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.beautiful,
    ENTRIES.desktop,
    ENTRIES.world,
    ENTRIES.hello
  ]);

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    function(inAppId) {
      appId = inAppId;
      remoteCall.callRemoteTestUtil('fakeMouseClick',
                                    appId,
                                    ['.table-header-cell:nth-of-type(1)'],
                                    this.next);
    },
    function() {
      remoteCall.waitForElement(appId, '.table-header-sort-image-asc').
          then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick',
                                    appId,
                                    ['.table-header-cell:nth-of-type(1)'],
                                    this.next);
    },
    function() {
      remoteCall.waitForElement(appId, '.table-header-sort-image-desc').
          then(this.next);
    },
    function() {
      remoteCall.waitForFiles(appId, NAME_DESC, {orderCheck: true}).
          then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick',
                                    appId,
                                    ['.table-header-cell:nth-of-type(2)'],
                                    this.next);
    },
    function() {
      remoteCall.waitForElement(appId, '.table-header-sort-image-desc').
          then(this.next);
    },
    function() {
      remoteCall.waitForFiles(appId, SIZE_DESC, {orderCheck: true}).
          then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick',
                                    appId,
                                    ['.table-header-cell:nth-of-type(2)'],
                                    this.next);
    },
    function() {
      remoteCall.waitForElement(appId, '.table-header-sort-image-asc').
          then(this.next);
    },
    function() {
      remoteCall.waitForFiles(appId, SIZE_ASC, {orderCheck: true}).
          then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick',
                                    appId,
                                    ['.table-header-cell:nth-of-type(4)'],
                                    this.next);
    },
    function() {
      remoteCall.waitForElement(appId, '.table-header-sort-image-asc').
          then(this.next);
    },
    function() {
      remoteCall.waitForFiles(appId, TYPE_ASC, {orderCheck: true}).
          then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick',
                                    appId,
                                    ['.table-header-cell:nth-of-type(4)'],
                                    this.next);
    },
    function() {
      remoteCall.waitForElement(appId, '.table-header-sort-image-desc').
          then(this.next);
    },
    function() {
      remoteCall.waitForFiles(appId, TYPE_DESC, {orderCheck: true}).
          then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick',
                                    appId,
                                    ['.table-header-cell:nth-of-type(5)'],
                                    this.next);
    },
    function() {
      remoteCall.waitForElement(appId, '.table-header-sort-image-desc').
          then(this.next);
    },
    function() {
      remoteCall.waitForFiles(appId, DATE_DESC, {orderCheck: true}).
          then(this.next);
    },
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick',
                                    appId,
                                    ['.table-header-cell:nth-of-type(5)'],
                                    this.next);
    },
    function() {
      remoteCall.waitForElement(appId, '.table-header-sort-image-asc').
          then(this.next);
    },
    function() {
      remoteCall.waitForFiles(appId, DATE_ASC, {orderCheck: true}).
          then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
