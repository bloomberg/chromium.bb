// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const {string} */
var FILE_LAST_MODIFIED = new Date("Dec 4 1968").toString();

/** @const {string} */
var FILE_SIZE = 1234;

/** @const {string} */
var FILE_PATH = 'test/data';

/** @const {string} */
var DEVICE = importer.Destination.DEVICE;

/** @const {string} */
var GOOGLE_DRIVE = importer.Destination.GOOGLE_DRIVE;

/**
 * Space Cloud: Your source for interstellar cloud storage.
 * @const {string}
 */
var SPACE_CAMP = 'Space Camp';

/** @type {!MockFileSystem|undefined} */
var testFileSystem;

/** @type {!MockFileEntry|undefined} */
var testFileEntry;

/** @type {!importer.RecordStorage|undefined} */
var storage;

/** @type {!Promise.<!importer.PersistentImportHistory>|undefined} */
var historyProvider;

/** @type {Promise} */
var testPromise;

// Set up the test components.
function setUp() {
  testFileEntry = new MockFileEntry(
      testFileSystem,
      FILE_PATH, {
        size: FILE_SIZE,
        modificationTime: FILE_LAST_MODIFIED
      });
  testFileSystem = new MockFileSystem('abc-123', 'filesystem:abc-123');
  testFileSystem.entries[FILE_PATH] = testFileEntry;

  storage = new TestRecordStorage();

  var history = new importer.PersistentImportHistory(storage);
  historyProvider = history.refresh();
}

function testHistoryWasCopiedFalseForUnknownEntry(callback) {
  // TestRecordWriter is pre-configured with a Space Cloud entry
  // but not for this file.
  testPromise = historyProvider.then(
      function(history) {
        history.wasCopied(testFileEntry, SPACE_CAMP).then(assertFalse);
      });

  reportPromise(testPromise, callback);
}

function testHistoryWasCopiedTrueForKnownEntryLoadedFromStorage(callback) {
  // TestRecordWriter is pre-configured with this entry.
  testPromise = historyProvider.then(
      function(history) {
        history.wasCopied(testFileEntry, GOOGLE_DRIVE).then(assertTrue);
      });

  reportPromise(testPromise, callback);
}

function testHistoryWasImportedTrueForKnownEntrySetAtRuntime(callback) {
  testPromise = historyProvider.then(
      function(history) {
        history.markImported(testFileEntry, SPACE_CAMP).then(
            function() {
              history.wasImported(testFileEntry, SPACE_CAMP).then(assertTrue);
            });
      });

  reportPromise(testPromise, callback);
}

function testCopyChangeFiresChangedEvent(callback) {
  testPromise = historyProvider.then(
      function(history) {
        var recorder = new TestCallRecorder();
        history.addObserver(recorder.callback);
        history.markCopied(testFileEntry, SPACE_CAMP, 'url1').then(
            function() {
              Promise.resolve()
                  .then(
                      function() {
                        recorder.assertCallCount(1);
                        assertEquals(
                            importer.ImportHistory.State.COPIED,
                            recorder.getLastArguments()[0]['state']);
                      });
            });
      });

  reportPromise(testPromise, callback);
}

function testHistoryWasImportedFalseForUnknownEntry(callback) {
  // TestRecordWriter is pre-configured with a Space Cloud entry
  // but not for this file.
  testPromise = historyProvider.then(
      function(history) {
        history.wasImported(testFileEntry, SPACE_CAMP).then(assertFalse);
      });

  reportPromise(testPromise, callback);
}

function testHistoryWasImportedTrueForKnownEntryLoadedFromStorage(callback) {
  // TestRecordWriter is pre-configured with this entry.
  testPromise = historyProvider.then(
      function(history) {
        history.wasImported(testFileEntry, GOOGLE_DRIVE).then(assertTrue);
      });

  reportPromise(testPromise, callback);
}

function testHistoryWasImportedTrueForKnownEntrySetAtRuntime(callback) {
  testPromise = historyProvider.then(
      function(history) {
        history.markImported(testFileEntry, SPACE_CAMP).then(
            function() {
              history.wasImported(testFileEntry, SPACE_CAMP).then(assertTrue);
            });
      });

  reportPromise(testPromise, callback);
}

function testImportChangeFiresChangedEvent(callback) {
  testPromise = historyProvider.then(
      function(history) {
        var recorder = new TestCallRecorder();
        history.addObserver(recorder.callback);
        history.markImported(testFileEntry, SPACE_CAMP).then(
            function() {
              Promise.resolve()
                  .then(
                      function() {
                        recorder.assertCallCount(1);
                        assertEquals(
                            importer.ImportHistory.State.IMPORTED,
                            recorder.getLastArguments()[0]['state']);
                      });
            });
      });

  reportPromise(testPromise, callback);
}

function testHistoryObserverUnsubscribe(callback) {
  testPromise = historyProvider.then(
      function(history) {
        var recorder = new TestCallRecorder();
        history.addObserver(recorder.callback);
        history.removeObserver(recorder.callback);

        var promises = [];
        promises.push(history.markCopied(testFileEntry, SPACE_CAMP, 'url2'));
        promises.push(history.markImported(testFileEntry, SPACE_CAMP));
        Promise.all(promises).then(
            function() {
              Promise.resolve()
                  .then(
                      function() {
                        recorder.assertCallCount(0);
                      });
            });
      });

  reportPromise(testPromise, callback);
}

