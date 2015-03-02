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
  'modificationTime', 'size', 'present', 'availableOffline', 'contentMimeType'
];

FileSystemMetadataProvider.prototype.__proto__ = NewMetadataProvider.prototype;

/**
 * @override
 */
FileSystemMetadataProvider.prototype.get = function(requests) {
  if (!requests.length)
    return Promise.resolve([]);
  return Promise.all(requests.map(function(request) {
    return Promise.all([
        new Promise(function(fulfill, reject) {
          request.entry.getMetadata(fulfill, reject);
        }),
        new Promise(function(fulfill) {
          if (request.names.indexOf('contentMimeType') > -1) {
            chrome.fileManagerPrivate.getMimeType(
                request.entry.toURL(), fulfill);
          } else {
            fulfill(null);
          }
        })
    ]).then(function(results) {
      var item = new MetadataItem();
      item.modificationTime = results[0].modificationTime;
      item.size = request.entry.isDirectory ? -1 : results[0].size;
      item.present = true;
      item.availableOffline = true;
      if (results[1] !== null)
        item.contentMimeType = results[1];
      return item;
    }, function() {
      return new MetadataItem();
    });
  }));
};
