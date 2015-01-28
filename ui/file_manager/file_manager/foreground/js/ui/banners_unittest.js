// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

loadTimeData.data = {
  DOWNLOADS_DIRECTORY_LABEL: 'Downloads',
  DRIVE_DIRECTORY_LABEL: 'Google Drive',
  DRIVE_MY_DRIVE_LABEL: 'My Drive',
  DRIVE_OFFLINE_COLLECTION_LABEL: 'Offline',
  DRIVE_SHARED_WITH_ME_COLLECTION_LABEL: 'Shared with me',
  DRIVE_RECENT_COLLECTION_LABEL: 'Recent',
  REMOVABLE_DIRECTORY_LABEL: 'External Storage',
  ARCHIVE_DIRECTORY_LABEL: 'Archives'
};

/** @type {!MockVolumeManager|undefined} */
var volumeManager;

/** @type {!MockDirectoryModel|undefined} */
var directoryModel;

/** @type {!MockChromeStorageAPI|undefined} */
var storageAPI;

/** @type {!MockDirectoryEntyr|undefined} */
var mtpDcimEntry;

/** @type {!MockDirectoryEntry|undefined} */
var downloadsEntry;

/** @type {!CloudImportBanner|undefined} */
var cloudImportBanner;

/** @type {!Element|undefined} */
var cloudImportBannerDiv;

/** @type {!Element|undefined} */
var cloudImportBannerCloseDiv;

function setUp() {
  volumeManager = new MockVolumeManagerWrapper();
  directoryModel = new MockDirectoryModel();
  storageAPI = new MockChromeStorageAPI();

  var mtpVolumeInfo = volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.MTP, 'mtp:test', 'Magic Space Phone');
  mtpDcimEntry = new MockDirectoryEntry(mtpVolumeInfo.fileSystem, '/DCIM');
  downloadsEntry = new MockDirectoryEntry(
      volumeManager.getCurrentProfileVolumeInfo(
          VolumeManagerCommon.VolumeType.DOWNLOADS).fileSystem,
      '/hello-world');

  cloudImportBanner = new CloudImportBanner(directoryModel, volumeManager);
  cloudImportBannerDiv = document.querySelector('#cloud-import-banner');
  cloudImportBannerCloseDiv = document.querySelector(
      '#cloud-import-banner-close');
}

function testIfCloudImportBannerOnDirectoryChanged(callback) {
  assertTrue(cloudImportBannerDiv.hidden);
  reportPromise(directoryModel.navigateToMockEntry(mtpDcimEntry)
      .then(function() {
        assertFalse(cloudImportBannerDiv.hidden);
        return directoryModel.navigateToMockEntry(downloadsEntry);
      })
      .then(function() {
        assertTrue(cloudImportBannerDiv.hidden);
      }), callback);
}

function testIfCloudImportBannerDiscard(callback) {
  assertTrue(cloudImportBannerDiv.hidden);
  reportPromise(directoryModel.navigateToMockEntry(mtpDcimEntry)
      .then(function() {
        assertFalse(cloudImportBannerDiv.hidden);
        cloudImportBannerCloseDiv.click();
      })
      .then(function() {
        assertTrue(cloudImportBannerDiv.hidden)
        // Go back to downloads, and again to an eligible directory.
        return directoryModel.navigateToMockEntry(downloadsEntry).then(
            directoryModel.navigateToMockEntry(mtpDcimEntry));
      })
      .then(function() {
        // Should not show up.
        assertTrue(cloudImportBannerDiv.hidden);
      }), callback);
}
