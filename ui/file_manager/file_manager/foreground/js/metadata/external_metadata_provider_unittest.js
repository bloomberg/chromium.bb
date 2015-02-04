// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var entryA = {
  toURL: function() { return "filesystem://A"; },
  isFile: true
};

var entryB = {
  toURL: function() { return "filesystem://B"; },
  isFile: true
};

function testExternalMetadataProviderBasic(callback) {
  // Mocking chrome API.
  window.chrome = {
    fileManagerPrivate: {
      getEntryProperties: function(urls, callback) {
        assertEquals('filesystem://A', urls[0]);
        assertEquals('filesystem://B', urls[1]);
        callback([{
          lastModifiedTime: new Date(2015, 0, 1).getTime(),
          fileSize: 1024
        }, {
          lastModifiedTime: new Date(2015, 1, 2).getTime(),
          fileSize: 2048
        }]);
      }
    },
    runtime: {lastError: null}
  };
  var cache = new MetadataProviderCache();
  var provider = new ExternalMetadataProvider(cache);
  reportPromise(provider.get(
      [entryA, entryB],
      ['modificationTime', 'size']).then(
          function(results) {
            assertEquals(2, results.length);
            assertEquals(
                new Date(2015, 0, 1).toString(),
                results[0].modificationTime.toString());
            assertEquals(1024, results[0].size);
            assertEquals(
                new Date(2015, 1, 2).toString(),
                results[1].modificationTime.toString());
            assertEquals(2048, results[1].size);
          }), callback);
}
