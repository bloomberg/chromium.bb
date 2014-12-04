// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 */

var metrics = metricsBase;

metrics.startInterval('Load.Total');
metrics.startInterval('Load.Script');

/**
 * A mapping of enum names to valid values. This object is consulted
 * any time an enum value is being reported unaccompanied by a list
 * of valid values.
 *
 * <p>Values in this object should correspond exactly with values
 * in {@code tools/metrics/histograms/histograms.xml}.
 *
 * <p>NEVER REMOVE OR REORDER ITEMS IN THIS LIST!
 *
 * @private {!Object.<string, !Array.<*>|number>}
 */
metrics.validEnumValues_ = {
  'CloudImport.UserAction': [
    'IMPORT_INITIATED'
  ]
};

/**
 * Convert a short metric name to the full format.
 *
 * @param {string} name Short metric name.
 * @return {string} Full metric name.
 * @override
 * @private
 */
metrics.convertName_ = function(name) {
  return 'FileBrowser.' + name;
};
