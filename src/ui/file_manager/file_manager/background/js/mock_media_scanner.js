// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * importer.MediaScanner and importer.ScanResult test double.
 *
 * @constructor
 * @struct
 * @implements {importer.MediaScanner}
 */
function TestMediaScanner() {
  /** @private {!Array<!importer.ScanResult>} */
  this.scans_ = [];

  /**
   * List of file entries found while scanning.
   * @type {!Array<!FileEntry>}
   */
  this.fileEntries = [];

  /**
   * List of duplicate file entries found while scanning.
   * @type {!Array<!FileEntry>}
   */
  this.duplicateFileEntries = [];

  /**
   * List of scan observers.
   * @private {!Array<!importer.ScanObserver>}
   */
  this.observers = [];

  /** @private {number} */
  this.totalBytes = 100;

  /** @private {number} */
  this.scanDuration = 100;
}

/** @override */
TestMediaScanner.prototype.scanDirectory = function(directory, mode) {
  var scan = new TestScanResult(this.fileEntries);
  scan.totalBytes = this.totalBytes;
  scan.scanDuration = this.scanDuration;
  this.scans_.push(scan);
  return scan;
};

/** @override */
TestMediaScanner.prototype.scanFiles = function(entries, mode) {
  var scan = new TestScanResult(this.fileEntries);
  scan.totalBytes = this.totalBytes;
  scan.scanDuration = this.scanDuration;
  this.scans_.push(scan);
  return scan;
};

/** @override */
TestMediaScanner.prototype.addObserver = function(observer) {
  this.observers.push(observer);
};

/** @override */
TestMediaScanner.prototype.removeObserver = function(observer) {
  var index = this.observers.indexOf(observer);
  if (index !== -1) {
    this.observers.splice(index, 1);
  } else {
    console.warn('Ignoring request to remove unregistered observer');
  }
};

/**
 * Finalizes all previously started scans.
 */
TestMediaScanner.prototype.finalizeScans = function() {
  this.scans_.forEach(this.finalize.bind(this));
};

/**
 * Notifies observers that the most recently started scan has been updated.
 */
TestMediaScanner.prototype.update = function() {
  assertTrue(this.scans_.length > 0);
  var scan = this.scans_[this.scans_.length - 1];
  this.observers.forEach(
      function(observer) {
        observer(importer.ScanEvent.UPDATED, scan);
      });
};

/**
 * Notifies observers that a scan has finished.
 * @param {!importer.ScanResult} scan
 */
TestMediaScanner.prototype.finalize = function(scan) {
  // Note the |scan| has {!TestScanResult} type in test, and needs a
  // finalize() call before being notified to scan observers.
  /** @type {!TestScanResult} */ (scan).finalize();

  this.observers.forEach(function(observer) {
    observer(importer.ScanEvent.FINALIZED, scan);
  });
};

/**
 * @param {number} expected
 */
TestMediaScanner.prototype.assertScanCount = function(expected) {
  assertEquals(expected, this.scans_.length);
};

/**
 * Asserts that the last scan was canceled. Fails if no
 *     scan exists.
 */
TestMediaScanner.prototype.assertLastScanCanceled = function() {
  assertTrue(this.scans_.length > 0);
  assertTrue(this.scans_[this.scans_.length - 1].canceled());
};

/**
 * importer.MediaScanner and importer.ScanResult test double.
 *
 * @constructor
 * @struct
 * @implements {importer.ScanResult}
 *
 * @param {!Array<!FileEntry>} fileEntries
 */
function TestScanResult(fileEntries) {
  /** @private {number} */
  this.scanId_ = ++TestScanResult.lastId_;

  /**
   * List of file entries found while scanning.
   * @type {!Array<!FileEntry>}
   */
  this.fileEntries = fileEntries.slice();

  /**
   * List of duplicate file entries found while scanning.
   * @type {!Array<!FileEntry>}
   */
  this.duplicateFileEntries = [];

  /** @type {number} */
  this.totalBytes = 100;

  /** @type {number} */
  this.scanDuration = 100;

  /** @type {function(*)} */
  this.resolveResult_;

  /** @type {function()} */
  this.rejectResult_;

  /** @type {boolean} */
  this.settled_ = false;

  /** @private {boolean} */
  this.canceled_ = false;

  /** @type {!Promise<!importer.ScanResult>} */
  this.whenFinal_ = new Promise(
      function(resolve, reject) {
        this.resolveResult_ = function(result) {
          this.settled_ = true;
          resolve(result);
        }.bind(this);
        this.rejectResult_ = function() {
          this.settled_ = true;
          reject();
        }.bind(this);
      }.bind(this));
}

/** @private {number} */
TestScanResult.lastId_ = 0;

/** @struct */
TestScanResult.prototype = {
  /** @return {string} */
  get name() {
    return 'TestScanResult(' + this.scanId_ + ')';
  }
};

/** @override */
TestScanResult.prototype.isFinal = function() {
  return this.settled_;
};

/** @override */
TestScanResult.prototype.cancel = function() {
  this.canceled_ = true;
};

/** @override */
TestScanResult.prototype.canceled = function() {
  return this.canceled_;
};

/** @override */
TestScanResult.prototype.setCandidateCount = function() {
  console.warn('setCandidateCount: not implemented');
};

/** @override */
TestScanResult.prototype.onCandidatesProcessed = function() {
  console.warn('onCandidatesProcessed: not implemented');
};

/** @override */
TestScanResult.prototype.getFileEntries = function() {
  return this.fileEntries;
};

/** @override */
TestScanResult.prototype.getDuplicateFileEntries = function() {
  return this.duplicateFileEntries;
};

/** @override */
TestScanResult.prototype.whenFinal = function() {
  return this.whenFinal_;
};

/** @override */
TestScanResult.prototype.getStatistics = function() {
  var duplicates = {};
  duplicates[importer.Disposition.CONTENT_DUPLICATE] = 0;
  duplicates[importer.Disposition.HISTORY_DUPLICATE] = 0;
  duplicates[importer.Disposition.SCAN_DUPLICATE] = 0;
  return /** @type {importer.ScanResult.Statistics} */ ({
    scanDuration: this.scanDuration,
    newFileCount: this.fileEntries.length,
    duplicates: duplicates,
    sizeBytes: this.totalBytes
  });
};

TestScanResult.prototype.finalize = function() {
  return this.resolveResult_(this);
};

/**
 * @constructor
 * @implements {importer.DirectoryWatcher}
 */
function TestDirectoryWatcher(callback) {
  /**
   * @public {function()}
   * @const
   */
  this.callback = callback;

  /**
   * @public {boolean}
   */
  this.triggered = false;
}

/**
 * @override
 */
TestDirectoryWatcher.prototype.addDirectory = function() {
};
