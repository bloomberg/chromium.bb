// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const shareBase = {
  // Params for 'Share with Linux'.
  vmNameTermina: 'termina',
  vmNameSelectorLinux: 'linux',
  enumUmaShareWithLinux: 12,
  enumUmaManageLinuxSharing: 13,
  // Params for 'Share with Plugin VM'.
  vmNamePluginVm: 'PluginVm',
  vmNameSelectorPluginVm: 'plugin-vm',
  enumUmaShareWithPluginVm: 16,
  enumUmaManagePluginVmSharing: 17,
};

const crostiniShare = {};
const pluginVmShare = {};

shareBase.testSharePathsSuccess =
    async (vmName, vmNameSelector, enumUma, done) => {
  const share = 'share-with-' + vmNameSelector;
  const manage = 'manage-' + vmNameSelector + '-sharing';
  const menuNoShareWith = '#file-context-menu:not([hidden]) ' +
      '[command="#' + share + '"][hidden][disabled="disabled"]';
  const menuShareWith = '#file-context-menu:not([hidden]) ' +
      '[command="#' + share + '"]:not([hidden]):not([disabled])';
  const menuNoManageSharing = '#file-context-menu:not([hidden]) ' +
      '[command="#' + manage + '"][hidden][disabled="disabled"]';
  const menuManageSharing = '#file-context-menu:not([hidden]) ' +
      '[command="#' + manage + '"]:not([hidden]):not([disabled])';
  const shareWith = '#file-context-menu [command="#' + share + '"]';
  const menuShareWithDirTree = '#directory-tree-context-menu:not([hidden]) ' +
      '[command="#' + share + '"]:not([hidden]):not([disabled])';
  const shareWithDirTree =
      '#directory-tree-context-menu [command="#' + share + '"]';
  const photos = '#file-list [file-name="photos"]';
  const myFilesDirTree = '#directory-tree [root-type-icon="my_files"]';
  const oldSharePaths = chrome.fileManagerPrivate.sharePathsWithCrostini;
  let sharePathsCalled = false;
  let sharePathsPersist;
  chrome.fileManagerPrivate.sharePathsWithCrostini =
      (vmName, entry, persist, callback) => {
        oldSharePaths(vmName, entry, persist, () => {
          sharePathsCalled = true;
          sharePathsPersist = persist;
          callback();
        });
      };
  const oldCrostiniUnregister = fileManager.crostini.unregisterSharedPath;
  let unregisterCalled = false;
  fileManager.crostini.unregisterSharedPath = function(vmName, entry) {
    unregisterCalled = true;
    oldCrostiniUnregister.call(fileManager.crostini, vmName, entry);
  };
  chrome.metricsPrivate.smallCounts_ = [];
  chrome.metricsPrivate.values_ = [];

  await test.setupAndWaitUntilReady();
  // Right-click 'photos' directory.
  // Check 'Share with <VM>' is shown in menu.
  assertTrue(test.fakeMouseRightClick(photos), 'right-click photos');
  await test.waitForElement(menuShareWith);

  // Check 'Manage <VM> sharing' is not shown in menu.
  assertTrue(!!document.querySelector(menuNoManageSharing));
  // Click on 'Share with <VM>'.
  assertTrue(test.fakeMouseClick(shareWith, 'Share with <VM>'));
  // Check sharePathsWithCrostini is called.
  await test.repeatUntil(() => {
    return sharePathsCalled || test.pending('wait for sharePathsCalled');
  });

  // Check toast is shown.
  await test.repeatUntil(() => {
    return document.querySelector('#toast').shadowRoot.querySelector(
               '#container:not([hidden])') ||
        test.pending('wait for toast');
  });

  // Right-click 'photos' directory.
  // Check 'Share with <VM>' is not shown in menu.
  assertTrue(test.fakeMouseRightClick(photos), 'right-click photos');
  await test.waitForElement(menuNoShareWith);

  // Check 'Manage <VM> sharing' is shown in menu.
  assertTrue(!!document.querySelector(menuManageSharing));

  // Share should persist when right-click > Share with <VM>.
  assertTrue(sharePathsPersist);
  // Validate UMAs.
  assertEquals(1, chrome.metricsPrivate.smallCounts_.length);
  assertArrayEquals(
      ['FileBrowser.CrostiniSharedPaths.Depth.Downloads', 1],
      chrome.metricsPrivate.smallCounts_[0]);
  const lastEnumUma = chrome.metricsPrivate.values_.pop();
  assertEquals('FileBrowser.MenuItemSelected', lastEnumUma[0].metricName);
  assertEquals(enumUma, lastEnumUma[1]);

  // Dispatch unshare event which is normally initiated when the user
  // manages shared paths in the settings page.
  const photosEntries =
      mockVolumeManager
          .getCurrentProfileVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS)
          .fileSystem.entries['/photos'];
  chrome.fileManagerPrivate.onCrostiniChanged.dispatchEvent(
      {eventType: 'unshare', vmName: vmName, entries: [photosEntries]});
  // Check unregisterSharedPath is called.
  await test.repeatUntil(() => {
    return unregisterCalled || test.pending('wait for unregisterCalled');
  });

  // Right-click 'photos' directory.
  // Check 'Share with <VM>' is shown in menu.
  assertTrue(test.fakeMouseRightClick(photos), 'right-click photos');
  await test.waitForElement(menuShareWith);

  // Verify dialog is shown for MyFiles root.
  // Check 'Share with <VM>' is shown in menu.
  assertTrue(test.fakeMouseRightClick(myFilesDirTree), 'right-click MyFiles');
  await test.waitForElement(menuShareWithDirTree);

  // Click 'Share with <VM>', verify dialog.
  assertTrue(test.fakeMouseClick(shareWithDirTree, 'Share with <VM>'));
  await test.waitForElement('.cr-dialog-container.shown');

  // Click Cancel button to close.
  assertTrue(test.fakeMouseClick('button.cr-dialog-cancel'));
  await test.waitForElementLost('.cr-dialog-container');

  // Restore fmp.*.
  chrome.fileManagerPrivate.sharePathsWithCrostini = oldSharePaths;
  // Restore Crostini.unregisterSharedPath.
  fileManager.crostini.unregisterSharedPath = oldCrostiniUnregister;
  done();
};

