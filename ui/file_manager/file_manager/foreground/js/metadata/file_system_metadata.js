// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!MetadataProviderCache} cache
 * @param {!FileSystemMetadataProvider} fileSystemMetadataProvider
 * @param {!ExternalMetadataProvider} externalMetadataProvider
 * @param {!ContentMetadataProvider} contentMetadataProvider
 * @param {!VolumeManagerCommon.VolumeInfoProvider} volumeManager
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
   * @private {!VolumeManagerCommon.VolumeInfoProvider}
   * @const
   */
  this.volumeManager_ = volumeManager;
}

/**
 * @param {!MetadataProviderCache} cache
 * @param {!VolumeManagerCommon.VolumeInfoProvider} volumeManager
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

  // Group property names.
  var fileSystemPropertyNames = [];
  var externalPropertyNames = [];
  var contentPropertyNames = [];
  var fallbackContentPropertyNames = [];
  for (var i = 0; i < names.length; i++) {
    var name = names[i];
    var isFileSystemProperty =
        FileSystemMetadataProvider.PROPERTY_NAMES.indexOf(name) !== -1;
    var isExternalProperty =
        ExternalMetadataProvider.PROPERTY_NAMES.indexOf(name) !== -1;
    var isContentProperty =
        ContentMetadataProvider.PROPERTY_NAMES.indexOf(name) !== -1;
    assert(isFileSystemProperty || isExternalProperty || isContentProperty);
    assert(!(isFileSystemProperty && isContentProperty));
    // If the property can be obtained both from ExternalProvider and from
    // ContentProvider, we can obtain the property from ExternalProvider without
    // fetching file content. On the other hand, the values from
    // ExternalProvider may be out of sync if the file is 'dirty'. Thus we
    // fallback to ContentProvider if the file is dirty. See below.
    if (isExternalProperty && isContentProperty) {
      externalPropertyNames.push(name);
      fallbackContentPropertyNames.push(name);
      continue;
    }
    if (isFileSystemProperty)
      fileSystemPropertyNames.push(name);
    if (isExternalProperty)
      externalPropertyNames.push(name);
    if (isContentProperty)
      contentPropertyNames.push(name);
  }

  // Obtain each group of property names.
  var resultPromises = [];
  var get = function(provider, entries, names) {
    return provider.get(entries, names).then(function(results) {
      return {entries: entries, results: results};
    });
  };
  resultPromises.push(get(
      this.fileSystemMetadataProvider_, localEntries, fileSystemPropertyNames));
  resultPromises.push(get(
      this.externalMetadataProvider_, externalEntries, externalPropertyNames));
  resultPromises.push(get(
      this.contentMetadataProvider_, entries, contentPropertyNames));
  if (fallbackContentPropertyNames.length) {
    var dirtyEntriesPromise = this.externalMetadataProvider_.get(
        externalEntries, ['dirty']).then(function(results) {
          return externalEntries.filter(function(entry, index) {
            return results[index].dirty;
          });
        });
    resultPromises.push(dirtyEntriesPromise.then(function(dirtyEntries) {
      return get(
          this.contentMetadataProvider_,
          localEntries.concat(dirtyEntries),
          fallbackContentPropertyNames);
    }.bind(this)));
  }

  // Merge results.
  return Promise.all(resultPromises).then(function(resultsList) {
    var integratedResults = {};
    for (var i = 0; i < resultsList.length; i++) {
      var inEntries = resultsList[i].entries;
      var results = resultsList[i].results;
      for (var j = 0; j < inEntries.length; j++) {
        var url = inEntries[j].toURL();
        integratedResults[url] = integratedResults[url] || new MetadataItem();
        for (var name in results[j]) {
          integratedResults[url][name] = results[j][name];
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
