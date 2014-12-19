// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * A persistent data store for Cloud Import history information.
 *
 * @interface
 */
importer.ImportHistory = function() {};

/**
 * @param {!FileEntry} entry
 * @param {!importer.Destination} destination
 * @return {!Promise.<boolean>} Resolves with true if the FileEntry
 *     was previously imported to the specified destination.
 */
importer.ImportHistory.prototype.wasImported;

/**
 * @param {!FileEntry} entry
 * @param {!importer.Destination} destination
 * @return {!Promise.<?>} Resolves when the operation is completed.
 */
importer.ImportHistory.prototype.markImported;

/**
 * Adds an observer, which will be notified when cloud import history changes.
 *
 * @param {!importer.ImportHistory.Observer} observer
 */
importer.ImportHistory.prototype.addObserver;

/**
 * Remove a previously registered observer.
 *
 * @param {!importer.ImportHistory.Observer} observer
 */
importer.ImportHistory.prototype.removeObserver;

/** @enum {string} */
importer.ImportHistory.State = {
  'IMPORTED': 'imported'
};

/**
 * @typedef {{
 *   state: !importer.ImportHistory.State,
 *   entry: !FileEntry
 * }}
 */
importer.ImportHistory.ChangedEvent;

/** @typedef {function(!importer.ImportHistory.ChangedEvent)} */
importer.ImportHistory.Observer;

/**
 * An dummy {@code ImportHistory} implementation. This class can conveniently
 * be used when cloud import is disabled.
 * @param {boolean} answer The value to answer all {@code wasImported}
 *     queries with.
 *
 * @constructor
 * @implements {importer.HistoryLoader}
 * @implements {importer.ImportHistory}
 */
importer.DummyImportHistory = function(answer) {
  /** @private {boolean} */
  this.answer_ = answer;
};

/** @override */
importer.DummyImportHistory.prototype.getHistory = function() {
  return Promise.resolve(this);
};

/** @override */
importer.DummyImportHistory.prototype.wasImported =
    function(entry, destination) {
  return Promise.resolve(this.answer_);
};

/** @override */
importer.DummyImportHistory.prototype.markImported =
    function(entry, destination) {
  return Promise.resolve();
};

/** @override */
importer.DummyImportHistory.prototype.addObserver = function(observer) {};

/** @override */
importer.DummyImportHistory.prototype.removeObserver = function(observer) {};

/**
 * An {@code ImportHistory} implementation that reads from and
 * writes to a storage object.
 *
 * @constructor
 * @implements {importer.ImportHistory}
 * @struct
 *
 * @param {!importer.RecordStorage} storage
 */
importer.PersistentImportHistory = function(storage) {

  /** @private {!importer.RecordStorage} */
  this.storage_ = storage;

  /**
   * An in-memory representation of import history.
   * The first value is the "key" (as generated internally
   * from a file entry).
   * @private {!Object.<string, !Array.<importer.Destination>>}
   */
  this.entries_ = {};

  /** @private {!Array.<!importer.ImportHistory.Observer>} */
  this.observers_ = [];

  /** @private {!Promise.<!importer.PersistentImportHistory>} */
  this.whenReady_ = this.refresh_();
};

/**
 * Loads history from storage and merges in any previously existing entries
 * that are not present in the newly loaded data. Should be called
 * when the storage is externally changed.
 *
 * @return {!Promise.<!importer.PersistentImportHistory>} Resolves when history
 *     has been refreshed.
 * @private
 */
importer.PersistentImportHistory.prototype.refresh_ = function() {
  var oldEntries = this.entries_;
  this.entries_ = {};
  return this.storage_.readAll()
      .then(this.updateHistoryRecords_.bind(this))
      .then(this.mergeEntries_.bind(this, oldEntries))
      .then(
          /**
           * @return {!importer.PersistentImportHistory}
           * @this {importer.PersistentImportHistory}
           */
          function() {
            return this;
          }.bind(this));
};

/**
 * Adds all entries not already present in history.
 *
 * @param {!Object.<string, !Array.<string>>} entries
 * @return {!Promise.<?>} Resolves once all updates are completed.
 * @private
 */
