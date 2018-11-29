// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

testcase.mountCrostini = async function() {
  const fakeLinuxFiles = '#directory-tree [root-type-icon="crostini"]';
  const realLinxuFiles = '#directory-tree [volume-type-icon="crostini"]';

  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.hello], []);

  // Add entries to crostini volume, but do not mount.
  await addEntries(['crostini'], BASIC_CROSTINI_ENTRY_SET);

  // Linux files fake root is shown.
  await remoteCall.waitForElement(appId, fakeLinuxFiles);

  // Mount crostini, and ensure real root and files are shown.
  remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [fakeLinuxFiles]);
  await remoteCall.waitForElement(appId, realLinxuFiles);
  const files = TestEntryInfo.getExpectedRows(BASIC_CROSTINI_ENTRY_SET);
  await remoteCall.waitForFiles(appId, files);

  // Unmount and ensure fake root is shown.
  remoteCall.callRemoteTestUtil('unmount', null, ['crostini']);
  await remoteCall.waitForElement(appId, fakeLinuxFiles);
};

testcase.sharePathWithCrostini = async function() {
  const fakeLinuxFiles = '#directory-tree [root-type-icon="crostini"]';
  const realLinuxFiles = '#directory-tree [volume-type-icon="crostini"]';
  const downloads = '#directory-tree [volume-type-icon="downloads"]';
  const photos = '#file-list [file-name="photos"]';
  const menuShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"]:not([hidden]):not([disabled])';
  const menuNoShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"][hidden][disabled="disabled"]';

  const {appId} = await setupAndWaitUntilReady(
      null, RootPath.DOWNLOADS, null, [ENTRIES.photos], []);

  // Ensure fake Linux files root is shown.
  await remoteCall.waitForElement(appId, fakeLinuxFiles);

  // Mount crostini, and ensure real root is shown.
  remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [fakeLinuxFiles]);
  await remoteCall.waitForElement(appId, realLinuxFiles);

  // Go back to downloads, wait for photos dir to be shown.
  remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [downloads]);
  await remoteCall.waitForElement(appId, photos);

  // Right-click 'photos' directory, ensure 'Share with Linux' is shown.
  remoteCall.callRemoteTestUtil('fakeMouseRightClick', appId, [photos]);
  await remoteCall.waitForElement(appId, menuShareWithLinux);

  // Click on 'Share with Linux', ensure menu is closed.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['#file-context-menu [command="#share-with-linux"]']);
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');

  // Right-click 'photos' directory, ensure 'Share with Linux' is not shown.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['#file-list [file-name="photos"']);
  await remoteCall.waitForElement(appId, menuNoShareWithLinux);
};
