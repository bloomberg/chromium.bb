// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Changes to analytics reporting structures can have disruptive effects on the
// analytics history of Files.app (e.g. making it hard or impossible to detect
// trending).
//
// In general, treat changes to analytics like histogram changes, i.e. make
// additive changes, don't remove or rename existing Dimensions, Events, Labels,
// etc.
//
// Changes to this file will need to be reviewed by someone familiar with the
// analytics system.

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

  CANCELLED: metrics.event.Builders_.IMPORT
      .action('Import Cancelled'),

  ERRORS: metrics.event.Builders_.IMPORT
      .action('Import Error Count'),

  FILES_IMPORTED: metrics.event.Builders_.IMPORT
      .action('Files Imported'),

  MEGABYTES_IMPORTED: metrics.event.Builders_.IMPORT
      .action('Megabytes Imported'),

  DEVICE_YANKED: metrics.event.Builders_.IMPORT
      .action('Device Yanked'),

  FILES_DEDUPLICATED: metrics.event.Builders_.IMPORT
      .action('Files Deduplicated')
};

// namespace
metrics.timing = metrics.timing || {};

/** @enum {string} */
metrics.timing.Variables = {
  COMPUTE_HASH: 'Compute Content Hash',
  SEARCH_BY_HASH: 'Search By Hash'
};