importer.PersistentImportHistory.prototype.mergeEntries_ = function(entries) {
  var promises = [];
  Object.keys(entries).forEach(
      /**
       * @param {string} key
       * @this {importer.PersistentImportHistory}
       */
      function(key) {
        entries[key].forEach(
            /**
             * @param {string} key
             * @this {importer.PersistentImportHistory}
             */
            function(destination) {
              if (this.getDestinations_(key).indexOf(destination) >= 0) {
                this.updateHistoryRecord_(key, destination);
                promises.push(this.storage_.write([key, destination]));
              }
        }.bind(this));
      }.bind(this));
  return Promise.all(promises);
};

/**
 * Reloads history from disk. Should be called when the file
 * is changed by an external source.
 *
 * @return {!Promise.<!importer.PersistentImportHistory>} Resolves when
 *     history has been refreshed.
 */
importer.PersistentImportHistory.prototype.refresh = function() {
  this.whenReady_ = this.refresh_();
  return this.whenReady_;
};

/**
 * @return {!Promise.<!importer.ImportHistory>}
 */
importer.PersistentImportHistory.prototype.whenReady = function() {
  return /** @type {!Promise.<!importer.ImportHistory>} */ (this.whenReady_);
};

/**
 * Adds a history entry to the in-memory history model.
 * @param {!Array.<!Array.<*>>} records
 * @private
 */
importer.PersistentImportHistory.prototype.updateHistoryRecords_ =
    function(records) {
  records.forEach(
      /**
       * @param {!Array.<*>} entry
       * @this {importer.PersistentImportHistory}
       */
      function(record) {
        this.updateHistoryRecord_(record[0], record[1]);
      }.bind(this));
};

/**
 * Adds a history entry to the in-memory history model.
 * @param {string} key
 * @param {string} destination
 * @private
 */
importer.PersistentImportHistory.prototype.updateHistoryRecord_ =
    function(key, destination) {
  if (key in this.entries_) {
    this.entries_[key].push(destination);
  } else {
    this.entries_[key] = [destination];
  }
};

/** @override */
importer.PersistentImportHistory.prototype.wasImported =
    function(entry, destination) {
  return this.whenReady_
      .then(this.createKey_.bind(this, entry))
      .then(
          /**
           * @param {string} key
           * @return {!Promise.<boolean>}
           * @this {importer.PersistentImportHistory}
           */
          function(key) {
            return this.getDestinations_(key).indexOf(destination) >= 0;
          }.bind(this));
};

/** @override */
importer.PersistentImportHistory.prototype.markImported =
    function(entry, destination) {
  return this.whenReady_
      .then(this.createKey_.bind(this, entry))
      .then(
          /**
           * @param {string} key
           * @return {!Promise.<?>}
           * @this {importer.ImportHistory}
           */
          function(key) {
            return this.addDestination_(destination, key);
          }.bind(this))
      .then(this.notifyObservers_.bind(this, entry));
};

/** @override */
importer.PersistentImportHistory.prototype.addObserver =
    function(observer) {
  this.observers_.push(observer);
};

/** @override */
importer.PersistentImportHistory.prototype.removeObserver =
    function(observer) {
  var index = this.observers_.indexOf(observer);
  if (index > -1) {
    this.observers_.splice(index, 1);
  } else {
    console.warn('Ignoring request to remove observer that is not registered.');
  }
};

/**
 * @param {!FileEntry} subject
 * @private
 */
importer.PersistentImportHistory.prototype.notifyObservers_ =
    function(subject) {
  this.observers_.forEach(
      function(observer) {
        observer({
          state: importer.ImportHistory.State.IMPORTED,
          entry: subject
        });
      }.bind(this));
};

/**
 * @param {string} destination
 * @param {string} key
 * @return {!Promise.<?>} Resolves once the write has been completed.
 * @private
 */
importer.PersistentImportHistory.prototype.addDestination_ =
    function(destination, key) {
  this.updateHistoryRecord_(key, destination);
  return this.storage_.write([key, destination]);
};

/**
 * @param {string} key
 * @return {!Array.<string>} The list of previously noted
 *     destinations, or an empty array, if none.
 * @private
 */
importer.PersistentImportHistory.prototype.getDestinations_ = function(key) {
  return key in this.entries_ ? this.entries_[key] : [];
};

/**
 * @param {!FileEntry} fileEntry
 * @return {!Promise.<string>} Resolves with a the key is available.
 * @private
 */
