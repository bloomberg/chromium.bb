// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared cloud importer namespace
var importer = importer || {};

/**
 * @enum {string}
 */
importer.Destination = {
  GOOGLE_DRIVE: 'google-drive'
};

/**
 * Returns true if the entry is cloud import eligible.
 *
 * @param {VolumeManagerCommon.VolumeInfoProvider} volumeInfoProvider
 * @param {Entry} entry
 * @return {boolean}
 */
importer.isEligibleEntry = function(volumeInfoProvider, entry) {
  assert(volumeInfoProvider != null);
  if (entry && entry.isFile && FileType.isImageOrVideo(entry)) {
    var volumeInfo = volumeInfoProvider.getVolumeInfo(entry);
    if (volumeInfo &&
      volumeInfo.volumeType == VolumeManagerCommon.VolumeType.REMOVABLE) {
      return entry.fullPath.indexOf('/DCIM/') === 0;
    }
  }
  return false;
};

/**
 * Returns true if the entry represents a media directory for the purposes
 * of cloud import.
 *
 * @param {Entry} entry
 * @param  {VolumeManagerCommon.VolumeInfoProvider} volumeInfoProvider
 * @return {boolean}
 */
importer.isMediaDirectory = function(entry, volumeInfoProvider) {
  if (!entry || !entry.isDirectory || !entry.fullPath) {
    return false;
  }

  if (entry.fullPath !== '/DCIM') {
    return false;
  }

  assert(volumeInfoProvider != null);
  var volumeInfo = volumeInfoProvider.getVolumeInfo(entry);
  return !!volumeInfo &&
      volumeInfo.isType(VolumeManagerCommon.VolumeType.REMOVABLE) &&
      volumeInfo.hasMedia;
};

/**
 * @return {!Promise.<boolean>} Resolves with true when Cloud Import feature
 *     is enabled.
 */
importer.importEnabled = function() {
  // TODO(smckay): Also verify that Drive is enabled and we're
  // not running in guest mode.
  return new Promise(
      function(resolve, reject) {
        chrome.commandLinePrivate.hasSwitch(
            'enable-cloud-backup',
            /**
             * @param {boolean} enabled
             */
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