crostiniShare.testSharePathsSuccess = done => {
  shareBase.testSharePathsSuccess(
      shareBase.vmNameTermina, shareBase.vmNameSelectorLinux,
      shareBase.enumUmaShareWithLinux, done);
};

pluginVmShare.testSharePathsSuccess = done => {
  shareBase.testSharePathsSuccess(
      shareBase.vmNamePluginVm, shareBase.vmNameSelectorPluginVm,
      shareBase.enumUmaShareWithPluginVm, done);
};

// Verify right-click menu with 'Share with <VM>' is not shown
// for:
// * Files (not directory).
// * Any folder already shared.
// * Root Downloads folder.
// * Any folder outside of downloads (e.g. crostini or drive).
// * Is shown for drive if DriveFS is enabled.
shareBase.testSharePathShown = async (vmName, vmNameSelector, done) => {
  const share = 'share-with-' + vmNameSelector;
  const myFiles = '#directory-tree .tree-item [root-type-icon="my_files"]';
  const downloads = '#file-list li [file-type-icon="downloads"]';
  const linuxFiles = '#directory-tree .tree-item [root-type-icon="crostini"]';
  const googleDrive = '#directory-tree .tree-item [volume-type-icon="drive"]';
  const menuHidden = '#file-context-menu[hidden]';
  const menuNoShareWith = '#file-context-menu:not([hidden]) ' +
      '[command="#' + share + '"][hidden][disabled="disabled"]';
  const menuShareWith = '#file-context-menu:not([hidden]) ' +
      '[command="#' + share + '"]:not([hidden]):not([disabled])';
  const removableVolumeRoot = '#directory-tree [volume-type-icon="removable"]';
  const menuShareWithDirTree = '#directory-tree-context-menu:not([hidden]) ' +
      '[command="#' + share + '"]:not([hidden]):not([disabled])';
  const menuShareWithVolumeRoot = '#roots-context-menu:not([hidden]) ' +
      '[command="#' + share + '"]:not([hidden]):not([disabled])';
  let alreadySharedPhotosDir;

  await test.setupAndWaitUntilReady();
  // Right-click 'hello.txt' file.
  // Check 'Share with <VM>' is not shown in menu.
  assertTrue(
      test.fakeMouseRightClick('#file-list [file-name="hello.txt"]'),
      'right-click hello.txt');
  await test.waitForElement(menuNoShareWith);

  // Set a folder as already shared.
  alreadySharedPhotosDir =
      mockVolumeManager
          .getCurrentProfileVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS)
          .fileSystem.entries['/photos'];
  fileManager.crostini.registerSharedPath(vmName, alreadySharedPhotosDir);
  assertTrue(
      test.fakeMouseRightClick('#file-list [file-name="photos"]'),
      'right-click hello.txt');
  await test.waitForElement(menuNoShareWith);

  // Select 'My files' in directory tree to show Downloads in file list.
  assertTrue(test.fakeMouseClick(myFiles), 'click My files');
  await test.waitForElement(downloads);

  // Right-click 'Downloads' directory.
  // Check 'Share with <VM>' is shown in menu.
  assertTrue(test.fakeMouseRightClick(downloads), 'right-click downloads');
  await test.waitForElement(menuShareWith);

  // Right-click 'MyFiles' in directory tree.
  // Check 'Share with <VM>' is shown in menu.
  assertTrue(test.fakeMouseRightClick(myFiles), 'MyFiles dirtree');
  await test.waitForElement(menuShareWithDirTree);

  // Select removable root.
  test.mountRemovable();
  await test.waitForElement(removableVolumeRoot);

  // Right-click 'MyUSB' removable root.
  // Check 'Share with <VM>' is shown in menu.
  assertTrue(test.fakeMouseRightClick(removableVolumeRoot), 'removable root');
  await test.waitForElement(menuShareWithVolumeRoot);

  // Select 'Linux files' in directory tree to show dir A in file list.
  await test.waitForElement(linuxFiles);

  assertTrue(test.fakeMouseClick(linuxFiles), 'click Linux files');
  await test.waitForFiles(
      test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));

  // Check 'Share with <VM>' is not shown in menu.
  assertTrue(
      test.fakeMouseRightClick('#file-list [file-name="A"]'),
      'right-click directory A');
  await test.waitForElement(menuNoShareWith);

  // Select 'Google Drive' to show dir photos in file list.
  await test.waitForElement(googleDrive);

  assertTrue(test.fakeMouseClick(googleDrive), 'click Google Drive');
  await test.waitForFiles(
      test.TestEntryInfo.getExpectedRows(test.BASIC_DRIVE_ENTRY_SET));

  // Check 'Share with <VM>' is not shown in menu.
  assertTrue(
      test.fakeMouseRightClick('#file-list [file-name="photos"]'),
      'right-click photos');
  await test.waitForElement(menuNoShareWith);

  // Close menu by clicking file-list.
  assertTrue(test.fakeMouseClick('#file-list'));
  await test.waitForElement(menuHidden);

  // Set DRIVE_FS_ENABLED, and check that 'Share with <VM>' is shown.
  loadTimeData.data_['DRIVE_FS_ENABLED'] = true;
  // Check 'Share with <VM>' is not shown in menu.
  assertTrue(
      test.fakeMouseRightClick('#file-list [file-name="photos"]'),
      'right-click photos');
  await test.waitForElement(menuShareWith);

  // Reset Linux files back to unmounted.
  chrome.fileManagerPrivate.removeMount('crostini');
  await test.waitForElement(
      '#directory-tree .tree-item [root-type-icon="crostini"]');

  // Unset DRIVE_FS_ENABLED.
  loadTimeData.data_['DRIVE_FS_ENABLED'] = false;
  // Clear Crostini shared folders.
  fileManager.crostini.unregisterSharedPath(vmName, alreadySharedPhotosDir);
  done();
};

