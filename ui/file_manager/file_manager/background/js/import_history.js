// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @struct
 *
 * @param {!RecordStorage} storage
 */
function ImportHistory(storage) {

  /** @private {!RecordStorage} */
  this.storage_ = storage;

  /** @private {!Object.<string, !Array.<string>>} */
  this.entries_ = {};

  /** @private {!Promise.<!ImportHistory>} */
  this.whenReady_ = this.refresh_();
}

/**
 * Loads history from disk and merges in any previously existing entries
 * that are not present in the newly loaded data. Should be called
 * when the file is changed by an external source.
 *
 * @return {!Promise.<!ImportHistory>} Resolves when history has been refreshed.
 * @private
 */
ImportHistory.prototype.refresh_ = function() {
  var oldEntries = this.entries_;
  this.entries_ = {};
  return this.storage_.readAll()
      .then(this.updateHistoryRecords_.bind(this))
      .then(this.mergeEntries_.bind(this, oldEntries))
      .then(
          /**
           * @return {!ImportHistory}
           * @this {ImportHistory}
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
ImportHistory.prototype.mergeEntries_ = function(entries) {
  var promises = [];
  Object.keys(entries).forEach(
      /**
       * @param {string} key
       * @this {ImportHistory}
       */
      function(key) {
        entries[key].forEach(
            /**
             * @param {string} key
             * @this {ImportHistory}
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
 * @return {!Promise.<!ImportHistory>} Resolves when history has been refreshed.
 */
ImportHistory.prototype.refresh = function() {
  this.whenReady_ = this.refresh_();
  return this.whenReady_;
};

/**
 * @return {!Promise.<!ImportHistory>}
 */
ImportHistory.prototype.whenReady = function() {
  return this.whenReady_;
};

/**
 * Adds a history entry to the in-memory history model.
 * @param {!Array.<!Array.<*>>} records
 * @private
 */
ImportHistory.prototype.updateHistoryRecords_ = function(records) {
  records.forEach(
      /**
       * @param {!Array.<*>} entry
       * @this {ImportHistory}
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
ImportHistory.prototype.updateHistoryRecord_ = function(key, destination) {
  if (key in this.entries_) {
    this.entries_[key].push(destination);
  } else {
    this.entries_[key] = [destination];
  }
};

/**
 * @param {!FileEntry} entry
 * @param {string} destination
 * @return {!Promise.<boolean>} Resolves with true if the FileEntry
 *     was previously imported to the specified destination.
 */
ImportHistory.prototype.wasImported = function(entry, destination) {
  return this.whenReady_
      .then(this.createKey_.bind(this, entry))
      .then(
          /**
           * @param {string} key
           * @return {!Promise.<boolean>}
           * @this {ImportHistory}
           */
          function(key) {
            return this.getDestinations_(key).indexOf(destination) >= 0;
          }.bind(this));
};

/**
 * @param {!FileEntry} entry
 * @param {string} destination
 * @return {!Promise.<?>} Resolves when the operation is completed.
 */
ImportHistory.prototype.markImported = function(entry, destination) {
  return this.whenReady_
      .then(this.createKey_.bind(this, entry))
      .then(
          /**
           * @param {string} key
           * @return {!Promise.<?>}
           * @this {ImportHistory}
           */
          function(key) {
            return this.addDestination_(destination, key);
          }.bind(this));
};

/**
 * @param {string} destination
 * @param {string} key
 * @return {!Promise.<?>} Resolves once the write has been completed.
 * @private
 */
ImportHistory.prototype.addDestination_ = function(destination, key) {
  this.updateHistoryRecord_(key, destination);
  return this.storage_.write([key, destination]);
};

/**
 * @param {string} key
 * @return {!Array.<string>} The list of previously noted
 *     destinations, or an empty array, if none.
 * @private
 */
ImportHistory.prototype.getDestinations_ = function(key) {
  return key in this.entries_ ? this.entries_[key] : [];
};

/**
 * @param {!FileEntry} fileEntry
 * @return {!Promise.<string>} Resolves with a the key is available.
 * @private
 */
ImportHistory.prototype.createKey_ = function(fileEntry) {
  var entry = new PromisaryFileEntry(fileEntry);
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {ImportHistory}
       */
      function(resolve, reject) {
        entry.getMetadata()
            .then(
                /**
                 * @param {!Object} metadata
                 * @return {!Promise.<string>}
                 * @this {ImportHistory}
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
 * Provider of lazy loaded ImportHistory. This is the main
 * access point for a fully prepared {@code ImportHistory} object.
 *
 * @interface
 */
function HistoryLoader() {}

/**
 * Instantiates an {@code ImportHistory} object and manages any
 * necessary ongoing maintenance of the object with respect to
 * its external dependencies.
 *
 * @see SynchronizedHistoryLoader for an example.
 *
 * @return {!Promise.<!ImportHistory>} Resolves when history instance is ready.
 */
HistoryLoader.prototype.loadHistory;

/**
 * Class responsible for lazy loading of {@code ImportHistory},
 * and reloading when the underlying data is updated (via sync).
 *
 * @constructor
 * @implements {HistoryLoader}
 * @struct
 *
 * @param {!SyncFileEntryProvider} fileProvider
 */
function SynchronizedHistoryLoader(fileProvider) {

  /** @private {!SyncFileEntryProvider} */
  this.fileProvider_ = fileProvider;

  /** @private {!ImportHistory|undefined} */
  this.history_;
}

/** @override */
SynchronizedHistoryLoader.prototype.loadHistory = function() {
  if (this.history_) {
    return this.history_.whenReady();
  }

  this.fileProvider_.addSyncListener(
      this.onSyncedDataChanged_.bind(this));

  return this.fileProvider_.getSyncFileEntry()
      .then(
          /**
           * @param {!FileEntry} fileEntry
           * @return {!Promise.<!ImportHistory>}
           * @this {SynchronizedHistoryLoader}
           */
          function(fileEntry) {
            var storage = new FileEntryRecordStorage(fileEntry);
            var history = new ImportHistory(storage);
            return history.refresh().then(
                /**
                 * @return {!ImportHistory}
                 * @this {SynchronizedHistoryLoader}
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
SynchronizedHistoryLoader.prototype.onSyncedDataChanged_ = function() {
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
function SyncFileEntryProvider() {}

/**
 * Adds a listener to be notified when the the FileEntry owned/managed
 * by this class is updated via sync.
 *
 * @param {function()} syncListener
 */
SyncFileEntryProvider.prototype.addSyncListener;

/**
 * Provides accsess to the sync FileEntry owned/managed by this class.
 *
 * @return {!Promise.<!FileEntry>}
 */
SyncFileEntryProvider.prototype.getSyncFileEntry;

/**
 * Factory for synchronized files based on chrome.syncFileSystem.
 *
 * @constructor
 * @implements {SyncFileEntryProvider}
 * @struct
 */
function ChromeSyncFileEntryProvider() {

  /** @private {!Array.<function()>} */
  this.syncListeners_ = [];

  /** @private {!Promise.<!FileEntry>|undefined} */
  this.fileEntryPromise_;
}

/** @private @const {string} */
ChromeSyncFileEntryProvider.FILE_NAME_ = 'import-history.data';

/**
 * Wraps chrome.syncFileSystem.onFileStatusChanged
 * so that we can report to our listeners when our file has changed.
 * @private
 */
ChromeSyncFileEntryProvider.prototype.monitorSyncEvents_ = function() {
  chrome.syncFileSystem.onFileStatusChanged.addListener(
      this.handleSyncEvent_.bind(this));
};

/** @override */
ChromeSyncFileEntryProvider.prototype.addSyncListener = function(listener) {
  if (this.syncListeners_.indexOf(listener) === -1) {
    this.syncListeners_.push(listener);
  }
};

/** @override */
ChromeSyncFileEntryProvider.prototype.getSyncFileEntry = function() {
  if (this.fileEntryPromise_) {
    return this.fileEntryPromise_;
  };

  this.fileEntryPromise_ = this.getFileSystem_()
      .then(
          /**
           * @param {!FileSystem} fileSystem
           * @return {!Promise.<!FileEntry>}
           * @this {ChromeSyncFileEntryProvider}
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
ChromeSyncFileEntryProvider.prototype.getFileSystem_ = function() {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {ChromeSyncFileEntryProvider}
       */
      function(resolve, reject) {
        chrome.syncFileSystem.requestFileSystem(
            /**
              * @param {FileSystem} fileSystem
              * @this {ChromeSyncFileEntryProvider}
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
ChromeSyncFileEntryProvider.prototype.getFileEntry_ = function(fileSystem) {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {ChromeSyncFileEntryProvider}
       */
      function(resolve, reject) {
        fileSystem.root.getFile(
            ChromeSyncFileEntryProvider.FILE_NAME_,
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
ChromeSyncFileEntryProvider.prototype.handleSyncEvent_ = function(event) {
  if (!this.fileEntryPromise_) {
    return;
  }

  this.fileEntryPromise_.then(
      /**
       * @param {!FileEntry} fileEntry
       * @this {ChromeSyncFileEntryProvider}
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
             * @this {ChromeSyncFileEntryProvider}
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
function RecordStorage() {}

/**
 * Adds a new record.
 *
 * @param {!Array.<*>} record
 * @return {!Promise.<?>} Resolves when record is added.
 */
RecordStorage.prototype.write;

/**
 * Reads all records.
 *
 * @return {!Promise.<!Array.<!Array.<*>>>}
 */
RecordStorage.prototype.readAll;

/**
 * A {@code RecordStore} that persists data in a {@code FileEntry}.
 *
 * @param {!FileEntry} fileEntry
 *
 * @constructor
 * @implements {RecordStorage}
 * @struct
 */
function FileEntryRecordStorage(fileEntry) {
  /** @private {!PromisaryFileEntry} */
  this.fileEntry_ = new PromisaryFileEntry(fileEntry);
}

/** @override */
FileEntryRecordStorage.prototype.write = function(record) {
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
FileEntryRecordStorage.prototype.writeRecord_ = function(record, writer) {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {FileEntryRecordStorage}
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
FileEntryRecordStorage.prototype.readAll = function() {
  return this.fileEntry_.file()
      .then(
          this.readFileAsText_.bind(this),
          /**
           * @return {string}
           * @this {FileEntryRecordStorage}
           */
          function() {
            console.error('Unable to read from history file.');
            return '';
          }.bind(this))
      .then(
          /**
           * @param {string} fileContents
           * @this {FileEntryRecordStorage}
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
FileEntryRecordStorage.prototype.readFileAsText_ = function(file) {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {FileEntryRecordStorage}
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
FileEntryRecordStorage.prototype.parse_ = function(text) {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {FileEntryRecordStorage}
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
function PromisaryFileEntry(fileEntry) {
  /** @private {!FileEntry} */
  this.fileEntry_ = fileEntry;
}

/**
 * A "Promisary" wrapper around entry.getWriter.
 * @return {!Promise.<!FileWriter>}
 */
PromisaryFileEntry.prototype.createWriter = function() {
  return new Promise(this.fileEntry_.createWriter.bind(this.fileEntry_));
};

/**
 * A "Promisary" wrapper around entry.file.
 * @return {!Promise.<!File>}
 */
PromisaryFileEntry.prototype.file = function() {
  return new Promise(this.fileEntry_.file.bind(this.fileEntry_));
};

/**
 * @return {!Promise.<!Object>}
 */
PromisaryFileEntry.prototype.getMetadata = function() {
  return new Promise(this.fileEntry_.getMetadata.bind(this.fileEntry_));
};
