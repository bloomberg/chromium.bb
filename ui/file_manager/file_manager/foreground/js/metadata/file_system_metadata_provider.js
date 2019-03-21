// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Metadata provider for FileEntry#getMetadata.
 *
 * @constructor
 * @extends {MetadataProvider}
 * @struct
 */
function FileSystemMetadataProvider() {
  MetadataProvider.call(this, FileSystemMetadataProvider.PROPERTY_NAMES);
}

/**
 * @const {!Array<string>}
 */
FileSystemMetadataProvider.PROPERTY_NAMES =
    ['modificationTime', 'size', 'present', 'availableOffline'];

FileSystemMetadataProvider.prototype.__proto__ = MetadataProvider.prototype;

/**
 * @override
 */
FileSystemMetadataProvider.prototype.get = requests => {
  if (!requests.length) {
    return Promise.resolve([]);
  }
  return Promise.all(requests.map(request => {
    return new Promise((fulfill, reject) => {
             request.entry.getMetadata(fulfill, reject);
           })
        .then(
            result => {
              const item = new MetadataItem();
              item.modificationTime = result.modificationTime;
              item.size = request.entry.isDirectory ? -1 : result.size;
              item.present = true;
              item.availableOffline = true;
              return item;
            },
            error => {
              // Can't use console.error because some tests hit this line and
              // console.error causes them to fail because of JSErrorCount. This
              // error is an acceptable condition.
              console.warn(
                  'getMetadata failure for: ' + request.entry.toURL(), error);
              return new MetadataItem();
            });
  }));
};