crostiniShare.testSharePathShown = done => {
  shareBase.testSharePathShown(
      shareBase.vmNameTermina, shareBase.vmNameSelectorLinux, done);
};

pluginVmShare.testSharePathShown = done => {
  shareBase.testSharePathShown(
      shareBase.vmNamePluginVm, shareBase.vmNameSelectorPluginVm, done);
};

// Verify gear menu 'Manage ? sharing'.
shareBase.testGearMenuManage =
    async (vmName, vmNameSelector, enumUma, done) => {
  const manage = '#gear-menu-manage-' + vmNameSelector + '-sharing';
  const gearMenuClosed = '#gear-menu[hidden]';
  const manageSharingOptionHidden =
      '#gear-menu:not([hidden]) ' + manage + '[hidden]';
  const manageSharingOptionShown =
      '#gear-menu:not([hidden]) ' + manage + ':not([hidden])';
  chrome.metricsPrivate.values_ = [];

  await test.setupAndWaitUntilReady();

  // Setup with crostini disabled.
  fileManager.crostini.setEnabled(vmName, false);
  // Click gear menu, ensure 'Manage <VM> sharing' is hidden.
  assertTrue(test.fakeMouseClick('#gear-button'));
  await test.waitForElement(manageSharingOptionHidden);

  // Close gear menu.
  assertTrue(test.fakeMouseClick('#gear-button'));
  await test.waitForElement(gearMenuClosed);

  // Setup with crostini enabled.
  fileManager.crostini.setEnabled(vmName, true);
  // Click gear menu, ensure 'Manage <VM> sharing' is shown.
  assertTrue(test.fakeMouseClick('#gear-button'));
  await test.waitForElement(manageSharingOptionShown);

  // Click 'Manage <VM> sharing'.
  assertTrue(test.fakeMouseClick(manage));
  await test.waitForElement(gearMenuClosed);

  // Verify UMA.
  const lastEnumUma = chrome.metricsPrivate.values_.pop();
  assertEquals('FileBrowser.MenuItemSelected', lastEnumUma[0].metricName);
  assertEquals(enumUma, lastEnumUma[1]);
  done();
};

crostiniShare.testGearMenuManageLinuxSharing = done => {
  shareBase.testGearMenuManage(
      shareBase.vmNameTermina, shareBase.vmNameSelectorLinux,
      shareBase.enumUmaManageLinuxSharing, done);
};

pluginVmShare.testGearMenuManagePluginVmSharing = done => {
  shareBase.testGearMenuManage(
      shareBase.vmNamePluginVm, shareBase.vmNameSelectorPluginVm,
      shareBase.enumUmaManagePluginVmSharing, done);
};
