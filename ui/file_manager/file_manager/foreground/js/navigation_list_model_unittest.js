// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!MockFileSystem} Simulate the drive file system. */
var drive;

/** @type {!MockFileSystem} Simulate a removable volume. */
var hoge;

// Set up string assets.
loadTimeData.data = {
  DRIVE_DIRECTORY_LABEL: 'My Drive',
  DRIVE_MY_DRIVE_LABEL: 'My Drive',
  DRIVE_TEAM_DRIVES_LABEL: 'Team Drives',
  DRIVE_OFFLINE_COLLECTION_LABEL: 'Offline',
  DRIVE_SHARED_WITH_ME_COLLECTION_LABEL: 'Shared with me',
  DRIVE_RECENT_COLLECTION_LABEL: 'Recents',
  DOWNLOADS_DIRECTORY_LABEL: 'Downloads',
  LINUX_FILES_ROOT_LABEL: 'Linux Files',
  MY_FILES_ROOT_LABEL: 'My Files',
  RECENT_ROOT_LABEL: 'Recent',
  MEDIA_VIEW_IMAGES_ROOT_LABEL: 'Images',
  MEDIA_VIEW_VIDEOS_ROOT_LABEL: 'Videos',
  MEDIA_VIEW_AUDIO_ROOT_LABEL: 'Audio',
};

function setUp() {
  new MockCommandLinePrivate();
  // Override VolumeInfo.prototype.resolveDisplayRoot.
  VolumeInfoImpl.prototype.resolveDisplayRoot = function() {};

  // TODO(crbug.com/834103): Add integration test for Crostini.
  drive = new MockFileSystem('drive');
  hoge = new MockFileSystem('removable:hoge');
}

function testModel() {
  var volumeManager = new MockVolumeManagerWrapper();
  var shortcutListModel = new MockFolderShortcutDataModel(
      [new MockFileEntry(drive, '/root/shortcut')]);
  var recentItem = new NavigationModelFakeItem(
      'recent-label', NavigationModelItemType.RECENT,
      {toURL: () => 'fake-entry://recent'});
  var addNewServicesItem = new NavigationModelMenuItem(
      'menu-button-label', '#add-new-services', 'menu-button-icon');
  var model = new NavigationListModel(
      volumeManager, shortcutListModel, recentItem, addNewServicesItem, true);
  model.linuxFilesItem = new NavigationModelFakeItem(
      'linux-files-label', NavigationModelItemType.CROSTINI,
      {toURL: () => 'fake-entry://linux-files'});

  assertEquals(6, model.length);
  assertEquals('drive', model.item(0).volumeInfo.volumeId);
  assertEquals('downloads', model.item(1).volumeInfo.volumeId);
  assertEquals('fake-entry://recent', model.item(2).entry.toURL());
  assertEquals('fake-entry://linux-files', model.item(3).entry.toURL());
  assertEquals('/root/shortcut', model.item(4).entry.fullPath);
  assertEquals('menu-button-label', model.item(5).label);
  assertEquals('#add-new-services', model.item(5).menu);
  assertEquals('menu-button-icon', model.item(5).icon);
}

function testNoRecentOrLinuxFiles() {
  var volumeManager = new MockVolumeManagerWrapper();
  var shortcutListModel = new MockFolderShortcutDataModel(
      [new MockFileEntry(drive, '/root/shortcut')]);
  var recentItem = null;
  var addNewServicesItem = new NavigationModelMenuItem(
      'menu-button-label', '#add-new-services', 'menu-button-icon');
  var model = new NavigationListModel(
      volumeManager, shortcutListModel, recentItem, addNewServicesItem, true);

  assertEquals(4, model.length);
  assertEquals('drive', model.item(0).volumeInfo.volumeId);
  assertEquals('downloads', model.item(1).volumeInfo.volumeId);
  assertEquals('/root/shortcut', model.item(2).entry.fullPath);
  assertEquals('menu-button-label', model.item(3).label);
  assertEquals('#add-new-services', model.item(3).menu);
  assertEquals('menu-button-icon', model.item(3).icon);
}

