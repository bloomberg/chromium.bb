// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

testcase.mountCrostini = function() {
  const fakeLinuxFiles = '#directory-tree [root-type-icon="crostini"]';
  const realLinxuFiles = '#directory-tree [volume-type-icon="crostini"]';
  let appId;

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.hello], []);
    },
    // Add entries to crostini volume, but do not mount.
    function(results) {
      appId = results.windowId;
      addEntries(['crostini'], BASIC_CROSTINI_ENTRY_SET, this.next);
    },
    // Linux files fake root is shown.
    function() {
      remoteCall.waitForElement(appId, fakeLinuxFiles).then(this.next);
    },
    // Mount crostini, and ensure real root and files are shown.
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [fakeLinuxFiles]);
      remoteCall.waitForElement(appId, realLinxuFiles).then(this.next);
    },
    function() {
      const files = TestEntryInfo.getExpectedRows(BASIC_CROSTINI_ENTRY_SET);
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
    // Unmount and ensure fake root is shown.
    function() {
      // Unmount and ensure fake root is shown.
      remoteCall.callRemoteTestUtil('unmount', null, ['crostini']);
      remoteCall.waitForElement(appId, fakeLinuxFiles).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};

testcase.sharePathWithCrostini = function() {
  const fakeLinuxFiles = '#directory-tree [root-type-icon="crostini"]';
  const realLinuxFiles = '#directory-tree [volume-type-icon="crostini"]';
  const downloads = '#directory-tree [volume-type-icon="downloads"]';
  const photos = '#file-list [file-name="photos"]';
  const menuShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"]:not([hidden]):not([disabled])';
  const menuNoShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"][hidden][disabled="disabled"]';
  let appId;

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.photos], []);
    },
    // Ensure fake Linux files root is shown.
    function(results) {
      appId = results.windowId;
      remoteCall.waitForElement(appId, fakeLinuxFiles).then(this.next);
    },
    // Mount crostini, and ensure real root is shown.
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [fakeLinuxFiles]);
      remoteCall.waitForElement(appId, realLinuxFiles).then(this.next);
    },
    // Go back to downloads, wait for photos dir to be shown.
    function(results) {
      remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [downloads]);
      remoteCall.waitForElement(appId, photos).then(this.next);
    },
    // Right-click 'photos' directory, ensure 'Share with Linux' is shown.
    function(results) {
      remoteCall.callRemoteTestUtil('fakeMouseRightClick', appId, [photos]);
      remoteCall.waitForElement(appId, menuShareWithLinux).then(this.next);
    },
    // Click on 'Share with Linux', ensure menu is closed.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId,
          ['#file-context-menu [command="#share-with-linux"]'], this.next);
      remoteCall.waitForElement(appId, '#file-context-menu[hidden]')
          .then(this.next);
    },
    // Right-click 'photos' directory, ensure 'Share with Linux' is not shown.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['#file-list [file-name="photos"'],
          this.next);
      remoteCall.waitForElement(appId, menuNoShareWithLinux).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};
