// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns a mock of metadata model.
 *
 * @constructor
 * @extends {MetadataModel}
 * @param {Object} initial_properties
 */
function MockMetadataModel(initial_properties) {
  /**
   * Dummy properties, which can be overwritten by a test.
   * @public {Object}
   * @const
   */
  this.properties = initial_properties;
}

/** @override */
MockMetadataModel.prototype.get = function() {
  return Promise.resolve([this.properties]);
};

/** @override */
MockMetadataModel.prototype.getCache = function() {
  return [this.properties];
};

/** @override */
MockMetadataModel.prototype.notifyEntriesChanged = function() {};
