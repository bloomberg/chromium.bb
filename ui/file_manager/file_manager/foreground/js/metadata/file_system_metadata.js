// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!MetadataProviderCache} cache
 * @param {!FileSystemMetadataProvider} fileSystemMetadataProvider
 * @param {!ExternalMetadataProvider} externalMetadataProvider
 * @param {!ContentMetadataProvider} contentMetadataProvider
 * @param {!VolumeManagerWrapper} volumeManager
 * @constructor
 * @struct
 */
function FileSystemMetadata(
    cache,
    fileSystemMetadataProvider,
    externalMetadataProvider,
    contentMetadataProvider,
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
   * @private {!ContentMetadataProvider}
   * @const
   */
  this.contentMetadataProvider_ = contentMetadataProvider;

  /**
   * @private {!VolumeManagerWrapper}
   * @const
   */
  this.volumeManager_ = volumeManager;
}

/**
 * @param {!MetadataProviderCache} cache
 * @param {!VolumeManagerWrapper} volumeManager
 * @return {!FileSystemMetadata}
 */
FileSystemMetadata.create = function(cache, volumeManager) {
  return new FileSystemMetadata(
      cache,
      new FileSystemMetadataProvider(cache),
      new ExternalMetadataProvider(cache),
      new ContentMetadataProvider(cache),
      volumeManager);
};

/**
 * Obtains metadata for entries.
 * @param {!Array<!Entry>} entries Entries.
 * @param {!Array<string>} names Metadata property names to be obtained.
 * @return {!Promise<!Array<!MetadataItem>>}
 */
FileSystemMetadata.prototype.get = function(entries, names) {
  var localEntries = [];
  var externalEntries = [];
  for (var i = 0; i < entries.length; i++) {
    var volumeInfo = this.volumeManager_.getVolumeInfo(entries[i]);
    if (volumeInfo &&
        (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE ||
         volumeInfo.volumeType === VolumeManagerCommon.VolumeType.PROVIDED)) {
      externalEntries.push(entries[i]);
    } else {
      localEntries.push(entries[i]);
    }
  }

  var fileSystemPropertyNames = [];
  var externalPropertyNames = [];
  var contentPropertyNames = [];
  for (var i = 0; i < names.length; i++) {
    var validName = false;
    if (FileSystemMetadataProvider.PROPERTY_NAMES.indexOf(names[i]) !== -1) {
      fileSystemPropertyNames.push(names[i]);
      validName = true;
    }
    if (ExternalMetadataProvider.PROPERTY_NAMES.indexOf(names[i]) !== -1) {
      externalPropertyNames.push(names[i]);
      validName = true;
    }
    if (ContentMetadataProvider.PROPERTY_NAMES.indexOf(names[i]) !== -1) {
      assert(!validName);
      contentPropertyNames.push(names[i]);
      validName = true;
    }
    assert(validName);
  }
  return Promise.all([
    this.fileSystemMetadataProvider_.get(localEntries, fileSystemPropertyNames),
    this.externalMetadataProvider_.get(externalEntries, externalPropertyNames),
    this.contentMetadataProvider_.get(entries, contentPropertyNames)
  ]).then(function(results) {
    var integratedResults = {};
    for (var i = 0; i < 3; i++) {
      var entryList = [localEntries, externalEntries, entries][i];
      for (var j = 0; j < entryList.length; j++) {
        var url = entryList[j].toURL();
        integratedResults[url] = integratedResults[url] || new MetadataItem();
        for (var name in results[i][j]) {
          integratedResults[url][name] = results[i][j][name];
        }
      }
    }
    return entries.map(function(entry) {
      return integratedResults[entry.toURL()];
    });
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