importer.PersistentImportHistory.prototype.createKey_ = function(fileEntry) {
  var entry = new importer.PromisingFileEntry(fileEntry);
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {importer.PersistentImportHistory}
       */
      function(resolve, reject) {
        entry.getMetadata()
            .then(
                /**
                 * @param {!Object} metadata
                 * @return {!Promise.<string>}
                 * @this {importer.PersistentImportHistory}
                 */
                function(metadata) {
                  if (!('modificationTime' in metadata)) {
                    reject('File entry missing "modificationTime" field.');
                  } else if (!('size' in metadata)) {
                    reject('File entry missing "size" field.');
                  } else {
                    resolve(
                      metadata['modificationTime'] + '_' + metadata['size']);
                  }
                }.bind(this));
      }.bind(this));
};

/**
 * Provider of lazy loaded importer.ImportHistory. This is the main
 * access point for a fully prepared {@code importer.ImportHistory} object.
 *
 * @interface
 */
importer.HistoryLoader = function() {};

/**
 * Instantiates an {@code importer.ImportHistory} object and manages any
 * necessary ongoing maintenance of the object with respect to
 * its external dependencies.
 *
 * @see importer.SynchronizedHistoryLoader for an example.
 *
 * @return {!Promise.<!importer.ImportHistory>} Resolves when history instance
 *     is ready.
 */
importer.HistoryLoader.prototype.getHistory;

/**
 * Class responsible for lazy loading of {@code importer.ImportHistory},
 * and reloading when the underlying data is updated (via sync).
 *
 * @constructor
 * @implements {importer.HistoryLoader}
 * @struct
 *
 * @param {!importer.SyncFileEntryProvider} fileProvider
 */
importer.SynchronizedHistoryLoader = function(fileProvider) {

  /** @private {!importer.SyncFileEntryProvider} */
  this.fileProvider_ = fileProvider;

  /** @private {!importer.PersistentImportHistory|undefined} */
  this.history_;
};

/** @override */
importer.SynchronizedHistoryLoader.prototype.getHistory = function() {
  if (this.history_) {
    return this.history_.whenReady();
  }

  this.fileProvider_.addSyncListener(
      this.onSyncedDataChanged_.bind(this));

  return this.fileProvider_.getSyncFileEntry()
      .then(
          /**
           * @param {!FileEntry} fileEntry
           * @return {!Promise.<!importer.ImportHistory>}
           * @this {importer.SynchronizedHistoryLoader}
           */
          function(fileEntry) {
            var storage = new importer.FileEntryRecordStorage(fileEntry);
            var history = new importer.PersistentImportHistory(storage);
            return history.refresh().then(
                /**
                 * @return {!importer.ImportHistory}
                 * @this {importer.SynchronizedHistoryLoader}
                 */
                function() {
                  this.history_ = history;
                  return history;
                }.bind(this));
          }.bind(this));
};

/**
 * Handles file sync events, by simply reloading history. The presumption
 * is that 99% of the time these events will basically be happening when
 * there is no active import process.
 *
 * @private
 */
importer.SynchronizedHistoryLoader.prototype.onSyncedDataChanged_ =
    function() {
  if (this.history_) {
    this.history_.refresh();  // Reload history entries.
  }
};

/**
 * Factory interface for creating/accessing synced {@code FileEntry}
 * instances and listening to sync events on those files.
 *
 * @interface
 */
importer.SyncFileEntryProvider = function() {};

/**
 * Adds a listener to be notified when the the FileEntry owned/managed
 * by this class is updated via sync.
 *
 * @param {function()} syncListener
 */
importer.SyncFileEntryProvider.prototype.addSyncListener;

/**
 * Provides accsess to the sync FileEntry owned/managed by this class.
 *
 * @return {!Promise.<!FileEntry>}
 */
importer.SyncFileEntryProvider.prototype.getSyncFileEntry;

/**
 * Factory for synchronized files based on chrome.syncFileSystem.
 *
 * @constructor
 * @implements {importer.SyncFileEntryProvider}
 * @struct
 */
importer.ChromeSyncFileEntryProvider = function() {

  /** @private {!Array.<function()>} */
  this.syncListeners_ = [];

  /** @private {!Promise.<!FileEntry>|undefined} */
  this.fileEntryPromise_;
};

/** @private @const {string} */
importer.ChromeSyncFileEntryProvider.FILE_NAME_ = 'import-history.data';

/**
 * Wraps chrome.syncFileSystem.onFileStatusChanged
 * so that we can report to our listeners when our file has changed.
 * @private
 */