function testAddAndRemoveShortcuts() {
  var volumeManager = new MockVolumeManagerWrapper();
  var shortcutListModel = new MockFolderShortcutDataModel(
      [new MockFileEntry(drive, '/root/shortcut')]);
  var recentItem = null;
  var addNewServicesItem = null;
  var model = new NavigationListModel(
      volumeManager, shortcutListModel, recentItem, addNewServicesItem, true);

  assertEquals(3, model.length);

  // Add a shortcut at the tail.
  shortcutListModel.splice(1, 0, new MockFileEntry(drive, '/root/shortcut2'));
  assertEquals(4, model.length);
  assertEquals('/root/shortcut2', model.item(3).entry.fullPath);

  // Add a shortcut at the head.
  shortcutListModel.splice(0, 0, new MockFileEntry(drive, '/root/hoge'));
  assertEquals(5, model.length);
  assertEquals('/root/hoge', model.item(2).entry.fullPath);
  assertEquals('/root/shortcut', model.item(3).entry.fullPath);
  assertEquals('/root/shortcut2', model.item(4).entry.fullPath);

  // Remove the last shortcut.
  shortcutListModel.splice(2, 1);
  assertEquals(4, model.length);
  assertEquals('/root/hoge', model.item(2).entry.fullPath);
  assertEquals('/root/shortcut', model.item(3).entry.fullPath);

  // Remove the first shortcut.
  shortcutListModel.splice(0, 1);
  assertEquals(3, model.length);
  assertEquals('/root/shortcut', model.item(2).entry.fullPath);
}

