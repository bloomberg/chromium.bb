// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Wait until the condition is met.
 *
 * @param {function()} condition Condition.
 * @return {Promise} A promise which is resolved when condition is met.
 */
function waitUntil(condition) {
  return new Promise(function(resolve, reject) {
    var checkCondition = function() {
      if (condition())
        resolve();
      else
        setTimeout(checkCondition, 100);
    };
    checkCondition();
  });
}

/**
 * Generates a data url of a sample image for testing.
 * TODO(yawano) Consider to share image generation logic with
 *     gallery/js/image_editor/test_util.js.
 *
 * @param {Document} document Document.
 * @return {string} Data url of a sample image.
 */
function generateSampleImageDataUrl(document) {
  var canvas = document.createElement('canvas');
  canvas.width = 160;
  canvas.height = 160;

  var context = canvas.getContext('2d');
  context.fillStyle = 'black';
  context.fillRect(0, 0, 80, 80);
  context.fillRect(80, 80, 80, 80);

  return canvas.toDataURL('image/jpeg', 0.5);
}

/**
 * Story test for list thumbnail loader.
 */
function testStory(callback) {
  ListThumbnailLoader.NUM_OF_MAX_ACTIVE_TASKS = 2;
  MockThumbnailLoader.setTestImageDataUrl(generateSampleImageDataUrl(document));

  var getOneCallbacks = {};
  var metadataCache = {
    getOne: function(entry, type, callback) {
      getOneCallbacks[entry.toURL()] = callback;
    }
  };

  var fileListModel = new FileListModel(metadataCache);

  var listThumbnailLoader = new ListThumbnailLoader(fileListModel,
      metadataCache, document, MockThumbnailLoader);
  var thumbnailLoadedEvents = [];
  listThumbnailLoader.addEventListener('thumbnailLoaded', function(event) {
    thumbnailLoadedEvents.push(event);
  });

  // Add 1 directory and 5 entries.
  var fileSystem = new MockFileSystem('volume-id');
  var directory1 = new MockDirectoryEntry(fileSystem, '/TestDirectory');
  var entry1 = new MockEntry(fileSystem, '/Test1.jpg');
  var entry2 = new MockEntry(fileSystem, '/Test2.jpg');
  var entry3 = new MockEntry(fileSystem, '/Test3.jpg');
  var entry4 = new MockEntry(fileSystem, '/Test4.jpg');
  var entry5 = new MockEntry(fileSystem, '/Test5.jpg');
  fileListModel.push(directory1, entry1, entry2, entry3, entry4, entry5);

  // Set high priority range to 0 - 2.
  listThumbnailLoader.setHighPriorityRange(0, 2);

  // Assert that 2 fetch tasks are running.
  assertArrayEquals([entry1.toURL(), entry2.toURL()],
      Object.keys(getOneCallbacks));

  // Fails to get thumbnail from cache for Test2.jpg.
  assertEquals(null, listThumbnailLoader.getThumbnailFromCache(entry2));

  // Set high priority range to 4 - 6.
  listThumbnailLoader.setHighPriorityRange(4, 6);

  // Assert that no new tasks are enqueued.
  assertArrayEquals([entry1.toURL(), entry2.toURL()],
      Object.keys(getOneCallbacks));

  // Resolve metadataCache.getOne for Test2.jpg.
  getOneCallbacks[entry2.toURL()]();
  delete getOneCallbacks[entry2.toURL()];

  reportPromise(waitUntil(function() {
    // Assert that thumbnailLoaded event is fired for Test2.jpg.
    return thumbnailLoadedEvents.length === 1;
  }).then(function() {
    var event = thumbnailLoadedEvents.shift();
    assertEquals('filesystem:volume-id/Test2.jpg', event.fileUrl);
    assertTrue(event.dataUrl.length > 0);
    assertEquals(160, event.width);
    assertEquals(160, event.height);

    // Since thumbnail of Test2.jpg is loaded into the cache,
    // getThumbnailFromCache returns thumbnail for the image.
    var thumbnail = listThumbnailLoader.getThumbnailFromCache(entry2);
    assertEquals('filesystem:volume-id/Test2.jpg', thumbnail.fileUrl);
    assertTrue(thumbnail.dataUrl.length > 0);
    assertEquals(160, thumbnail.width);
    assertEquals(160, thumbnail.height);

    // Assert that new task is enqueued.
    return waitUntil(function() {
      return !!getOneCallbacks[entry1.toURL()] &&
          !!getOneCallbacks[entry4.toURL()] &&
          Object.keys(getOneCallbacks).length === 2;
    });
  }).then(function() {
    // Set high priority range to 2 - 4.
    listThumbnailLoader.setHighPriorityRange(2, 4);

    // Resolve metadataCache.getOne for Test1.jpg.
    getOneCallbacks[entry1.toURL()]();
    delete getOneCallbacks[entry1.toURL()];

    // Assert that task for (Test3.jpg) is enqueued.
    return waitUntil(function() {
      return !!getOneCallbacks[entry3.toURL()] &&
          !!getOneCallbacks[entry4.toURL()] &&
          Object.keys(getOneCallbacks).length === 2;
    });
  }).then(function() {
    // Cache is deleted when the item is removed from the list.
    var result = fileListModel.splice(2, 1); // Remove Test2.jpg.

    // Fail to fetch thumbnail from cache.
    return waitUntil(function() {
      return listThumbnailLoader.getThumbnailFromCache(entry2) === null;
    });
  }), callback);
}
