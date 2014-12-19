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

  /** @private {!importer.ScanResult} */
  this.scans_ = [];

  /**
   * List of file entries found while scanning.
   * @type {!Array.<!FileEntry>}
   */
  this.fileEntries = [];

  /** @type {number} */
  this.totalBytes = 100;

  /** @type {number} */
  this.scanDuration = 100;

  /** @private {!Array.<!importer.ScanObserver>} */
  this.observers = [];
}

/** @override */
TestMediaScanner.prototype.addObserver = function(observer) {
  this.observers.push(observer);
};

/** @override */
TestMediaScanner.prototype.removeObserver = function(observer) {
  var index = this.observers.indexOf(observer);
  if (index > -1) {
    this.observers.splice(index, 1);
  } else {
    console.warn('Ignoring request to remove observer that is not registered.');
  }
};

/** @override */
TestMediaScanner.prototype.scan = function(entries) {
  var result = new TestScanResult(this.fileEntries);
  result.totalBytes = this.totalBytes;
  result.scanDuration = this.scanDuration;
  this.scans_.push(result);
  return result;
};

/**
 * Finalizes all previously started scans.
 */
TestMediaScanner.prototype.finalizeScans = function() {
  this.scans_.forEach(this.finalize.bind(this));
};

/**
 * Notifies observers that a scan has finished.
 * @param {!importer.ScanResult} result
 */
TestMediaScanner.prototype.finalize = function(result) {
  result.finalize();
  this.observers.forEach(
      function(observer) {
        observer(importer.ScanEvent.FINALIZED, result);
      });
};

/**
 * @param {number} expected
 */
TestMediaScanner.prototype.assertScanCount = function(expected) {
  assertEquals(expected, this.scans_.length);
};

/**
 * importer.MediaScanner and importer.ScanResult test double.
 *
 * @constructor
 * @struct
 * @implements {importer.ScanResult}
 *
 * @param {!Array.<!FileEntry>} fileEntries
 */
function TestScanResult(fileEntries) {
  /**
   * List of file entries found while scanning.
   * @type {!Array.<!FileEntry>}
   */
  this.fileEntries = fileEntries.slice();

  /** @type {number} */
  this.totalBytes = 100;

  /** @type {number} */
  this.scanDuration = 100;

  /** @type {function} */
  this.resolveResult_;

  /** @type {function} */
  this.rejectResult_;

  /** @type {boolean} */
  this.settled_ = false;

  /** @type {!Promise.<!importer.ScanResult>} */
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

/** @override */
TestScanResult.prototype.getFileEntries = function() {
  return this.fileEntries;
};

/** @override */
TestScanResult.prototype.getTotalBytes = function() {
  return this.totalBytes;
};

/** @override */
TestScanResult.prototype.getScanDurationMs = function() {
  return this.scanDuration;
};

/** @override */
TestScanResult.prototype.finalize = function() {
  return this.resolveResult_(this);
};

/** @override */
TestScanResult.prototype.whenFinal = function() {
  return this.whenFinal_;
};

/** @override */
TestScanResult.prototype.isFinal = function() {
  return this.settled_;
};
