// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var entryA = {
  toURL: function() { return "filesystem://A"; }
};

var entryB = {
  toURL: function() { return "filesystem://B"; }
};

function testExternalMetadataProviderBasic(callback) {
  // Mocking SharedWorker's port.
  var port = {
    postMessage: function(message) {
      if (message.verb === 'request') {
        this.onmessage({
          data: {
            verb: 'result',
            arguments: [
              message.arguments[0],
              {
                thumbnailURL: message.arguments[0] + ',url',
                thumbnailTransform: message.arguments[0] + ',transform'
              }
            ]
          }
        });
      }
    },
    start: function() {}
  };
  var provider = new ContentMetadataProvider(port);
  reportPromise(provider.get([
    new MetadataRequest(
        entryA, ['contentThumbnailUrl', 'contentThumbnailTransform']),
    new MetadataRequest(
        entryB, ['contentThumbnailUrl', 'contentThumbnailTransform'])
  ]).then(function(results) {
    assertEquals(2, results.length);
    assertEquals('filesystem://A,url', results[0].contentThumbnailUrl);
    assertEquals(
        'filesystem://A,transform',
        results[0].contentThumbnailTransform);
    assertEquals('filesystem://B,url', results[1].contentThumbnailUrl);
    assertEquals(
        'filesystem://B,transform',
        results[1].contentThumbnailTransform);
  }), callback);
}
