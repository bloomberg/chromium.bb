// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{modificationTime:Date, size:number}}
 */
var FileSystemMetadataProperties;

/**
 * Metadata provider for FileEntry#getMetadata.
 *
 * @param {!MetadataProviderCache} cache
 * @constructor
 * @extends {NewMetadataProvider<!FileSystemMetadataProperties>}
 * @struct
 */
function FileSystemMetadataProvider(cache) {
  NewMetadataProvider.call(
      this, cache, FileSystemMetadataProvider.PROPERTY_NAMES);
}

/**
 * @const {!Array<string>}
 */
FileSystemMetadataProvider.PROPERTY_NAMES =
    ['modificationTime', 'size', 'present'];

FileSystemMetadataProvider.prototype.__proto__ = NewMetadataProvider.prototype;

/**
 * @override
 */
FileSystemMetadataProvider.prototype.getImpl = function(requests) {
  return Promise.all(requests.map(function(request) {
    return new Promise(function(fulfill, reject) {
      request.entry.getMetadata(fulfill, reject);
    }).then(function(properties) {
      return {
        modificationTime: properties.modificationTime,
        size: request.entry.isDirectory ? -1 : properties.size,
        present: true
      };
    });
  }));
};
