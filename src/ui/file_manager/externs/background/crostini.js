// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Crostini shared path state handler.
 *
 * @interface
 */
function Crostini() {}

/**
 * Initialize Volume Manager.
 * @param {!VolumeManager} volumeManager
 */
Crostini.prototype.init = function(volumeManager) {};

/**
 * Register for any shared path changes.
 */
Crostini.prototype.listen = function() {};

/**
 * Set whether the specified VM is enabled.
 * @param {string} vmName
 * @param {boolean} enabled
 */
Crostini.prototype.setEnabled = function(vmName, enabled) {};

/**
 * Returns true if the specified VM is enabled.
 * @param {string} vmName
 * @return {boolean}
 */
Crostini.prototype.isEnabled = function(vmName) {};

/**
 * Registers an entry as a shared path for the specified VM.
 * @param {string} vmName
 * @param {!Entry} entry
 */
Crostini.prototype.registerSharedPath = function(vmName, entry) {};

/**
 * Unregisters entry as a shared path from the specified VM.
 * @param {string} vmName
 * @param {!Entry} entry
 */
Crostini.prototype.unregisterSharedPath = function(vmName, entry) {};

/**
 * Returns true if entry is shared with the specified VM.
 * @param {string} vmName
 * @param {!Entry} entry
 * @return {boolean} True if path is shared either by a direct
 *   share or from one of its ancestor directories.
 */
Crostini.prototype.isPathShared = function(vmName, entry) {};

/**
 * Returns true if entry can be shared with the specified VM.
 * @param {string} vmName
 * @param {!Entry} entry
 * @param {boolean} persist If path is to be persisted.
 */
Crostini.prototype.canSharePath = function(vmName, entry, persist) {};
