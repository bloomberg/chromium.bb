// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var entryA = {
  toURL: function() { return 'filesystem://A'; }
};

var entryB = {
  toURL: function() { return 'filesystem://B'; }
};

var entryC = {
  toURL: function() { return 'filesystem://C'; }
};

var volumeManager = {
  getVolumeInfo: function(entry) {
    if (entry.toURL() === 'filesystem://A') {
      return {
        volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS
      };
    } else if (entry.toURL() === 'filesystem://B') {
      return {
        volumeType: VolumeManagerCommon.VolumeType.DRIVE
      };
    } else if (entry.toURL() === 'filesystem://C') {
      return {
        volumeType: VolumeManagerCommon.VolumeType.DRIVE
      };
    }
    assertNotReached();
  }
};

function testFileSystemMetadataBasic(callback) {
  var cache = new MetadataProviderCache();
  var model = new FileSystemMetadata(
      cache,
      // Mocking FileSystemMetadataProvider.
      {
        get: function(entries, names) {
          assertEquals(1, entries.length);
          assertEquals('filesystem://A', entries[0].toURL());
          assertArrayEquals(['size', 'modificationTime'], names);
          return Promise.resolve(
              [{modificationTime: new Date(2015, 0, 1), size: 1024}]);
        }
      },
      // Mocking ExternalMetadataProvider.
      {
        get: function(entries, names) {
          assertEquals(1, entries.length);
          assertEquals('filesystem://B', entries[0].toURL());
          assertArrayEquals(['size', 'modificationTime'], names);
          return Promise.resolve(
              [{modificationTime: new Date(2015, 1, 2), size: 2048}]);
        }
      },
      // Mocking ContentMetadataProvider.
      {
        get: function(entries, names) {
          assertEquals(2, entries.length);
          assertEquals('filesystem://A', entries[0].toURL());
          assertEquals('filesystem://B', entries[1].toURL());
          assertArrayEquals(['contentThumbnailUrl'], names);
          return Promise.resolve([
            {contentThumbnailUrl: 'THUMBNAIL_URL_A'},
            {contentThumbnailUrl: 'THUMBNAIL_URL_B'}
          ]);
        }
      },
      // Mocking VolumeManagerWrapper.
      volumeManager);
  reportPromise(
      model.get(
          [entryA, entryB],
          ['size', 'modificationTime', 'contentThumbnailUrl']).then(
          function(results) {
            assertEquals(2, results.length);
            assertEquals(
                new Date(2015, 0, 1).toString(),
                results[0].modificationTime.toString());
            assertEquals(1024, results[0].size);
            assertEquals('THUMBNAIL_URL_A', results[0].contentThumbnailUrl);
            assertEquals(
                new Date(2015, 1, 2).toString(),
                results[1].modificationTime.toString());
            assertEquals(2048, results[1].size);
            assertEquals('THUMBNAIL_URL_B', results[1].contentThumbnailUrl);
          }), callback);
}

function testFileSystemMetadataExternalAndContentProperty(callback) {
  var cache = new MetadataProviderCache();
  var model = new FileSystemMetadata(
      cache,
      // Mocking FileSystemMetadataProvider.
      {
        get: function(entries, names) {
          assertEquals(0, names.length);
          return Promise.resolve([{}]);
        }
      },
      // Mocking ExternalMetadataProvider.
      {
        get: function(entries, names) {
          assertEquals(2, entries.length);
          assertEquals('filesystem://B', entries[0].toURL());
          assertEquals('filesystem://C', entries[1].toURL());
          return Promise.resolve([
            {dirty: false, imageWidth: 200},
            {dirty: true, imageWidth: 400}
          ]);
        }
      },
      // Mocking ContentMetadataProvider.
      {
        get: function(entries, names) {
          if (names.length == 0)
            return Promise.resolve(entries.map(function() { return {}; }));
          assertEquals(2, entries.length);
          assertEquals('filesystem://A', entries[0].toURL());
          assertEquals('filesystem://C', entries[1].toURL());
          assertArrayEquals(['imageWidth'], names);
          return Promise.resolve([
            {imageWidth: 100},
            {imageWidth: 300}
          ]);
        }
      },
      // Mocking VolumeManagerWrapper.
      volumeManager);
  reportPromise(model.get([entryA, entryB, entryC], ['imageWidth']).then(
      function(results) {
        assertEquals(3, results.length);
        assertEquals(100, results[0].imageWidth);
        assertEquals(200, results[1].imageWidth);
        assertEquals(300, results[2].imageWidth);
      }), callback);
}
