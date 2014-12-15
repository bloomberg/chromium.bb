// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared cloud importer namespace
var importer = importer || {};

/**
 * Volume types eligible for the affections of Cloud Import.
 * @private @const {!Array.<!VolumeManagerCommon.VolumeType>}
 */
importer.ELIGIBLE_VOLUME_TYPES_ = [
  VolumeManagerCommon.VolumeType.MTP,
  VolumeManagerCommon.VolumeType.REMOVABLE
];

/**
 * @enum {string}
 */
importer.Destination = {
  GOOGLE_DRIVE: 'google-drive'
};

/**
 * Returns true if the entry is a media file (and a descendant of a DCIM dir).
 *
 * @param {Entry} entry
 * @return {boolean}
 */
importer.isMediaEntry = function(entry) {
  return !!entry &&
      entry.isFile &&
      FileType.isImageOrVideo(entry) &&
      entry.fullPath.toUpperCase().indexOf('/DCIM/') === 0;
};

/**
 * Returns true if the volume is eligible for Cloud Import.
 *
 * @param {VolumeInfo} volumeInfo
 * @return {boolean}
 */
importer.isEligibleVolume = function(volumeInfo) {
  return !!volumeInfo &&
      importer.ELIGIBLE_VOLUME_TYPES_.indexOf(volumeInfo.volumeType) !== -1;
};

/**
 * Returns true if the entry is cloud import eligible.
 *
 * @param {VolumeManagerCommon.VolumeInfoProvider} volumeInfoProvider
 * @param {Entry} entry
 * @return {boolean}
 */
importer.isEligibleEntry = function(volumeInfoProvider, entry) {
  console.assert(volumeInfoProvider !== null);
  if (importer.isMediaEntry(entry)) {
    var volumeInfo = volumeInfoProvider.getVolumeInfo(entry);
    return importer.isEligibleVolume(volumeInfo);
  }
  return false;
};

/**
 * Returns true if the entry represents a media directory for the purposes
 * of Cloud Import.
 *
 * @param {Entry} entry
 * @param  {VolumeManagerCommon.VolumeInfoProvider} volumeInfoProvider
 * @return {boolean}
 */
importer.isMediaDirectory = function(entry, volumeInfoProvider) {
  if (!entry || !entry.isDirectory || !entry.fullPath) {
    return false;
  }

  var fullPath = entry.fullPath.toUpperCase();
  if (fullPath !== '/DCIM' && fullPath !== '/DCIM/') {
    return false;
  }

  console.assert(volumeInfoProvider !== null);
  var volumeInfo = volumeInfoProvider.getVolumeInfo(entry);
  return importer.isEligibleVolume(volumeInfo);
};

/**
 * @return {!Promise.<boolean>} Resolves with true when Cloud Import feature
 *     is enabled.
 */
importer.importEnabled = function() {
  return new Promise(
      function(resolve, reject) {
        chrome.commandLinePrivate.hasSwitch(
            'enable-cloud-backup',
            /** @param {boolean} enabled */
            function(enabled) {
              importer.lastKnownImportEnabled = enabled;
              resolve(enabled);
            });
      });
};

/**
 * The last known state for the cloud import feature being enabled.
 *
 * <p>NOTE: The "command" framework is fully synchronous, meaning
 * we have to answer questions, like "can execute" synchronously.
 * For this reason we cache the last result from importer.importEnabled().
 * It might be wrong once, but it won't be wrong for long.
 *
 * @type {boolean}
 */
importer.lastKnownImportEnabled = false;
