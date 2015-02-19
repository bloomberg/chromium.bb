// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!MetadataProviderCache} cache
 * @param {!FileSystemMetadataProvider} fileSystemMetadataProvider
 * @param {!ExternalMetadataProvider} externalMetadataProvider
 * @param {!VolumeManagerWrapper} volumeManager
 * @constructor
 * @struct
 */
function FileSystemMetadata(
    cache,
    fileSystemMetadataProvider,
    externalMetadataProvider,
    volumeManager) {
  /**
   * @private {!MetadataProviderCache}
   * @const
   */
  this.cache_ = cache;

  /**
   * @private {!FileSystemMetadataProvider}
   * @const
   */
  this.fileSystemMetadataProvider_ = fileSystemMetadataProvider;

  /**
   * @private {!ExternalMetadataProvider}
   * @const
   */
  this.externalMetadataProvider_ = externalMetadataProvider;

  /**
   * @private {!VolumeManagerWrapper}
   * @const
   */
  this.volumeManager_ = volumeManager;
}

/**
 * Obtains metadata for entries.
 * @param {!Array<!Entry>} entries Entries.
 * @param {!Array<string>} names Metadata property names to be obtained.
 * @return {!Promise<!Array<!MetadataItem>>}
 */
FileSystemMetadata.prototype.get = function(entries, names) {
  var localEntries = [];
  var localEntryIndexes = [];
  var externalEntries = [];
  var externalEntryIndexes = [];
  for (var i = 0; i < entries.length; i++) {
    var volumeInfo = this.volumeManager_.getVolumeInfo(entries[i]);
    if (volumeInfo &&
        (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE ||
         volumeInfo.volumeType === VolumeManagerCommon.VolumeType.PROVIDED)) {
      externalEntries.push(entries[i]);
      externalEntryIndexes.push(i);
    } else {
      localEntries.push(entries[i]);
      localEntryIndexes.push(i);
    }
  }

  // Correct property names that are valid for fileSystemMetadataProvider.
  var fileSystemPropertyNames = names.filter(function(name) {
    return FileSystemMetadataProvider.PROPERTY_NAMES.indexOf(name) !== -1;
  });

  return Promise.all([
    this.fileSystemMetadataProvider_.get(localEntries, fileSystemPropertyNames),
    this.externalMetadataProvider_.get(externalEntries, names)
  ]).then(function(results) {
    var integratedResults = [];
    var localResults = results[0];
    for (var i = 0; i < localResults.length; i++) {
      integratedResults[localEntryIndexes[i]] = localResults[i];
    }
    var externalResults = results[1];
    for (var i = 0; i < externalResults.length; i++) {
      integratedResults[externalEntryIndexes[i]] = externalResults[i];
    }
    return integratedResults;
  });
};

/**
 * Obtains metadata cache for entries.
 * @param {!Array<!Entry>} entries Entries.
 * @param {!Array<string>} names Metadata property names to be obtained.
 * @return {!Array<!MetadataItem>}
 */
FileSystemMetadata.prototype.getCache = function(entries, names) {
  return this.cache_.get(entries, names);
};

/**
 * Clears old metadata for newly created entries.
 * @param {!Array<!Entry>} entries
 */
FileSystemMetadata.prototype.notifyEntriesCreated = function(entries) {
  this.cache_.clear(util.entriesToURLs(entries));
};

/**
 * Clears metadata for deleted entries.
 * @param {!Array<string>} urls Note it is not an entry list because we cannot
 *     obtain entries after removing them from the file system.
 */
FileSystemMetadata.prototype.notifyEntriesRemoved = function(urls) {
  this.cache_.clear(urls);
};

/**
 * Invalidates metadata for updated entries.
 * @param {!Array<!Entry>} entries
 */
FileSystemMetadata.prototype.notifyEntriesChanged = function(entries) {
  this.cache_.invalidate(this.cache_.generateRequestId(), entries);
};
