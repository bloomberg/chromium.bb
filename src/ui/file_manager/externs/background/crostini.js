// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Crostini shared path state handler.
 * @constructor
 */
function Crostini() {}

/**
 * Set from feature 'crostini-files'.
 * @param {boolean} enabled
 */
Crostini.prototype.setEnabled = function(enabled) {};

/**
 * @return {boolean} Whether crostini is enabled.
 */
Crostini.prototype.isEnabled = function() {};

/**
 * Registers an entry as a shared path.
 * @param {!Entry} entry
 */
Crostini.prototype.registerSharedPath = function(entry) {};

/**
 * Unregisters entry as a shared path.
 * @param {!Entry} entry
 */
Crostini.prototype.unregisterSharedPath = function(entry) {};

/**
 * Returns true if entry is shared.
 * @param {!Entry} entry
 * @return {boolean} True if path is shared either by a direct
 *   share or from one of its ancestor directories.
 */
Crostini.prototype.isPathShared = function(entry) {};

/**
 * Returns true if entry can be shared with Crostini.
 * @param {!Entry} entry
 * @param {boolean} persist If path is to be persisted.
 */
Crostini.prototype.canSharePath = function(entry, persist) {};
