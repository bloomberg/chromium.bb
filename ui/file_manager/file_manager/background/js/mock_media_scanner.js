// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * importer.MediaScanner and importer.ScanResult test double.
 *
 * @constructor
 * @struct
 * @implements {importer.MediaScanner}
 * @implements {importer.ScanResult}
 */
function TestMediaScanner() {
  /**
   * List of file entries found while scanning.
   * @type {!Array.<!FileEntry>}
   */
  this.fileEntries = [];

  /** @type {number} */
  this.totalBytes = 100;

  /** @type {number} */
  this.scanDuration = 100;

  /** @type {!Promise.<!importer.ScanResult>} */
  this.whenFinished;
}

/** @override */
TestMediaScanner.prototype.scan = function(entries) {
  var result = new TestScanResult(this);
  this.whenFinished = Promise.resolve(result);
  return result;
};

/**
 * importer.MediaScanner and importer.ScanResult test double.
 *
 * @constructor
 * @struct
 * @implements {importer.MediaScanner}
 * @implements {importer.ScanResult}
 *
 * @param {!TestMediaScanner} scanner
 */
function TestScanResult(scanner) {
  /** @private {!TestMediaScanner} */
  this.scanner_ = scanner;
}

/** @override */
TestScanResult.prototype.getFileEntries = function() {
  return this.scanner_.fileEntries;
};

/** @override */
TestScanResult.prototype.getTotalBytes = function() {
  return this.scanner_.totalBytes;
};

/** @override */
TestScanResult.prototype.getScanDurationMs = function() {
  return this.scanner_.scanDuration;
};

/** @override */
TestScanResult.prototype.whenFinished = function() {
  return this.scanner_.whenFinished;
};