function testAddAndRemoveVolumes() {
  var volumeManager = new MockVolumeManagerWrapper();
  var shortcutListModel = new MockFolderShortcutDataModel(
      [new MockFileEntry(drive, '/root/shortcut')]);
  var recentItem = null;
  var addNewServicesItem = null;
  var model = new NavigationListModel(
      volumeManager, shortcutListModel, recentItem, addNewServicesItem, true);

  assertEquals(3, model.length);

  // Removable volume 'hoge' is mounted.
  volumeManager.volumeInfoList.push(
      MockVolumeManagerWrapper.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:hoge'));
  assertEquals(4, model.length);
  assertEquals('drive', model.item(0).volumeInfo.volumeId);
  assertEquals('downloads', model.item(1).volumeInfo.volumeId);
  assertEquals('removable:hoge', model.item(2).volumeInfo.volumeId);
  assertEquals('/root/shortcut', model.item(3).entry.fullPath);

  // Removable volume 'fuga' is mounted.
  volumeManager.volumeInfoList.push(
      MockVolumeManagerWrapper.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:fuga'));
  assertEquals(5, model.length);
  assertEquals('drive', model.item(0).volumeInfo.volumeId);
  assertEquals('downloads', model.item(1).volumeInfo.volumeId);
  assertEquals('removable:hoge', model.item(2).volumeInfo.volumeId);
  assertEquals('removable:fuga', model.item(3).volumeInfo.volumeId);
  assertEquals('/root/shortcut', model.item(4).entry.fullPath);

  // A shortcut is created on the 'hoge' volume.
  shortcutListModel.splice(
      1, 0, new MockFileEntry(hoge, '/shortcut2'));
  assertEquals(6, model.length);
  assertEquals('drive', model.item(0).volumeInfo.volumeId);
  assertEquals('downloads', model.item(1).volumeInfo.volumeId);
  assertEquals('removable:hoge', model.item(2).volumeInfo.volumeId);
  assertEquals('removable:fuga', model.item(3).volumeInfo.volumeId);
  assertEquals('/root/shortcut', model.item(4).entry.fullPath);
  assertEquals('/shortcut2', model.item(5).entry.fullPath);
}

/**
 * Checks that orderAndNestItems_ function does the following:
 * 1. produces the expected order of volumes.
 * 2. manages NavigationSection for the relevant volumes.
 * 3. keeps MTP/Archive/Removable volumes on the original order.
 */
function testOrderAndNestItems() {
  const volumeManager = new MockVolumeManagerWrapper();
  const shortcutListModel = new MockFolderShortcutDataModel([
    new MockFileEntry(drive, '/root/shortcut'),
    new MockFileEntry(drive, '/root/shortcut2')
  ]);
  const recentItem = new NavigationModelFakeItem(
      'recent-label', NavigationModelItemType.RECENT,
      {toURL: () => 'fake-entry://recent'});
  const addNewServicesItem = null;

  // Create different volumes.
  volumeManager.volumeInfoList.push(
      MockVolumeManagerWrapper.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:hoge'));
  volumeManager.volumeInfoList.push(
      MockVolumeManagerWrapper.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.PROVIDED, 'provided:prov1'));
  volumeManager.volumeInfoList.push(
      MockVolumeManagerWrapper.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.ARCHIVE, 'archive:a-zip'));
  volumeManager.volumeInfoList.push(
      MockVolumeManagerWrapper.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:fuga'));
  volumeManager.volumeInfoList.push(
      MockVolumeManagerWrapper.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.MTP, 'mtp:a-phone'));
  volumeManager.volumeInfoList.push(
      MockVolumeManagerWrapper.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.PROVIDED, 'provided:prov2'));
  volumeManager.volumeInfoList.push(
      MockVolumeManagerWrapper.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.ANDROID_FILES, 'android_files:droid'));

  // Navigation items built above:
  //  1.  fake-entry://recent
  //  2.  /root/shortcut
  //  3.  /root/shortcut2
  //  4.  My-Files
  //        -> Downloads
  //        -> Play Files
  //        -> Linux Files
  //  5.  removable:hoge
  //  6.  archive:a-zip
  //  7.  removable:fuga
  //  8.  mtp:a-phone
  //  9.  Drive  - from setup()
  // 10.  provided:prov1
  // 11.  provided:prov2

  // Constructor already calls orderAndNestItems_.
  const model = new NavigationListModel(
      volumeManager, shortcutListModel, recentItem, addNewServicesItem, false);

  // Check items order and that MTP/Archive/Removable respect the original
  // order.
  assertEquals(11, model.length);
  assertEquals('recent-label', model.item(0).label);
  assertEquals('shortcut', model.item(1).label);
  assertEquals('shortcut2', model.item(2).label);
  assertEquals('My Files', model.item(3).label);
  assertEquals('removable:hoge', model.item(4).label);
  assertEquals('archive:a-zip', model.item(5).label);
  assertEquals('removable:fuga', model.item(6).label);
  assertEquals('mtp:a-phone', model.item(7).label);
  assertEquals('My Drive', model.item(8).label);
  assertEquals('provided:prov1', model.item(9).label);
  assertEquals('provided:prov2', model.item(10).label);

  // Check NavigationSection, which defaults to TOP.
  // recent-label.
  assertEquals(NavigationSection.TOP, model.item(0).section);
  // shortcut.
  assertEquals(NavigationSection.TOP, model.item(1).section);
  // shortcut2.
  assertEquals(NavigationSection.TOP, model.item(2).section);

  // My Files.
  assertEquals(NavigationSection.MY_FILES, model.item(3).section);

  // MTP/Archive/Removable are grouped together.
  // removable:hoge.
  assertEquals(NavigationSection.REMOVABLE, model.item(4).section);
  // archive:a-zip.
  assertEquals(NavigationSection.REMOVABLE, model.item(5).section);
  // removable:fuga.
  assertEquals(NavigationSection.REMOVABLE, model.item(6).section);
  // mtp:a-phone.
  assertEquals(NavigationSection.REMOVABLE, model.item(7).section);

  // Drive and FSP are grouped together.
  // My Drive.
  assertEquals(NavigationSection.CLOUD, model.item(8).section);
  // provided:prov1.
  assertEquals(NavigationSection.CLOUD, model.item(9).section);
  // provided:prov2.
  assertEquals(NavigationSection.CLOUD, model.item(10).section);

  const myFilesModel = model.item(3);
  // Re-order again.
  model.orderAndNestItems_();
  // Check if My Files continues on the same position.
  assertEquals(NavigationSection.MY_FILES, model.item(3).section);
  // Check if My Files model is still the same instance, because DirectoryTree
  // expects it to be the same instance to be able to find it on the tree.
  assertEquals(myFilesModel, model.item(3));
}
