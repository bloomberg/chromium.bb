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
 * @param {!Entry} entry
 * @param  {!VolumeManagerCommon.VolumeInfoProvider} volumeInfoProvider
 * @return {boolean}
 */
importer.isEligibleEntry = function(entry, volumeInfoProvider) {
  // TODO(smckay): Check that entry is for media type.
  if (!entry.isFile) {
    return false;
  }

  var info = volumeInfoProvider.getVolumeInfo(entry);
  if (info && info.volumeType == VolumeManagerCommon.VolumeType.REMOVABLE) {
    return entry.fullPath.indexOf('/DCIM/') == 0;
  }
  return false;
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
            resolve);
      });
};
