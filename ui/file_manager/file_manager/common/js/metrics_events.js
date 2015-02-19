// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// namespace
var metrics = metrics || metricsBase;

/** @enum {string} */
metrics.Categories = {
  ACQUISITION: 'Acquisition'
};

/**
 * The values of these enums come from the analytics console.
 * @private @enum {number}
 */
metrics.Dimension_ = {
  USER_TYPE: 1,
  SESSION_TYPE: 2
};

/**
 * @enum {!analytics.EventBuilder.Dimension}
 */
metrics.Dimensions = {
  USER_TYPE_NON_IMPORT: {
    index: metrics.Dimension_.USER_TYPE,
    value: 'Non-import'
  },
  USER_TYPE_IMPORT: {
    index: metrics.Dimension_.USER_TYPE,
    value: 'Import'
  },
  SESSION_TYPE_NON_IMPORT: {
    index: metrics.Dimension_.SESSION_TYPE,
    value: 'Non-import'
  },
  SESSION_TYPE_IMPORT: {
    index: metrics.Dimension_.SESSION_TYPE,
    value: 'Import'
  }
};

// namespace
metrics.event = metrics.event || {};

/**
 * Base event builders for files app.
 * @private @enum {!analytics.EventBuilder}
 */
metrics.event.Builders_ = {
  IMPORT: analytics.EventBuilder.builder()
      .category(metrics.Categories.ACQUISITION)
};

/**
 * @enum {!analytics.EventBuilder}
 */
metrics.ImportEvents = {
  STARTED: metrics.event.Builders_.IMPORT
      .action('Import Started')
      .dimension(metrics.Dimensions.SESSION_TYPE_IMPORT)
      .dimension(metrics.Dimensions.USER_TYPE_IMPORT),

  ENDED: metrics.event.Builders_.IMPORT
      .action('Import Completed'),

  CANCELLED: metrics.event.Builders_.IMPORT
      .action('Import Cancelled'),

  ERROR: metrics.event.Builders_.IMPORT
      .action('Import Error'),

  FILE_COUNT: metrics.event.Builders_.IMPORT
      .action('Files Imported'),

  BYTE_COUNT: metrics.event.Builders_.IMPORT
      .action('Total Bytes Imported'),

  DEVICE_YANKED: metrics.event.Builders_.IMPORT
      .action('Device Yanked'),

  HISTORY_DEDUPE_COUNT: metrics.event.Builders_.IMPORT
      .action('Files Deduped By History'),

  CONTENT_DEDUPE_COUNT: metrics.event.Builders_.IMPORT
      .action('Files Deduped By Content'),

  HISTORY_CHANGED: metrics.event.Builders_.IMPORT
      .action('History Changed')
};

// namespace
metrics.timing = metrics.timing || {};

/** @enum {string} */
metrics.timing.Variables = {
  COMPUTE_HASH: 'Compute Content Hash',
  SEARCH_BY_HASH: 'Search By Hash'
};
