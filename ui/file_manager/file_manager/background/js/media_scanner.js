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
 * Returns a promise that fires when scanning is complete.
 *
 * @return {!Promise.<!importer.ScanResult>}
 */
importer.ScanResult.prototype.whenFinal;

/**
 * @return {!importer.ScanResult.Statistics}
 */
importer.ScanResult.prototype.getStatistics;

/**
 * @typedef {{
 *   scanDuration: number,
 *   newFileCount: number,
 *   duplicateFileCount: number,
 *   sizeBytes: number
 * }}
 */
importer.ScanResult.Statistics;

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

  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /**
   * A little factory for DefaultScanResults which allows us to forgo
   * the saving it's dependencies in our fields.
   * @return {!importer.DefaultScanResult}
   */
  this.createScanResult_ = function() {
    return new importer.DefaultScanResult(hashGenerator);
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

  var scan = this.createScanResult_();
  var watcher = this.watcherFactory_(
      /** @this {importer.DefaultMediaScanner} */
      function() {
        scan.invalidateScan();
        this.notify_(importer.ScanEvent.INVALIDATED, scan);
      }.bind(this));

  var scanPromises = entries.map(
      this.scanEntry_.bind(this, scan, watcher));

  Promise.all(scanPromises)
      .then(scan.resolve)
      .catch(scan.reject);

  scan.whenFinal()
      .then(
          /** @this {importer.DefaultMediaScanner} */
          function() {
            this.notify_(importer.ScanEvent.FINALIZED, scan);
          }.bind(this));

  return scan;
};

/**
 * Notifies all listeners at some point in the near future.
 *
 * @param {!importer.ScanEvent} event
 * @param {!importer.DefaultScanResult} result
 * @private
 */
importer.DefaultMediaScanner.prototype.notify_ = function(event, result) {
  this.observers_.forEach(
      /** @param {!importer.ScanObserver} observer */
      function(observer) {
        observer(event, result);
      });
};

/**
 * Resolves the entry by either:
 * a) recursing on it (when a directory)
 * b) adding it to the results (when a media type file)
 * c) ignoring it, if neither a or b
 *
 * @param {!importer.DefaultScanResult} scan
 * @param {!importer.DirectoryWatcher} watcher
 * @param {!Entry} entry
 *
 * @return {!Promise}
 * @private
 */
importer.DefaultMediaScanner.prototype.scanEntry_ =
    function(scan, watcher, entry) {

  if (entry.isDirectory) {
    return this.scanDirectory_(
      scan,
      watcher,
      /** @type {!DirectoryEntry} */ (entry));
  }

  // Since this entry is by client code (and presumably the user)
  // we add it directly (skipping over the history dupe check).
  return this.onUniqueFileFound_(scan, /** @type {!FileEntry} */ (entry));
};

/**
 * Finds all files beneath directory.
 *
 * @param {!importer.DefaultScanResult} scan
 * @param {!importer.DirectoryWatcher} watcher
 * @param {!DirectoryEntry} entry
 * @return {!Promise}
 * @private
 */
importer.DefaultMediaScanner.prototype.scanDirectory_ =
    function(scan, watcher, entry) {
  // Collect promises for all files being added to results.
  // The directory scan promise can't resolve until all
  // file entries are completely promised.
  var promises = [];

  return fileOperationUtil.findEntriesRecursively(
      entry,
      /**
       * @param  {!Entry} entry
       * @this {importer.DefaultMediaScanner}
       */
      function(entry) {
        if (watcher.triggered) {
          return;
        }

        if (entry.isDirectory) {
          // Note, there is no need for us to recurse, the utility
          // function findEntriesRecursively does that. So we
          // just watch the directory for modifications, and that's it.
          watcher.addDirectory(/** @type {!DirectoryEntry} */(entry));
          return;
        }

        promises.push(
            this.onFileEntryFound_(scan, /** @type {!FileEntry} */(entry)));

      }.bind(this))
      .then(Promise.all.bind(Promise, promises));
};

/**
 * Finds all files beneath directory.
 *
 * @param {!importer.DefaultScanResult} scan
 * @param {!FileEntry} entry
 * @return {!Promise}
 * @private
 */
importer.DefaultMediaScanner.prototype.onFileEntryFound_ =
    function(scan, entry) {
  return this.hasHistoryDuplicate_(entry)
      .then(
          /**
           * @param {boolean} duplicate
           * @return {!Promise}
           * @this {importer.DefaultMediaScanner}
           */
          function(duplicate) {
            return duplicate ?
                this.onDuplicateFileFound_(scan, entry) :
                this.onUniqueFileFound_(scan, entry);
          }.bind(this));
};

