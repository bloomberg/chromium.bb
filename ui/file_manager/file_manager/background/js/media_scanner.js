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
 * Adds an observer, which will be notified on scan events.
 *
 * @param {!importer.ScanObserver} observer
 */
importer.MediaScanner.prototype.addObserver;

/**
 * Remove a previously registered observer.
 *
 * @param {!importer.ScanObserver} observer
 */
importer.MediaScanner.prototype.removeObserver;

/**
 * Class representing the results of a scan operation.
 *
 * @interface
 */
importer.ScanResult = function() {};

/**
 * @return {boolean} true if scanning is complete.
 */
importer.ScanResult.prototype.isFinal;

/**
 * @return {boolean} true if scanning is invalidated.
 */
importer.ScanResult.prototype.isInvalidated;

/**
 * Returns all files entries discovered so far. The list will be
 * complete only after scanning has completed and {@code isFinal}
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
importer.ScanResult.prototype.whenFinal;

/**
 * Recursively scans through a list of given files and directories, and creates
 * a list of media files.
 *
 * @constructor
 * @struct
 * @implements {importer.MediaScanner}
 *
 * @param {function(!FileEntry): !Promise.<string>} hashGenerator
 * @param {!importer.HistoryLoader} historyLoader
 * @param {!importer.DirectoryWatcherFactory} watcherFactory
 */
importer.DefaultMediaScanner = function(
    hashGenerator, historyLoader, watcherFactory) {
  /**
   * A little factory for DefaultScanResults which allows us to forgo
   * the saving it's dependencies in our fields.
   * @return {!importer.DefaultScanResult}
   */
  this.createScanResult_ = function() {
    return new importer.DefaultScanResult(hashGenerator, historyLoader);
  };

  /** @private {!Array.<!importer.ScanObserver>} */
  this.observers_ = [];

  /**
   * @private {!importer.DirectoryWatcherFactory}
   * @const
   */
  this.watcherFactory_ = watcherFactory;
};

/** @override */
importer.DefaultMediaScanner.prototype.addObserver = function(observer) {
  this.observers_.push(observer);
};

/** @override */
importer.DefaultMediaScanner.prototype.removeObserver = function(observer) {
  var index = this.observers_.indexOf(observer);
  if (index > -1) {
    this.observers_.splice(index, 1);
  } else {
    console.warn('Ignoring request to remove observer that is not registered.');
  }
};

/** @override */
importer.DefaultMediaScanner.prototype.scan = function(entries) {
  if (entries.length == 0) {
    throw new Error('Cannot scan empty list of entries.');
  }

  var scanResult = this.createScanResult_();
  var watcher = this.watcherFactory_(
      /** @this {importer.DefaultMediaScanner} */
      function() {
        scanResult.invalidateScan();
        this.observers_.forEach(
            /** @param {!importer.ScanObserver} observer */
            function(observer) {
              observer(importer.ScanEvent.INVALIDATED, scanResult);
            });
      }.bind(this));
  var scanPromises = entries.map(
      this.scanEntry_.bind(this, scanResult, watcher));

  Promise.all(scanPromises)
      .then(scanResult.resolve)
      .catch(scanResult.reject);

  scanResult.whenFinal()
      .then(
          function() {
            this.onScanFinished_(scanResult);
          }.bind(this));

  return scanResult;
};

/**
 * Called when a scan is finished.
 *
 * @param {!importer.DefaultScanResult} result
 * @private
 */
importer.DefaultMediaScanner.prototype.onScanFinished_ = function(result) {
  this.observers_.forEach(
      /** @param {!importer.ScanObserver} observer */
      function(observer) {
        observer(importer.ScanEvent.FINALIZED, result);
      });
};

/**
 * Resolves the entry to a list of {@code FileEntry}.
 *
 * @param {!importer.DefaultScanResult} result
 * @param {!importer.DirectoryWatcher} watcher
 * @param {!Entry} entry
 * @return {!Promise}
 * @private
 */
importer.DefaultMediaScanner.prototype.scanEntry_ =
    function(result, watcher, entry) {
  return entry.isFile ?
      result.onFileEntryFound(/** @type {!FileEntry} */ (entry)) :
      this.scanDirectory_(
          result, watcher, /** @type {!DirectoryEntry} */ (entry));
};

/**
 * Finds all files beneath directory.
 *
 * @param {!importer.DefaultScanResult} result
 * @param {!importer.DirectoryWatcher} watcher
 * @param {!DirectoryEntry} entry
 * @return {!Promise}
 * @private
 */
importer.DefaultMediaScanner.prototype.scanDirectory_ =
    function(result, watcher, entry) {
  // Collect promises for all files being added to results.
  // The directory scan promise can't resolve until all
  // file entries are completely promised.
  var promises = [];

  return fileOperationUtil.findEntriesRecursively(
      entry,
      /** @param  {!Entry} entry */
      function(entry) {
        if (watcher.triggered) {
          return;
        }
        if (entry.isDirectory) {
          watcher.addDirectory(/** @type {!DirectoryEntry} */(entry));
        } else {
          promises.push(
              result.onFileEntryFound(/** @type {!FileEntry} */(entry)));
        }
      })
      .then(Promise.all.bind(Promise, promises));
};

/**
 * Results of a scan operation. The object is "live" in that data can and
 * will change as the scan operation discovers files.
 *
 * <p>The scan is complete, and the object will become static once the
 * {@code whenFinal} promise resolves.
 *
 * @constructor
 * @struct
 * @implements {importer.ScanResult}
 *
 * @param {function(!FileEntry): !Promise.<string>} hashGenerator
 * @param {!importer.HistoryLoader} historyLoader
 */
