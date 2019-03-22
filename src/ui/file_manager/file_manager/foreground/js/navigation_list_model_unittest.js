// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!MockFileSystem} Simulate the drive file system. */
var drive;

/** @type {!MockFileSystem} Simulate a removable volume. */
var hoge;

function setUp() {
  window.loadTimeData.data = {
    MY_FILES_VOLUME_ENABLED: false,
    MY_FILES_ROOT_LABEL: 'My files',
    DOWNLOADS_DIRECTORY_LABEL: 'Downloads',
    DRIVE_DIRECTORY_LABEL: 'My Drive',
  };
  window.loadTimeData.getString = id => {
    return window.loadTimeData.data_[id] || id;
  };
  new MockCommandLinePrivate();
  // Override VolumeInfo.prototype.resolveDisplayRoot to be sync.
  VolumeInfoImpl.prototype.resolveDisplayRoot = function(successCallback) {
    this.displayRoot_ = this.fileSystem_.root;
    successCallback(this.displayRoot_);
  };

  // TODO(crbug.com/834103): Add integration test for Crostini.
  drive = new MockFileSystem('drive');
  hoge = new MockFileSystem('removable:hoge');

}

function testModel() {
  var volumeManager = new MockVolumeManager();
  var shortcutListModel = new MockFolderShortcutDataModel(
      [new MockFileEntry(drive, '/root/shortcut')]);
  var recentItem = new NavigationModelFakeItem(
      'recent-label', NavigationModelItemType.RECENT,
      {toURL: () => 'fake-entry://recent'});
  var model =
      new NavigationListModel(volumeManager, shortcutListModel, recentItem);
  model.linuxFilesItem = new NavigationModelFakeItem(
      'linux-files-label', NavigationModelItemType.CROSTINI,
      new FakeEntry(
          'linux-files-label', VolumeManagerCommon.RootType.CROSTINI));

  assertEquals(4, model.length);
  assertEquals('fake-entry://recent', model.item(0).entry.toURL());
  assertEquals('/root/shortcut', model.item(1).entry.fullPath);
  assertEquals('My files', model.item(2).label);
  assertEquals('drive', model.item(3).volumeInfo.volumeId);

  // Downloads and Crostini are displayed within My files.
  const myFilesEntry = model.item(2).entry;
  assertEquals(2, myFilesEntry.getUIChildren().length);
  assertEquals('Downloads', myFilesEntry.getUIChildren()[0].name);
  assertEquals('linux-files-label', myFilesEntry.getUIChildren()[1].name);
}

function testNoRecentOrLinuxFiles() {
  var volumeManager = new MockVolumeManager();
  var shortcutListModel = new MockFolderShortcutDataModel(
      [new MockFileEntry(drive, '/root/shortcut')]);
  var recentItem = null;
  var model =
      new NavigationListModel(volumeManager, shortcutListModel, recentItem);

  assertEquals(3, model.length);
  assertEquals('/root/shortcut', model.item(0).entry.fullPath);
  assertEquals('My files', model.item(1).label);
  assertEquals('drive', model.item(2).volumeInfo.volumeId);
}

function testAddAndRemoveShortcuts() {
  var volumeManager = new MockVolumeManager();
  var shortcutListModel = new MockFolderShortcutDataModel(
      [new MockFileEntry(drive, '/root/shortcut')]);
  var recentItem = null;
  var model =
      new NavigationListModel(volumeManager, shortcutListModel, recentItem);

  assertEquals(3, model.length);

  // Add a shortcut at the tail, shortcuts are sorted by their label.
  shortcutListModel.splice(1, 0, new MockFileEntry(drive, '/root/shortcut2'));
  assertEquals(4, model.length);
  assertEquals('shortcut', model.item(0).label);
  assertEquals('shortcut2', model.item(1).label);

  // Add a shortcut at the head.
  shortcutListModel.splice(0, 0, new MockFileEntry(drive, '/root/hoge'));
  assertEquals(5, model.length);
  assertEquals('hoge', model.item(0).label);
  assertEquals('shortcut', model.item(1).label);
  assertEquals('shortcut2', model.item(2).label);

  // Remove the last shortcut.
  shortcutListModel.splice(2, 1);
  assertEquals(4, model.length);
  assertEquals('hoge', model.item(0).label);
  assertEquals('shortcut', model.item(1).label);

  // Remove the first shortcut.
  shortcutListModel.splice(0, 1);
  assertEquals(3, model.length);
  assertEquals('shortcut', model.item(0).label);
}