/**
 * Adds a newly discovered file to the given scan result.
 *
 * @param {!importer.DefaultScanResult} scan
 * @param {!FileEntry} entry
 * @return {!Promise}
 * @private
 */
importer.DefaultMediaScanner.prototype.onUniqueFileFound_ =
    function(scan, entry) {

  if (!FileType.isImageOrVideo(entry)) {
    return Promise.resolve();
  }

  return scan.addFileEntry(entry)
      .then(
          /**
           * @param {boolean} added
           * @this {importer.DefaultMediaScanner}
           */
          function(added) {
            if (added) {
              this.notify_(importer.ScanEvent.UPDATED, scan);
            }
          }.bind(this));
};

/**
 * Adds a duplicate file to the given scan result.  This is to track the number
 * of duplicates that are being encountered.
 *
 * @param {!importer.DefaultScanResult} scan
 * @param {!FileEntry} entry
 * @return {!Promise}
 * @private
 */
importer.DefaultMediaScanner.prototype.onDuplicateFileFound_ =
    function(scan, entry) {
  scan.addDuplicateEntry(entry);
  return Promise.resolve();
};

/**
 * @param {!FileEntry} entry
 * @return {!Promise.<boolean>} True if there is a history-entry-duplicate
 *     for the file.
 * @private
 */
importer.DefaultMediaScanner.prototype.hasHistoryDuplicate_ = function(entry) {
  return this.historyLoader_.getHistory()
      .then(
          /**
           * @param {!importer.ImportHistory} history
           * @return {!Promise}
           * @this {importer.DefaultMediaScanner}
           */
          function(history) {
            return Promise.all([
              history.wasCopied(entry, importer.Destination.GOOGLE_DRIVE),
              history.wasImported(entry, importer.Destination.GOOGLE_DRIVE)
            ]).then(
                /**
                 * @param {!Array.<boolean>} results
                 * @return {!Promise}
                 * @this {importer.DefaultMediaScanner}
                 */
                function(results) {
                  return results[0] || results[1];
                }.bind(this));
          }.bind(this));
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
 * @param {function(!FileEntry): !Promise.<string>} hashGenerator Hash-code
 *     generator used to dedupe within the scan results itself.
 */
importer.DefaultScanResult = function(hashGenerator) {

  /** @private {function(!FileEntry): !Promise.<string>} */
  this.createHashcode_ = hashGenerator;

  /**
   * List of file entries found while scanning.
   * @private {!Array.<!FileEntry>}
   */
  this.fileEntries_ = [];

  /**
   * Hashcodes of all files included captured by this result object so-far.
   * Used to dedupe newly discovered files against other files withing
   * the ScanResult.
   * @private {!Object.<string, !FileEntry>}
   */
  this.fileHashcodes_ = {};

  /** @private {number} */
  this.totalBytes_ = 0;

  /** @private {number} */
  this.duplicateFileCount_ = 0;

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

  /**
   * @private {boolean}
   */
  this.invalidated_ = false;

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
 * Adds a file to results.
 *
 * @param {!FileEntry} entry
 * @return {!Promise.<boolean>} True if the file as added, false if it was
 *     rejected as a dupe.
 */
importer.DefaultScanResult.prototype.addFileEntry = function(entry) {
  return new Promise(entry.getMetadata.bind(entry)).then(
      /**
       * @param {!Metadata} metadata
       * @this {importer.DefaultScanResult}
       */
      function(metadata) {
        console.assert(
            'size' in metadata,
            'size attribute missing from metadata.');

        return this.createHashcode_(entry)
            .then(
                /**
                 * @param {string} hashcode
                 * @this {importer.DefaultScanResult}
                 */
                function(hashcode) {
                  this.lastScanActivity_ = new Date();

                  if (hashcode in this.fileHashcodes_) {
                    this.addDuplicateEntry(entry);
                    return false;
                  }

                  entry.size = metadata.size;
                  this.totalBytes_ += metadata.size;
                  this.fileHashcodes_[hashcode] = entry;
                  this.fileEntries_.push(entry);
                  return true;
                }.bind(this));

    }.bind(this));
};

/**
 * Logs the fact that a duplicate file entry was discovered during the scan.
 * @param {!FileEntry} entry
 */
importer.DefaultScanResult.prototype.addDuplicateEntry = function(entry) {
  this.duplicateFileCount_++;
};

/** @override */
importer.DefaultScanResult.prototype.getStatistics = function() {
  return {
    scanDuration:
        this.lastScanActivity_.getTime() - this.scanStarted_.getTime(),
    newFileCount: this.fileEntries_.length,
    duplicateFileCount: this.duplicateFileCount_,
    sizeBytes: this.totalBytes_
  };
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
