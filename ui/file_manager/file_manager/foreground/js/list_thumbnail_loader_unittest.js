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

var listThumbnailLoader;
var getOneCallbacks;
var thumbnailLoadedEvents;
var fileListModel;
var fileSystem = new MockFileSystem('volume-id');
var directory1 = new MockDirectoryEntry(fileSystem, '/TestDirectory');
var entry1 = new MockEntry(fileSystem, '/Test1.jpg');
var entry2 = new MockEntry(fileSystem, '/Test2.jpg');
var entry3 = new MockEntry(fileSystem, '/Test3.jpg');
var entry4 = new MockEntry(fileSystem, '/Test4.jpg');
var entry5 = new MockEntry(fileSystem, '/Test5.jpg');
var entry6 = new MockEntry(fileSystem, '/Test6.jpg');

function setUp() {
  ListThumbnailLoader.NUM_OF_MAX_ACTIVE_TASKS = 2;
  ListThumbnailLoader.NUM_OF_PREFETCH = 1;
  ListThumbnailLoader.CACHE_SIZE = 5;
  MockThumbnailLoader.errorUrls = [];
  MockThumbnailLoader.testImageDataUrl = generateSampleImageDataUrl(document);
  MockThumbnailLoader.testImageWidth = 160;
  MockThumbnailLoader.testImageHeight = 160;

  getOneCallbacks = {};
  var metadataCache = {
    getOne: function(entry, type, callback) {
      getOneCallbacks[entry.toURL()] = callback;
    }
  };

  fileListModel = new FileListModel(metadataCache);

  listThumbnailLoader = new ListThumbnailLoader(fileListModel, metadataCache,
      document, MockThumbnailLoader);

  thumbnailLoadedEvents = [];
  listThumbnailLoader.addEventListener('thumbnailLoaded', function(event) {
    thumbnailLoadedEvents.push(event);
  });
}

function resolveGetOneCallback(url) {
  assert(getOneCallbacks[url]);
  getOneCallbacks[url]();
  delete getOneCallbacks[url];
}

function areEntriesInCache(entries) {
  for (var i = 0; i < entries.length; i++) {
    if (null === listThumbnailLoader.getThumbnailFromCache(entries[i]))
      return false;
  }
  return true;
}

/**
 * Story test for list thumbnail loader.
 */
function testStory(callback) {
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

  resolveGetOneCallback(entry2.toURL());

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

    resolveGetOneCallback(entry1.toURL());

    // Assert that task for (Test3.jpg) is enqueued.
    return waitUntil(function() {
      return !!getOneCallbacks[entry3.toURL()] &&
          !!getOneCallbacks[entry4.toURL()] &&
          Object.keys(getOneCallbacks).length === 2;
    });
  }), callback);
}

/**
 * Do not enqueue prefetch task when high priority range is at the end of list.
 */
function testRangeIsAtTheEndOfList() {
  // Set high priority range to 5 - 6.
  listThumbnailLoader.setHighPriorityRange(5, 6);

  fileListModel.push(directory1, entry1, entry2, entry3, entry4, entry5);

  // Assert that a task is enqueued for entry5.
  assertEquals(1, Object.keys(getOneCallbacks).length);
  assertEquals('filesystem:volume-id/Test5.jpg',
      Object.keys(getOneCallbacks)[0]);
}

function testCache(callback) {
  ListThumbnailLoader.NUM_OF_MAX_ACTIVE_TASKS = 5;

  // Set high priority range to 0 - 2.
  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(entry1, entry2, entry3, entry4, entry5, entry6);

  resolveGetOneCallback(entry1.toURL());
  // In this test case, entry 3 is resolved earlier than entry 2.
  resolveGetOneCallback(entry3.toURL());
  resolveGetOneCallback(entry2.toURL());
  assertEquals(0, Object.keys(getOneCallbacks).length);

  reportPromise(waitUntil(function() {
    return areEntriesInCache([entry3, entry2, entry1]);
  }).then(function() {
    // Move high priority range to 1 - 3.
    listThumbnailLoader.setHighPriorityRange(1, 3);
    resolveGetOneCallback(entry4.toURL());
    assertEquals(0, Object.keys(getOneCallbacks).length);

    return waitUntil(function() {
      return areEntriesInCache([entry4, entry3, entry2, entry1]);
    });
  }).then(function() {
    // Move high priority range to 4 - 6.
    listThumbnailLoader.setHighPriorityRange(4, 6);
    resolveGetOneCallback(entry5.toURL());
    resolveGetOneCallback(entry6.toURL());
    assertEquals(0, Object.keys(getOneCallbacks).length);

    return waitUntil(function() {
      return areEntriesInCache([entry6, entry5, entry4, entry3, entry2]);
    });
  }).then(function() {
    // Move high priority range to 3 - 5.
    listThumbnailLoader.setHighPriorityRange(3, 5);
    assertEquals(0, Object.keys(getOneCallbacks).length);
    assertTrue(areEntriesInCache([entry6, entry5, entry4, entry3, entry2]));

    // Move high priority range to 0 - 2.
    listThumbnailLoader.setHighPriorityRange(0, 2);
    resolveGetOneCallback(entry1.toURL());
    assertEquals(0, Object.keys(getOneCallbacks).length);

    return waitUntil(function() {
      return areEntriesInCache([entry3, entry2, entry1, entry6, entry5]);
    });
  }), callback);
}

/**
 * Test case for thumbnail fetch error. In this test case, thumbnail fetch for
 * entry 2 is failed.
 */
function testErrorHandling(callback) {
  MockThumbnailLoader.errorUrls = [entry2.toURL()];

  listThumbnailLoader.setHighPriorityRange(0, 2);
  fileListModel.push(entry1, entry2, entry3, entry4);

  resolveGetOneCallback(entry2.toURL());

  // Assert that new task is enqueued for entry3.
  reportPromise(waitUntil(function() {
    return !!getOneCallbacks[entry3.toURL()];
  }), callback);
}