function testRecordStorageRemembersPreviouslyWrittenRecords(callback) {
  testPromise = createRealStorage('recordStorageTest.data')
      .then(
          function(storage) {
            storage.write(['abc', '123']).then(
                function() {
                  storage.readAll().then(
                      function(records) {
                        assertTrue(records.length != 1);
                      });
                });
            });

  reportPromise(testPromise, callback);
}

function testHistoryLoaderIntegration(callback) {

  /** @type {!SyncFileEntryProvider|undefined} */
  var fileProvider;

  /** @type {!HistoryLoader|undefined} */
  var loader;

  /** @type {!importer.ImportHistory|undefined} */
  var history;

  /** @type {!TestSyncFileEntryProvider|undefined} */
  var syncFileProvider;

  testPromise = createFileEntry('historyLoaderTest.data')
      .then(
          /**
           * @param  {!FileEntry} fileEntry
           * @return {!Promise.<!<Array.<!importer.ImportHistory>>}
           */
          function(fileEntry) {
            syncFileProvider = new TestSyncFileEntryProvider(fileEntry);
            loader = new importer.SynchronizedHistoryLoader(syncFileProvider);
            // Used to write new data to the "sync" file...data to be
            // refreshed by the the non-remote history instance.
            var remoteLoader = new importer.SynchronizedHistoryLoader(
                new TestSyncFileEntryProvider(fileEntry));

            var promises = [];
            promises.push(loader.getHistory());
            promises.push(remoteLoader.getHistory());
            return Promise.all(promises);
          })
      .then(
          /**
           * @param {!<Array.<!importer.ImportHistory>} histories
           * @return {!Promise.<?>}
           */
          function(histories) {
            history = histories[0];
            var remoteHistory = histories[1];
            return remoteHistory.markImported(testFileEntry, SPACE_CAMP);
          })
      .then(
          /**
           * @return {!Promise.<?>} Resolves
           *     when history has been reloaded.
           */
          function() {
            syncFileProvider.fireSyncEvent();
          })
      // TODO(smckay): Add markCopied support once FileWriter issues
      // are resolved.
      .then(
          /**
           * @return {!Promise.<boolean>} Resolves with true if the
           *     entry was previously marked as imported.
           */
          function() {
            return history.wasImported(testFileEntry, SPACE_CAMP);
          })
      .then(
          function(wasImported) {
            assertTrue(wasImported);
          });

  reportPromise(testPromise, callback);
}

/**
 * Shared error handler.
 * @param {function()} callback
 * @param {*} error
 */
function handleError(callback, error) {
  console.error(error && (error.stack || error) || 'Unspecified test error.');
  callback(/* error */ true);
}

/**
 * @param {string} fileName
 * @return {!Promise.<!FileEntry>}
 */
function createRealStorage(fileName) {
  return createFileEntry(fileName)
      .then(
          function(fileEntry) {
            return new importer.FileEntryRecordStorage(fileEntry);
          });
}

/**
 * Creates a *real* FileEntry in the DOM filesystem.
 *
 * @param {string} fileName
 * @return {!Promise.<FileEntry>}
 */
function createFileEntry(fileName) {
  return new Promise(
      function(resolve, reject) {
        var onFileSystemReady = function(fileSystem) {
          fileSystem.root.getFile(
              fileName,
              {
                create: true,
                exclusive: false
              },
              resolve,
              reject);
        };

        window.webkitRequestFileSystem(
            TEMPORARY,
            1024 * 1024,
            onFileSystemReady,
            reject);
      });
}

/**
 * In-memory test implementation of {@code RecordStorage}.
 *
 * @constructor
 * @implements {importer.RecordStorage}
 * @struct
 */
var TestRecordStorage = function() {

  // Pre-populate the store with some "previously written" data <wink>.
  /** @private {!Array.<!Array.<string>>} */
  this.records_ = [
    [1, FILE_LAST_MODIFIED + '_' + FILE_SIZE, GOOGLE_DRIVE],
    [0, FILE_LAST_MODIFIED + '_' + FILE_SIZE, 'google-drive', 'url4'],
    [1, '99999_99999', SPACE_CAMP]
  ];

  /**
   * @override
   * @this {TestRecordStorage}
   */
  this.readAll = function() {
    return Promise.resolve(this.records_);
  };

  /**
   * @override
   * @this {TestRecordStorage}
   */
  this.write = function(record) {
    this.records_.push(record);
    return Promise.resolve();
  };
};

/**
 * Test implementation of SyncFileEntryProvider.
 *
 * @constructor
 * @implements {importer.SyncFileEntryProvider}
 * @final
 * @struct
 *
 * @param {!FileEntry} fileEntry
 */
var TestSyncFileEntryProvider = function(fileEntry) {
  /** @private {!FileEntry} */
  this.fileEntry_ = fileEntry;

  /** @private {function()} */
  this.listener_;

  /**
   * @override
   * @this {TestSyncFileEntryProvider}
   */
  this.addSyncListener = function(listener) {
    if (!!this.listener_) {
      throw new Error('Listener already set. Unexpected reality!');
    }
    this.listener_ = listener;
  };

  /**
   * @override
   * @this {TestSyncFileEntryProvider}
   */
  this.getSyncFileEntry = function() {
    return Promise.resolve(this.fileEntry_);
  };

  /**
   * @this {TestSyncFileEntryProvider}
   */
  this.fireSyncEvent = function() {
    this.listener_();
  };
};
