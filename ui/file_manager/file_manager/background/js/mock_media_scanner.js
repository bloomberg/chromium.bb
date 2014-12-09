// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A fake media scanner for testing.
 * @constructor
 * @struct
 */
function MockMediaScanner() {
  /** @private {!Array<!FileEntry>} */
  this.results_ = [];
}

/**
 * Returns scan "results".
 * @return {!Promise<!Array<!FileEntry>>}
 */
MockMediaScanner.prototype.scan = function(entries) {
  return Promise.resolve(this.results_);
};

/**
 * Sets the entries that the scanner will return.
 * @param {!Array<!FileEntry>} entries
 */
MockMediaScanner.prototype.setScanResults = function(entries) {
  this.results_ = entries;
};