function testAddAndRemoveVolumes() {
  var volumeManager = new MockVolumeManager();
  var shortcutListModel = new MockFolderShortcutDataModel(
      [new MockFileEntry(drive, '/root/shortcut')]);
  var recentItem = null;
  var model =
      new NavigationListModel(volumeManager, shortcutListModel, recentItem);

  assertEquals(3, model.length);

  // Removable volume 'hoge' is mounted.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:hoge'));
  assertEquals(4, model.length);
  assertEquals('/root/shortcut', model.item(0).entry.fullPath);
  assertEquals('My files', model.item(1).label);
  assertEquals('drive', model.item(2).volumeInfo.volumeId);
  assertEquals('removable:hoge', model.item(3).volumeInfo.volumeId);

  // Removable volume 'fuga' is mounted.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:fuga'));
  assertEquals(5, model.length);
  assertEquals('/root/shortcut', model.item(0).entry.fullPath);
  assertEquals('My files', model.item(1).label);
  assertEquals('drive', model.item(2).volumeInfo.volumeId);
  assertEquals('removable:hoge', model.item(3).volumeInfo.volumeId);
  assertEquals('removable:fuga', model.item(4).volumeInfo.volumeId);

  // A shortcut is created on the 'hoge' volume.
  shortcutListModel.splice(
      1, 0, new MockFileEntry(hoge, '/shortcut2'));
  assertEquals(6, model.length);
  assertEquals('/root/shortcut', model.item(0).entry.fullPath);
  assertEquals('/shortcut2', model.item(1).entry.fullPath);
  assertEquals('My files', model.item(2).label);
  assertEquals('drive', model.item(3).volumeInfo.volumeId);
  assertEquals('removable:hoge', model.item(4).volumeInfo.volumeId);
  assertEquals('removable:fuga', model.item(5).volumeInfo.volumeId);
}

/**
 * Checks that orderAndNestItems_ function does the following:
 * 1. produces the expected order of volumes.
 * 2. manages NavigationSection for the relevant volumes.
 * 3. keeps MTP/Archive/Removable volumes on the original order.
 */
function testOrderAndNestItems() {
  const volumeManager = new MockVolumeManager();
  const shortcutListModel = new MockFolderShortcutDataModel([
    new MockFileEntry(drive, '/root/shortcut'),
    new MockFileEntry(drive, '/root/shortcut2')
  ]);
  const recentItem = new NavigationModelFakeItem(
      'recent-label', NavigationModelItemType.RECENT,
      {toURL: () => 'fake-entry://recent'});
  const zipVolumeId = 'provided:dmboannefpncccogfdikhmhpmdnddgoe:' +
      '~%2FDownloads%2Fazip_file%2Ezip:' +
      '096eaa592ea7e8ffb9a27435e50dabd6c809c125';

  // Create different volumes.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:hoge'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'provided:prov1'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ARCHIVE, 'archive:a-rar'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:fuga'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.MTP, 'mtp:a-phone'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'provided:prov2'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, 'android_files:droid'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.MEDIA_VIEW, 'media_view:images_root'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.MEDIA_VIEW, 'media_view:videos_root'));
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.MEDIA_VIEW, 'media_view:audio_root'));
  // ZipArchiver mounts zip files as PROVIDED volume type.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, zipVolumeId));

  // Navigation items built above:
  //  1.  fake-entry://recent
  //  2.  media_view:images_root
  //  3.  media_view:videos_root
  //  4.  media_view:audio_root
  //  5.  /root/shortcut
  //  6.  /root/shortcut2
  //  7.  My files
  //        -> Downloads
  //        -> Play files
  //        -> Linux files
  //  8.  Drive  - from setup()
  //  9.  provided:prov1
  // 10.  provided:prov2
  //
  // 11.  removable:hoge
  // 12.  archive:a-rar  - mounted as archive
  // 13.  removable:fuga
  // 14.  mtp:a-phone
  // 15.  provided:"zip" - mounted as provided: $zipVolumeId

  // Constructor already calls orderAndNestItems_.
  const model =
      new NavigationListModel(volumeManager, shortcutListModel, recentItem);

  // Check items order and that MTP/Archive/Removable respect the original
  // order.
  assertEquals(15, model.length);
  assertEquals('recent-label', model.item(0).label);

  assertEquals('media_view:images_root', model.item(1).label);
  assertEquals('media_view:videos_root', model.item(2).label);
  assertEquals('media_view:audio_root', model.item(3).label);

  assertEquals('shortcut', model.item(4).label);
  assertEquals('shortcut2', model.item(5).label);
  assertEquals('My files', model.item(6).label);

  assertEquals('My Drive', model.item(7).label);
  assertEquals('provided:prov1', model.item(8).label);
  assertEquals('provided:prov2', model.item(9).label);

  assertEquals('removable:hoge', model.item(10).label);
  assertEquals('archive:a-rar', model.item(11).label);
  assertEquals('removable:fuga', model.item(12).label);
  assertEquals('mtp:a-phone', model.item(13).label);
  assertEquals(zipVolumeId, model.item(14).label);

  // Check NavigationSection, which defaults to TOP.
  // recent-label.
  assertEquals(NavigationSection.TOP, model.item(0).section);
  // Media View.
  // Images.
  assertEquals(NavigationSection.TOP, model.item(1).section);
  // Videos.
  assertEquals(NavigationSection.TOP, model.item(2).section);
  // Audio.
  assertEquals(NavigationSection.TOP, model.item(3).section);
  // shortcut.
  assertEquals(NavigationSection.TOP, model.item(4).section);
  // shortcut2.
  assertEquals(NavigationSection.TOP, model.item(5).section);

  // My Files.
  assertEquals(NavigationSection.MY_FILES, model.item(6).section);

  // Drive and FSP are grouped together.
  // My Drive.
  assertEquals(NavigationSection.CLOUD, model.item(7).section);
  // provided:prov1.
  assertEquals(NavigationSection.CLOUD, model.item(8).section);
  // provided:prov2.
  assertEquals(NavigationSection.CLOUD, model.item(9).section);

  // MTP/Archive/Removable are grouped together.
  // removable:hoge.
  assertEquals(NavigationSection.REMOVABLE, model.item(10).section);
  // archive:a-rar.
  assertEquals(NavigationSection.REMOVABLE, model.item(11).section);
  // removable:fuga.
  assertEquals(NavigationSection.REMOVABLE, model.item(12).section);
  // mtp:a-phone.
  assertEquals(NavigationSection.REMOVABLE, model.item(13).section);
  // archive:"zip" - $zipVolumeId
  assertEquals(NavigationSection.REMOVABLE, model.item(14).section);

  const myFilesModel = model.item(6);
  // Re-order again.
  model.orderAndNestItems_();
  // Check if My Files continues on the same position.
  assertEquals(NavigationSection.MY_FILES, model.item(6).section);
  // Check if My Files model is still the same instance, because DirectoryTree
  // expects it to be the same instance to be able to find it on the tree.
  assertEquals(myFilesModel, model.item(6));
}


