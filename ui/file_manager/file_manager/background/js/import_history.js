// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @constructor
 *
 * @param {!RecordStorage} storage
 */
function ImportHistory(storage) {

  /** @private {!RecordStorage} */
  this.storage_ = storage;

  /** @private {!Object.<string, !Array.<string>>} */
  this.history_ = {};
}

/**
 * Prints an error to the console.
 *
 * @param  {!Error} error
 * @private
 */
ImportHistory.handleError_ = function(error) {
  console.error(error.stack || error);
};

/**
 * Use this factory method to get a fully ready instance of ImportHistory.
 *
 * @param {!RecordStorage} storage
 *
 * @return {!Promise.<!ImportHistory>} Resolves when history instance is ready.
 */
ImportHistory.load = function(storage) {
  var history = new ImportHistory(storage);
  return history.reload().then(
    function() {
      return history;
    });
};

/**
 * Reloads history from disk. Should be called when the file
 * is synced.
 *
 * @return {!Promise} Resolves when history has been loaded.
 */
ImportHistory.prototype.reload = function() {
  this.history_ = {};
  return this.storage_.readAll()
      .then(
          function(entries) {
            entries.forEach(
                function(entry) {
                  this.updateHistoryRecord_(entry[0], entry[1]);
                }.bind(this));
          }.bind(this))
      .catch(ImportHistory.handleError_);
};

/**
 * Adds a history entry to the in-memory history model.
 * @param {string} key
 * @param {string} destination
 * @private
 */
ImportHistory.prototype.updateHistoryRecord_ = function(key, destination) {
  if (key in this.history_) {
    this.history_[key].push(destination);
  } else {
    this.history_[key] = [destination];
  }
};

/**
 * @param {!FileEntry} entry
 * @param {string} destination
 * @return {!Promise.<boolean>} Settles with true if the FileEntry
 *     was previously imported to the specified destination.
 */
ImportHistory.prototype.wasImported = function(entry, destination) {
  return this.createKey_(entry)
      .then(
          function(key) {
            return this.getDestinations_(key).indexOf(destination) >= 0;
          }.bind(this))
      .catch(ImportHistory.handleError_);
};

/**
 * @param {!FileEntry} entry
 * @param {string} destination
 * @return {!Promise.<boolean>} Settles with true if the FileEntry
 *     was previously imported to the specified destination.
 */
ImportHistory.prototype.markImported = function(entry, destination) {
  return this.createKey_(entry)
      .then(this.addDestination_.bind(this, destination))
      .catch(ImportHistory.handleError_);
};

/**
 * @param {string} key
 * @return {!Array.<string>} The list of previously noted
 *     destinations, or an empty array, if none.
 * @private
 */
ImportHistory.prototype.getDestinations_ = function(key) {
  return key in this.history_ ? this.history_[key] : [];
};

/**
 * @param {string} destination
 * @param {string} key
 * @return {!Promise}
 * @private
 */
ImportHistory.prototype.addDestination_ = function(destination, key) {
  this.updateHistoryRecord_(key, destination);
  return this.storage_.write([key, destination]);
};

/**
 * @param {!FileEntry} entry
 * @return {!Promise.<string>} Settles with a the key is available.
 * @private
 */
ImportHistory.prototype.createKey_ = function(fileEntry) {
  var entry = new PromisaryFileEntry(fileEntry);
  return new Promise(
      function(resolve, reject) {
        entry.getMetadata()
            .then(
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
 * An simple record storage mechanism.
 *
 * @interface
 */
function RecordStorage(entry) {}

/**
 * Adds a new record.
 *
 * @param {!Array.<*>} record
 * @return {!Promise} Resolves when record is added.
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
 * @param {!FileEntry} entry
 *
 * @constructor
 * @implements {RecordStorage}
 */
function FileEntryRecordStorage(entry) {
  /** @private {!PromisaryFileEntry} */
  this.entry_ = new PromisaryFileEntry(entry);
}

/**
 * Prints an error to the console.
 *
 * @param  {!Error} error
 * @private
 */
FileEntryRecordStorage.handleError_ = function(error) {
  console.error(error.stack || error);
};

/** @override */
FileEntryRecordStorage.prototype.write = function(record) {
  // TODO(smckay): should we make an effort to reuse a file writer?
  return this.entry_.createWriter()
      .then(this.writeRecord_.bind(this, record))
      .catch(FileEntryRecordStorage.handleError_);
};

/**
 * Appends a new record to the end of the file.
 *
 * @param {!Object} record
 * @param {!FileWriter} writer
 * @return {!Promise} Resolves when write is complete.
 * @private
 */
FileEntryRecordStorage.prototype.writeRecord_ = function(record, writer) {
  return new Promise(
      function(resolve, reject) {
        var blob = new Blob(
            [JSON.stringify(record) + ',\n'],
            {type: 'text/plain; charset=UTF-8'});

        writer.onwriteend = function() {
          resolve();
        };
        writer.onerror = function() {
          reject();
        };

        writer.seek(writer.length);
        writer.write(blob);
      }.bind(this));
};

/** @override */
FileEntryRecordStorage.prototype.readAll = function() {
  return this.entry_.file()
      .then(
          this.readFileAsText_.bind(this),
          function() {
            FileEntryRecordStorage.handleError_(
                new Error('Unable to read from history file.'));
            return '';
          })
      .then(this.parse_.bind(this))
      .then(
          function(entries) {
            return entries;
          })
      .catch(FileEntryRecordStorage.handleError_);
};

/**
 * Reads all lines from the entry.
 *
 * @param {!File} file
 * @return {!Promise.<!Array.<string>>}
 * @private
 */
FileEntryRecordStorage.prototype.readFileAsText_ = function(file) {
  return new Promise(
      function(resolve, reject) {

        var reader = new FileReader();

        reader.onloadend = function() {
          if (!!reader.error) {
            FileEntryRecordStorage.handleError_(reader.error);
            reject();
          } else {
            resolve(reader.result);
          }
        };

        reader.onerror = function(error) {
          FileEntryRecordStorage.handleError_(error);
          reject(e);
        };

        reader.readAsText(file);
      }.bind(this));
};

/**
 * Parses the text.
 *
 * @param {string} text
 * @return {!Promise.<!Array.<!Object>>}
 * @private
 */
FileEntryRecordStorage.prototype.parse_ = function(text) {
  return new Promise(
      function(resolve, reject) {
        if (text.length == 0) {
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
 * @param {!FileEntry} entry
 *
 * @constructor
 */
function PromisaryFileEntry(entry) {
  /** @private {!FileEntry} */
  this.entry_ = entry;
}

/**
 * A "Promisary" wrapper around entry.getWriter.
 * @return {!Promise.<!FileWriter>}
 */
PromisaryFileEntry.prototype.createWriter = function() {
  return new Promise(this.entry_.createWriter.bind(this.entry_));
};

/**
 * A "Promisary" wrapper around entry.file.
 * @return {!Promise.<!File>}
 */
PromisaryFileEntry.prototype.file = function() {
  return new Promise(this.entry_.file.bind(this.entry_));
};

/**
 * @return {!Promise.<!Object>}
 */
PromisaryFileEntry.prototype.getMetadata = function() {
  return new Promise(this.entry_.getMetadata.bind(this.entry_));
};
