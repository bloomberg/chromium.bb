// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class representing the results of a scan operation.
 *
 * @interface
 */
importer.MediaScanner = function() {};

/**
 * Initiates scanning.
 *
 * @param {!Array.<!Entry>} entries Must be non-empty.
 * @return {!importer.ScanResult} ScanResult object representing the scan
 *     job both while in-progress and when completed.
 */
importer.MediaScanner.prototype.scan;

/**
 * Class representing the results of a scan operation.
 *
 * @interface
 */
importer.ScanResult = function() {};

/**
 * Returns all files entries discovered so far. The list will be
 * complete only after scanning has completed and {@code isFinished}
 * returns {@code true}.
 *
 * @return {!Array.<!FileEntry>}
 */
importer.ScanResult.prototype.getFileEntries;

/**
 * Returns the aggregate size, in bytes, of all FileEntries discovered
 * during scanning.
 *
 * @return {number}
 */
importer.ScanResult.prototype.getTotalBytes;

/**
 * Returns the scan duration in milliseconds.
 *
 * @return {number}
 */
importer.ScanResult.prototype.getScanDurationMs;

/**
 * Returns a promise that fires when scanning is complete.
 *
 * @return {!Promise.<!importer.ScanResult>}
 */
importer.ScanResult.prototype.whenFinished;

/**
 * Recursively scans through a list of given files and directories, and creates
 * a list of media files.
 *
 * @constructor
 * @struct
 * @implements {importer.MediaScanner}
 *
 * @param {!importer.HistoryLoader} historyLoader
 */
importer.DefaultMediaScanner = function(historyLoader) {
  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;
};

/** @override */
importer.DefaultMediaScanner.prototype.scan = function(entries) {
  if (entries.length == 0) {
    throw new Error('Cannot scan empty list of entries.');
  }

  var scanResult = new importer.DefaultScanResult(this.historyLoader_);
  var scanPromises = entries.map(this.scanEntry_.bind(this, scanResult));

  Promise.all(scanPromises)
      .then(scanResult.resolveScan.bind(scanResult))
      .catch(scanResult.rejectScan.bind(scanResult));

  return scanResult;
};

/**
 * Resolves the entry to a list of {@code FileEntry}.
 *
 * @param {!importer.DefaultScanResult} result
 * @param {!Entry} entry
 * @return {!Promise}
 * @private
 */
importer.DefaultMediaScanner.prototype.scanEntry_ =
    function(result, entry) {
  return entry.isFile ?
      result.onFileEntryFound(/** @type {!FileEntry} */ (entry)) :
      this.scanDirectory_(result, /** @type {!DirectoryEntry} */ (entry));
};

/**
 * Finds all files beneath directory.
 *
 * @param {!importer.DefaultScanResult} result
 * @param {!DirectoryEntry} entry
 * @return {!Promise}
 * @private
 */
importer.DefaultMediaScanner.prototype.scanDirectory_ =
    function(result, entry) {
  return new Promise(
      function(resolve, reject) {
        // Collect promises for all files being added to results.
        // The directory scan promise can't resolve until all
        // file entries are completely promised.
        var promises = [];
        fileOperationUtil.findFilesRecursively(
            entry,
            /** @param  {!FileEntry} fileEntry */
            function(fileEntry) {
              promises.push(result.onFileEntryFound(fileEntry));
            })
            .then(
                /** @this {importer.DefaultScanResult} */
                function() {
                  Promise.all(promises).then(resolve).catch(reject);
                });
      });
};

/**
 * Results of a scan operation. The object is "live" in that data can and
 * will change as the scan operation discovers files.
 *
 * <p>The scan is complete, and the object will become static once the
 * {@code whenFinished} promise resolves.
 *
 * @constructor
 * @struct
 * @implements {importer.ScanResult}
 *
 * @param {!importer.HistoryLoader} historyLoader
 */
importer.DefaultScanResult = function(historyLoader) {
  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /**
   * List of file entries found while scanning.
   * @private {!Array.<!FileEntry>}
   */
  this.fileEntries_ = [];

  /** @private {number} */
  this.totalBytes_ = 0;

  /**
   * The point in time when the scan was started.
   * @type {Date}
   */
  this.scanStarted_ = new Date();

  /**
   * The point in time when the last scan activity occured.
   * @type {Date}
   */
  this.lastScanActivity_ = this.scanStarted_;

  /** @type {function()} */
  this.resolveScan;

  /** @type {function(*)} */
  this.rejectScan;

  /** @private {!Promise.<!importer.ScanResult>} */
  this.finishedPromise_ = new Promise(
      function(resolve, reject) {
        this.resolveScan = function() {
          resolve(this);
        }.bind(this);
        this.rejectScan = reject;
      }.bind(this));
};

/** @override */
importer.DefaultScanResult.prototype.getFileEntries = function() {
  return this.fileEntries_;
};

/** @override */
importer.DefaultScanResult.prototype.getTotalBytes = function() {
  return this.totalBytes_;
};

/** @override */
importer.DefaultScanResult.prototype.getScanDurationMs = function() {
  return this.lastScanActivity_.getTime() - this.scanStarted_.getTime();
};

/** @override */
importer.DefaultScanResult.prototype.whenFinished = function() {
  return this.finishedPromise_;
};

/**
 * Handles files discovered during scanning.
 *
 * @param {!FileEntry} entry
 * @return {!Promise} Resolves once file entry has been processed
 *     and is represented in results.
 */
importer.DefaultScanResult.prototype.onFileEntryFound = function(entry) {
  this.lastScanActivity_ = new Date();

  if (!FileType.isImageOrVideo(entry)) {
    return Promise.resolve();
  }

  return this.historyLoader_.getHistory()
      .then(
          /**
           * @param {!importer.ImportHistory} history
           * @return {!Promise}
           * @this {importer.DefaultScanResult}
           */
          function(history) {
            return history.wasImported(
                entry,
                importer.Destination.GOOGLE_DRIVE)
                .then(
                    /**
                     * @param {boolean} imported
                     * @return {!Promise}
                     * @this {importer.DefaultScanResult}
                     */
                    function(imported) {
                      return imported ?
                          Promise.resolve() :
                          this.addFileEntry_(entry);
                    }.bind(this));
          }.bind(this));
};

/**
 * Adds a file to results.
 *
 * @param {!FileEntry} entry
 * @return {!Promise} Resolves once file entry has been processed
 *     and is represented in results.
 * @private
 */
importer.DefaultScanResult.prototype.addFileEntry_ = function(entry) {
  return new Promise(
      function(resolve, reject) {
        // TODO(smckay): Update to use MetadataCache.
        entry.getMetadata(
          /**
           * @param {!Metadata} metadata
           * @this {importer.DefaultScanResult}
           */
          function(metadata) {
            this.lastScanActivity_ = new Date();
            if ('size' in metadata) {
              this.totalBytes_ += metadata['size'];
              this.fileEntries_.push(entry);
              // Closure compiler currently requires an arg to resolve
              // and reject. If this is 2015, you can probably remove it.
              resolve(undefined);
            } else {
              // Closure compiler currently requires an arg to resolve
              // and reject. If this is 2015, you can probably remove it.
              reject(undefined);
            }
          }.bind(this),
          reject);
      }.bind(this));
};