function testMyFilesVolumeEnabled(callback) {
  loadTimeData.data_['MY_FILES_VOLUME_ENABLED'] = true;
  const volumeManager = new MockVolumeManager();
  // Index 1 is Downloads.
  assertEquals(
      VolumeManagerCommon.VolumeType.DOWNLOADS,
      volumeManager.volumeInfoList.item(1).volumeType);

  // Create a downloads folder inside it.
  const downloadsVolume = volumeManager.volumeInfoList.item(1);
  downloadsVolume.fileSystem.populate(['/Downloads/']);

  const shortcutListModel = new MockFolderShortcutDataModel([]);
  const recentItem = null;


  const crostiniFakeItem = new NavigationModelFakeItem(
      'linux-files-label', NavigationModelItemType.CROSTINI,
      new FakeEntry(
          'linux-files-label', VolumeManagerCommon.RootType.CROSTINI));

  // Create Android volume.
  volumeManager.volumeInfoList.add(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, 'android_files:droid'));

  // Navigation items built above:
  //  1. My files
  //       -> Play files
  //       -> Linux files
  //  2. Drive  - added by default by MockVolumeManager.

  // Constructor already calls orderAndNestItems_.
  const model =
      new NavigationListModel(volumeManager, shortcutListModel, recentItem);
  model.linuxFilesItem = crostiniFakeItem;

  assertEquals(2, model.length);
  assertEquals('My files', model.item(0).label);
  assertEquals('My Drive', model.item(1).label);

  // Android and Crostini are displayed within My files. And there is no
  // Downloads volume inside it. Downloads should be a normal folder inside My
  // files volume.
  const myFilesEntry = model.item(0).entry;
  assertEquals(2, myFilesEntry.children_.length);
  assertEquals('android_files:droid', myFilesEntry.children_[0].name);
  assertEquals('linux-files-label', myFilesEntry.children_[1].name);

  const reader = myFilesEntry.createReader();
  const foundEntries = [];
  reader.readEntries((entries) => {
    for (entry of entries)
      foundEntries.push(entry);
  });
  reportPromise(
      waitUntil(() => {
        // Wait for Downloads folder to be read from My files volume.
        return foundEntries.length >= 1;
      }).then(() => {
        assertEquals(foundEntries[0].name, 'Downloads');
        assertTrue(foundEntries[0].isDirectory);
      }),
      callback);
}
