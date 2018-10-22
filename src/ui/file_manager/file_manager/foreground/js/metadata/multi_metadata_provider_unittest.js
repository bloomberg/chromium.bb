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

function testMultiMetadataProviderBasic(callback) {
  var model = new MultiMetadataProvider(
      // Mocking FileSystemMetadataProvider.
      {
        get: function(requests) {
          assertEquals(1, requests.length);
          assertEquals('filesystem://A', requests[0].entry.toURL());
          assertArrayEquals(['size', 'modificationTime'], requests[0].names);
          return Promise.resolve(
              [{modificationTime: new Date(2015, 0, 1), size: 1024}]);
        }
      },
      // Mocking ExternalMetadataProvider.
      {
        get: function(requests) {
          assertEquals(1, requests.length);
          assertEquals('filesystem://B', requests[0].entry.toURL());
          assertArrayEquals(['size', 'modificationTime'], requests[0].names);
          return Promise.resolve(
              [{modificationTime: new Date(2015, 1, 2), size: 2048}]);
        }
      },
      // Mocking ContentMetadataProvider.
      {
        get: function(requests) {
          if (requests.length === 0)
            return Promise.resolve([]);
          assertEquals(2, requests.length);
          assertEquals('filesystem://A', requests[0].entry.toURL());
          assertEquals('filesystem://B', requests[1].entry.toURL());
          assertArrayEquals(['contentThumbnailUrl'], requests[0].names);
          assertArrayEquals(['contentThumbnailUrl'], requests[1].names);
          return Promise.resolve([
            {contentThumbnailUrl: 'THUMBNAIL_URL_A'},
            {contentThumbnailUrl: 'THUMBNAIL_URL_B'}
          ]);
        }
      },
      // Mocking VolumeManagerWrapper.
      volumeManager);
  reportPromise(model.get([
    new MetadataRequest(
        entryA, ['size', 'modificationTime', 'contentThumbnailUrl']),
    new MetadataRequest(
        entryB, ['size', 'modificationTime', 'contentThumbnailUrl'])
  ]).then(function(results) {
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

function testMultiMetadataProviderExternalAndContentProperty(callback) {
  var model = new MultiMetadataProvider(
      // Mocking FileSystemMetadataProvider.
      {
        get: function(requests) {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        }
      },
      // Mocking ExternalMetadataProvider.
      {
        get: function(requests) {
          assertEquals(2, requests.length);
          assertEquals('filesystem://B', requests[0].entry.toURL());
          assertEquals('filesystem://C', requests[1].entry.toURL());
          assertArrayEquals(['imageWidth', 'present'], requests[0].names);
          assertArrayEquals(['imageWidth', 'present'], requests[1].names);
          return Promise.resolve([
            {present: false, imageWidth: 200},
            {present: true, imageWidth: 400}
          ]);
        }
      },
      // Mocking ContentMetadataProvider.
      {
        get: function(requests) {
          assertEquals(1, requests.length);
          assertTrue(requests[0].entry.toURL() in this.results_);
          return Promise.resolve([
            this.results_[requests[0].entry.toURL()]
          ]);
        },
        results_: {
          'filesystem://A': {imageWidth: 100},
          'filesystem://C': {imageWidth: 300}
        }
      },
      // Mocking VolumeManagerWrapper.
      volumeManager);
  reportPromise(model.get([
    new MetadataRequest(entryA, ['imageWidth']),
    new MetadataRequest(entryB, ['imageWidth']),
    new MetadataRequest(entryC, ['imageWidth'])
  ]).then(function(results) {
    assertEquals(3, results.length);
    assertEquals(100, results[0].imageWidth);
    assertEquals(200, results[1].imageWidth);
    assertEquals(300, results[2].imageWidth);
  }), callback);
}
