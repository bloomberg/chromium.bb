// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {uselessCode, externsValidation} Temporary suppress because of the
 * line exporting.
 */

// #import {importer} from '../../file_manager/common/js/importer_common.m.js';

// Namespace
// eslint-disable-next-line no-var
var importerHistoryInterfaces = {};

/**
 * A persistent data store for Cloud Import history information.
 *
 * @interface
 */
importerHistoryInterfaces.ImportHistory = class {
  /**
   * @return {!Promise<!importerHistoryInterfaces.ImportHistory>} Resolves when
   *     history has been fully loaded.
   */
  whenReady() {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<boolean>} Resolves with true if the FileEntry
   *     was previously copied to the device.
   */
  wasCopied(entry, destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<boolean>} Resolves with true if the FileEntry
   *     was previously imported to the specified destination.
   */
  wasImported(entry, destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @param {string} destinationUrl
   * @return {!Promise}
   */
  markCopied(entry, destination, destinationUrl) {}

  /**
   * List urls of all files that are marked as copied, but not marked as synced.
   * @param {!importer.Destination} destination
   * @return {!Promise<!Array<string>>}
   */
  listUnimportedUrls(destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<?>} Resolves when the operation is completed.
   */
  markImported(entry, destination) {}

  /**
   * @param {string} destinationUrl
   * @return {!Promise<?>} Resolves when the operation is completed.
   */
  markImportedByUrl(destinationUrl) {}

  /**
   * Adds an observer, which will be notified when cloud import history changes.
   *
   * @param {!importerHistoryInterfaces.ImportHistory.Observer} observer
   */
  addObserver(observer) {}

  /**
   * Remove a previously registered observer.
   *
   * @param {!importerHistoryInterfaces.ImportHistory.Observer} observer
   */
  removeObserver(observer) {}
};

/**
 * @enum {string} Import history changed event |state| values.
 */
importerHistoryInterfaces.ImportHistoryState = {
  'COPIED': 'copied',
  'IMPORTED': 'imported'
};

/**
 * Import history changed event (sent to ImportHistory.Observer's).
 *
 * @typedef {{
 *   state: !importerHistoryInterfaces.ImportHistoryState,
 *   entry: !FileEntry,
 *   destination: !importer.Destination,
 *   destinationUrl: (string|undefined)
 * }}
 */
importerHistoryInterfaces.ImportHistory.ChangedEvent;

/**
 * @typedef {function(!importerHistoryInterfaces.ImportHistory.ChangedEvent)}
 */
importerHistoryInterfaces.ImportHistory.Observer;

/**
 * Provider of lazy loaded importerHistoryInterfaces.ImportHistory. This is the
 * main access point for a fully prepared {@code
 * importerHistoryInterfaces.ImportHistory} object.
 *
 * @interface
 */
importerHistoryInterfaces.HistoryLoader = class {
  /**
   * Instantiates an {@code importerHistoryInterfaces.ImportHistory} object and
   * manages any necessary ongoing maintenance of the object with respect to its
   * external dependencies.
   *
   * @see importerHistory.SynchronizedHistoryLoader for an example.
   *
   * @return {!Promise<!importerHistoryInterfaces.ImportHistory>} Resolves when
   *     history instance is ready.
   */
  getHistory() {}

  /**
   * Adds a listener to be notified when history is fully loaded for the first
   * time. If history is already loaded...will be called immediately.
   *
   * @param {function(!importerHistoryInterfaces.ImportHistory)} listener
   */
  addHistoryLoadedListener(listener) {}
};

// eslint-disable-next-line semi,no-extra-semi
/* #export */ {importerHistoryInterfaces};
