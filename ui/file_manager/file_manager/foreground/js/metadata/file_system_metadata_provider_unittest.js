// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var entryA = {
  toURL: function() { return "filesystem://A"; },
  getMetadata: function(fulfill, reject) {
    Promise.resolve({modificationTime: new Date(2015, 1, 1), size: 1024}).then(
        fulfill, reject);
  }
};

var entryB = {
  toURL: function() { return "filesystem://B"; },
  getMetadata: function(fulfill, reject) {
    Promise.resolve({modificationTime: new Date(2015, 2, 2), size: 2048}).then(
        fulfill, reject);
  }
};

function testFileSystemMetadataProviderBasic(callback) {
  var cache = new MetadataProviderCache();
  var provider = new FileSystemMetadataProvider(cache);
  reportPromise(provider.get(
      [entryA, entryB],
      ['modificationTime', 'size']).then(
          function(results) {
            assertEquals(2, results.length);
            assertEquals(
                new Date(2015, 1, 1).toString(),
                results[0].modificationTime.toString());
            assertEquals(1024, results[0].size);
            assertEquals(
                new Date(2015, 2, 2).toString(),
                results[1].modificationTime.toString());
            assertEquals(2048, results[1].size);
          }), callback);
}
