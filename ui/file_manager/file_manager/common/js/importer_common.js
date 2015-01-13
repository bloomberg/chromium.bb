// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared cloud importer namespace
var importer = importer || {};

/** @enum {string} */
importer.ScanEvent = {
  FINALIZED: 'finalized'
};

/**
 * @typedef {function(
 *     !importer.ScanEvent, importer.ScanResult)}
 */
importer.ScanObserver;

/**
 * Volume types eligible for the affections of Cloud Import.
 * @private @const {!Array.<!VolumeManagerCommon.VolumeType>}
 */
importer.ELIGIBLE_VOLUME_TYPES_ = [
  VolumeManagerCommon.VolumeType.MTP,
  VolumeManagerCommon.VolumeType.REMOVABLE
];

/**
 * @enum {string}
 */
importer.Destination = {
  // locally copied, but not imported to cloud as of yet.
  DEVICE: 'device',
  GOOGLE_DRIVE: 'google-drive'
};

/**
 * Returns true if the entry is a media file (and a descendant of a DCIM dir).
 *
 * @param {Entry} entry
 * @return {boolean}
 */
importer.isMediaEntry = function(entry) {
  return !!entry &&
      entry.isFile &&
      FileType.isImageOrVideo(entry) &&
      entry.fullPath.toUpperCase().indexOf('/DCIM/') === 0;
};

/**
 * Returns true if the volume is eligible for Cloud Import.
 *
 * @param {VolumeInfo} volumeInfo
 * @return {boolean}
 */
importer.isEligibleVolume = function(volumeInfo) {
  return !!volumeInfo &&
      importer.ELIGIBLE_VOLUME_TYPES_.indexOf(volumeInfo.volumeType) !== -1;
};

/**
 * Returns true if the entry is cloud import eligible.
 *
 * @param {VolumeManagerCommon.VolumeInfoProvider} volumeInfoProvider
 * @param {Entry} entry
 * @return {boolean}
 */
importer.isEligibleEntry = function(volumeInfoProvider, entry) {
  console.assert(volumeInfoProvider !== null);
  if (importer.isMediaEntry(entry)) {
    var volumeInfo = volumeInfoProvider.getVolumeInfo(entry);
    return importer.isEligibleVolume(volumeInfo);
  }
  return false;
};

/**
 * Returns true if the entry represents a media directory for the purposes
 * of Cloud Import.
 *
 * @param {Entry} entry
 * @param  {VolumeManagerCommon.VolumeInfoProvider} volumeInfoProvider
 * @return {boolean}
 */
importer.isMediaDirectory = function(entry, volumeInfoProvider) {
  if (!entry || !entry.isDirectory || !entry.fullPath) {
    return false;
  }

  var fullPath = entry.fullPath.toUpperCase();
  if (fullPath !== '/DCIM' && fullPath !== '/DCIM/') {
    return false;
  }

  console.assert(volumeInfoProvider !== null);
  var volumeInfo = volumeInfoProvider.getVolumeInfo(entry);
  return importer.isEligibleVolume(volumeInfo);
};

/**
 * @return {!Promise.<boolean>} Resolves with true when Cloud Import feature
 *     is enabled.
 */
importer.importEnabled = function() {
  return new Promise(
      function(resolve, reject) {
        chrome.commandLinePrivate.hasSwitch(
            'enable-cloud-backup',
            /** @param {boolean} enabled */
            function(enabled) {
              resolve(enabled);
            });
      });
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

/**
 * @param {!FileEntry} fileEntry
 * @return {!Promise.<string>} Resolves with a "hashcode" consisting of
 *     just the last modified time and the file size.
 */
importer.createMetadataHashcode = function(fileEntry) {
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
                    resolve(metadata.modificationTime + '_' + metadata.size);
                  }
                }.bind(this));
      }.bind(this));
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
 *
 * @param {string} fileName
 */
importer.ChromeSyncFileEntryProvider = function(fileName) {

  /** @private {string} */
  this.fileName_ = fileName;

  /** @private {!Array.<function()>} */
  this.syncListeners_ = [];

  /** @private {Promise.<!FileEntry>} */
  this.fileEntryPromise_ = null;

  this.monitorSyncEvents_();
};

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
    return /** @type {!Promise.<!FileEntry>} */ (this.fileEntryPromise_);
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

  return /** @type {!Promise.<!FileEntry>} */ (this.fileEntryPromise_);
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
            this.fileName_,
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
              'Unexpected sync event action for sync file: ' + event.action);
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