importer.ChromeSyncFileEntryProvider.prototype.monitorSyncEvents_ =
    function() {
  chrome.syncFileSystem.onFileStatusChanged.addListener(
      this.handleSyncEvent_.bind(this));
};

/** @override */
importer.ChromeSyncFileEntryProvider.prototype.addSyncListener =
    function(listener) {
  if (this.syncListeners_.indexOf(listener) === -1) {
    this.syncListeners_.push(listener);
  }
};

/** @override */
importer.ChromeSyncFileEntryProvider.prototype.getSyncFileEntry = function() {
  if (this.fileEntryPromise_) {
    return this.fileEntryPromise_;
  };

  this.fileEntryPromise_ = this.getFileSystem_()
      .then(
          /**
           * @param {!FileSystem} fileSystem
           * @return {!Promise.<!FileEntry>}
           * @this {importer.ChromeSyncFileEntryProvider}
           */
          function(fileSystem) {
            return this.getFileEntry_(fileSystem);
          }.bind(this));

  return this.fileEntryPromise_;
};

/**
 * Wraps chrome.syncFileSystem in a Promise.
 *
 * @return {!Promise.<!FileSystem>}
 * @private
 */
importer.ChromeSyncFileEntryProvider.prototype.getFileSystem_ = function() {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {importer.ChromeSyncFileEntryProvider}
       */
      function(resolve, reject) {
        chrome.syncFileSystem.requestFileSystem(
            /**
              * @param {FileSystem} fileSystem
              * @this {importer.ChromeSyncFileEntryProvider}
              */
            function(fileSystem) {
              if (chrome.runtime.lastError) {
                reject(chrome.runtime.lastError.message);
              } else {
                resolve(/** @type {!FileSystem} */ (fileSystem));
              }
            });
      }.bind(this));
};

/**
 * @param {!FileSystem} fileSystem
 * @return {!Promise.<!FileEntry>}
 * @private
 */
importer.ChromeSyncFileEntryProvider.prototype.getFileEntry_ =
    function(fileSystem) {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {importer.ChromeSyncFileEntryProvider}
       */
      function(resolve, reject) {
        fileSystem.root.getFile(
            importer.ChromeSyncFileEntryProvider.FILE_NAME_,
            {
              create: true,
              exclusive: false
            },
            resolve,
            reject);
      }.bind(this));
};

/**
 * Handles sync events. Checks to see if the event is for the file
 * we track, and sync-direction, and if so, notifies syncListeners.
 *
 * @see https://developer.chrome.com/apps/syncFileSystem
 *     #event-onFileStatusChanged
 *
 * @param {!Object} event Having a structure not unlike: {
 *     fileEntry: Entry,
 *     status: string,
 *     action: (string|undefined),
 *     direction: (string|undefined)}
 *
 * @private
 */
importer.ChromeSyncFileEntryProvider.prototype.handleSyncEvent_ =
    function(event) {
  if (!this.fileEntryPromise_) {
    return;
  }

  this.fileEntryPromise_.then(
      /**
       * @param {!FileEntry} fileEntry
       * @this {importer.ChromeSyncFileEntryProvider}
       */
      function(fileEntry) {
        if (event['fileEntry'].fullPath !== fileEntry.fullPath) {
          return;
        }

        if (event.direction && event.direction !== 'remote_to_local') {
          return;
        }

        if (event.action && event.action !== 'updated') {
          console.error(
            'Unexpected sync event action for history file: ' + event.action);
          return;
        }

        this.syncListeners_.forEach(
            /**
             * @param {function()} listener
             * @this {importer.ChromeSyncFileEntryProvider}
             */
            function(listener) {
              // Notify by way of a promise so that it is fully asynchronous
              // (which can rationalize testing).
              Promise.resolve().then(listener);
            }.bind(this));
      }.bind(this));
};

/**
 * An simple record storage mechanism.
 *
 * @interface
 */
importer.RecordStorage = function() {};

/**
 * Adds a new record.
 *
 * @param {!Array.<*>} record
 * @return {!Promise.<?>} Resolves when record is added.
 */
importer.RecordStorage.prototype.write;

/**
 * Reads all records.
 *
 * @return {!Promise.<!Array.<!Array.<*>>>}
 */
importer.RecordStorage.prototype.readAll;

