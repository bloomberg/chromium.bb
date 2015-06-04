// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Metadata provider for FileEntry#getMetadata.
 *
 * @constructor
 * @extends {NewMetadataProvider}
 * @struct
 */
function FileSystemMetadataProvider() {
  NewMetadataProvider.call(this, FileSystemMetadataProvider.PROPERTY_NAMES);
}

/**
 * @const {!Array<string>}
 */
FileSystemMetadataProvider.PROPERTY_NAMES = [
  'modificationTime', 'size', 'present', 'availableOffline'
];

FileSystemMetadataProvider.prototype.__proto__ = NewMetadataProvider.prototype;

/**
 * @override
 */
FileSystemMetadataProvider.prototype.get = function(requests) {
  if (!requests.length)
    return Promise.resolve([]);
  return Promise.all(requests.map(function(request) {
    return new Promise(function(fulfill, reject) {
      request.entry.getMetadata(fulfill, reject);
    }).then(function(result) {
      var item = new MetadataItem();
      item.modificationTime = result.modificationTime;
      item.size = request.entry.isDirectory ? -1 : result.size;
      item.present = true;
      item.availableOffline = true;
      return item;
    }, function() {
      return new MetadataItem();
    });
  }));
};
