// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{modificationTime:Date, size:number}}
 */
var FileSystemMetadata;

/**
 * Metadata provider for FileEntry#getMetadata.
 *
 * @param {!MetadataProviderCache} cache
 * @constructor
 * @extends {NewMetadataProvider<!FileSystemMetadata>}
 * @struct
 */
function FileSystemMetadataProvider(cache) {
  NewMetadataProvider.call(this, cache, ['modificationTime', 'size']);
}

FileSystemMetadataProvider.prototype.__proto__ = NewMetadataProvider.prototype;

/**
 * @override
 */
FileSystemMetadataProvider.prototype.getImpl = function(requests) {
  return Promise.all(requests.map(function(request) {
    return new Promise(function(fulfill, reject) {
      request.entry.getMetadata(fulfill, reject);
    });
  }));
};