importer.DefaultScanResult = function(hashGenerator, historyLoader) {

  /** @private {function(!FileEntry): !Promise.<string>} */
  this.createHashcode_ = hashGenerator;

  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /**
   * List of file entries found while scanning.
   * @private {!Array.<!FileEntry>}
   */
  this.fileEntries_ = [];

  /**
   * @private {boolean}
   */
  this.invalidated_ = false;

  /**
   * Hashcodes of all files included captured by this result object so-far.
   * Used to dedupe newly discovered files against other files withing
   * the ScanResult.
   * @private {!Object.<string, !FileEntry>}
   */
  this.fileHashcodes_ = {};

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

  /** @private {!importer.Resolver.<!importer.ScanResult>} */
  this.resolver_ = new importer.Resolver();
};

/** @struct */
importer.DefaultScanResult.prototype = {

  /** @return {function()} */
  get resolve() { return this.resolver_.resolve.bind(null, this); },

  /** @return {function(*=)} */
  get reject() { return this.resolver_.reject; }
};

/** @override */
importer.DefaultScanResult.prototype.isFinal = function() {
  return this.resolver_.settled;
};

importer.DefaultScanResult.prototype.isInvalidated = function() {
  return this.invalidated_;
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
importer.DefaultScanResult.prototype.whenFinal = function() {
  return this.resolver_.promise;
};

/**
 * Invalidates this scan.
 */
importer.DefaultScanResult.prototype.invalidateScan = function() {
  this.invalidated_ = true;
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
            return Promise.all([
              history.wasCopied(entry, importer.Destination.GOOGLE_DRIVE),
              history.wasImported(entry, importer.Destination.GOOGLE_DRIVE)
            ]).then(
                /**
                 * @param {!Array.<boolean>} results
                 * @return {!Promise}
                 * @this {importer.DefaultScanResult}
                 */
                function(results) {
                  return results[0] || results[1] ?
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
        this.createHashcode_(entry).then(
            /**
             * @param {string} hashcode
             * @this {importer.DefaultScanResult}
             */
            function(hashcode) {
              // Ignore the entry if it is a duplicate.
              if (hashcode in this.fileHashcodes_) {
                resolve();
                return;
              }

              entry.getMetadata(
                  /**
                   * @param {!Metadata} metadata
                   * @this {importer.DefaultScanResult}
                   */
                  function(metadata) {
                    console.assert(
                        'size' in metadata,
                        'size attribute missing from metadata.');
                    this.lastScanActivity_ = new Date();

                    // Double check that a dupe entry wasn't added while we were
                    // busy looking up metadata.
                    if (hashcode in this.fileHashcodes_) {
                      resolve();
                      return;
                    }
                    entry.size = metadata.size;
                    this.totalBytes_ += metadata['size'];
                    this.fileHashcodes_[hashcode] = entry;
                    this.fileEntries_.push(entry);
                    resolve();
                }.bind(this));
            }.bind(this));
      }.bind(this));
};

/**
 * Watcher for directories.
 * @interface
 */
importer.DirectoryWatcher = function() {};

/**
 * Registers new directory to be watched.
 * @param {!DirectoryEntry} entry
 */
importer.DirectoryWatcher.prototype.addDirectory = function(entry) {};

/**
 * @typedef {function()}
 */
importer.DirectoryWatcherFactoryCallback;

/**
 * @typedef {function(importer.DirectoryWatcherFactoryCallback):
 *     !importer.DirectoryWatcher}
 */
importer.DirectoryWatcherFactory;

/**
 * Watcher for directories.
 * @param {function()} callback Callback to be invoked when one of watched
 *     directories is changed.
 * @implements {importer.DirectoryWatcher}
 * @constructor
 */
importer.DefaultDirectoryWatcher = function(callback) {
  this.callback_ = callback;
  this.watchedDirectories_ = {};
  this.triggered = false;
  this.listener_ = null;
};

/**
 * Creates new directory watcher.
 * @param {function()} callback Callback to be invoked when one of watched
 *     directories is changed.
 * @return {!importer.DirectoryWatcher}
 */
importer.DefaultDirectoryWatcher.create = function(callback) {
  return new importer.DefaultDirectoryWatcher(callback);
};

/**
 * Registers new directory to be watched.
 * @param {!DirectoryEntry} entry
 */
importer.DefaultDirectoryWatcher.prototype.addDirectory = function(entry) {
  if (!this.listener_) {
    this.listener_ = this.onWatchedDirectoryModified_.bind(this);
    chrome.fileManagerPrivate.onDirectoryChanged.addListener(
        assert(this.listener_));
  }
  this.watchedDirectories_[entry.toURL()] = true;
  chrome.fileManagerPrivate.addFileWatch(entry.toURL(), function() {});
};

/**
 * @param {FileWatchEvent} event
 * @private
 */
importer.DefaultDirectoryWatcher.prototype.onWatchedDirectoryModified_ =
    function(event) {
  if (!this.watchedDirectories_[event.entry.toURL()])
    return;
  this.triggered = true;
  for (var url in this.watchedDirectories_) {
    chrome.fileManagerPrivate.removeFileWatch(url, function() {});
  }
  chrome.fileManagerPrivate.onDirectoryChanged.removeListener(
      assert(this.listener_));
  this.callback_();
};