/**
 * A {@code RecordStore} that persists data in a {@code FileEntry}.
 *
 * @param {!FileEntry} fileEntry
 *
 * @constructor
 * @implements {importer.RecordStorage}
 * @struct
 */
importer.FileEntryRecordStorage = function(fileEntry) {
  /** @private {!importer.PromisingFileEntry} */
  this.fileEntry_ = new importer.PromisingFileEntry(fileEntry);
};

/** @override */
importer.FileEntryRecordStorage.prototype.write = function(record) {
  // TODO(smckay): should we make an effort to reuse a file writer?
  return this.fileEntry_.createWriter()
      .then(this.writeRecord_.bind(this, record));
};

/**
 * Appends a new record to the end of the file.
 *
 * @param {!Object} record
 * @param {!FileWriter} writer
 * @return {!Promise.<?>} Resolves when write is complete.
 * @private
 */
importer.FileEntryRecordStorage.prototype.writeRecord_ =
    function(record, writer) {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {importer.FileEntryRecordStorage}
       */
      function(resolve, reject) {
        var blob = new Blob(
            [JSON.stringify(record) + ',\n'],
            {type: 'text/plain; charset=UTF-8'});

        writer.onwriteend = resolve;
        writer.onerror = reject;

        writer.seek(writer.length);
        writer.write(blob);
      }.bind(this));
};

/** @override */
importer.FileEntryRecordStorage.prototype.readAll = function() {
  return this.fileEntry_.file()
      .then(
          this.readFileAsText_.bind(this),
          /**
           * @return {string}
           * @this {importer.FileEntryRecordStorage}
           */
          function() {
            console.error('Unable to read from history file.');
            return '';
          }.bind(this))
      .then(
          /**
           * @param {string} fileContents
           * @this {importer.FileEntryRecordStorage}
           */
          function(fileContents) {
            return this.parse_(fileContents);
          }.bind(this));
};

/**
 * Reads the entire entry as a single string value.
 *
 * @param {!File} file
 * @return {!Promise.<string>}
 * @private
 */
importer.FileEntryRecordStorage.prototype.readFileAsText_ = function(file) {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {importer.FileEntryRecordStorage}
       */
      function(resolve, reject) {
        var reader = new FileReader();

        reader.onloadend = function() {
          if (reader.error) {
            console.error(reader.error);
            reject();
          } else {
            resolve(reader.result);
          }
        }.bind(this);

        reader.onerror = function(error) {
            console.error(error);
          reject(error);
        }.bind(this);

        reader.readAsText(file);
      }.bind(this));
};

/**
 * Parses the text.
 *
 * @param {string} text
 * @return {!Promise.<!Array.<!Array.<*>>>}
 * @private
 */
importer.FileEntryRecordStorage.prototype.parse_ = function(text) {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {importer.FileEntryRecordStorage}
       */
      function(resolve, reject) {
        if (text.length === 0) {
          resolve([]);
        } else {
          // Dress up the contents of the file like an array,
          // so the JSON object can parse it using JSON.parse.
          // That means we need to both:
          //   1) Strip the trailing ',\n' from the last record
          //   2) Surround the whole string in brackets.
          // NOTE: JSON.parse is WAY faster than parsing this
          // ourselves in javascript.
          var json = '[' + text.substring(0, text.length - 2) + ']';
          resolve(JSON.parse(json));
        }
      }.bind(this));
};

/**
 * A wrapper for FileEntry that provides Promises.
 *
 * @param {!FileEntry} fileEntry
 *
 * @constructor
 * @struct
 */
importer.PromisingFileEntry = function(fileEntry) {
  /** @private {!FileEntry} */
  this.fileEntry_ = fileEntry;
};

/**
 * A "Promisary" wrapper around entry.getWriter.
 * @return {!Promise.<!FileWriter>}
 */
importer.PromisingFileEntry.prototype.createWriter = function() {
  return new Promise(this.fileEntry_.createWriter.bind(this.fileEntry_));
};

/**
 * A "Promisary" wrapper around entry.file.
 * @return {!Promise.<!File>}
 */
importer.PromisingFileEntry.prototype.file = function() {
  return new Promise(this.fileEntry_.file.bind(this.fileEntry_));
};

/**
 * @return {!Promise.<!Object>}
 */
importer.PromisingFileEntry.prototype.getMetadata = function() {
  return new Promise(this.fileEntry_.getMetadata.bind(this.fileEntry_));
};
