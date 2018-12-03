// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock FolderShortcutDataModel.
 * @param {!Array<MockEntry>} args Array of shortcuts.
 *
 * @extends {cr.ui.ArrayDataModel}
 * @constructor
 */
function MockFolderShortcutDataModel(...args) {
  cr.ui.ArrayDataModel.apply(this, args);
}

MockFolderShortcutDataModel.prototype = {
  __proto__: cr.ui.ArrayDataModel.prototype
};

/**
 * Mock function for FolderShortcutDataModel.compare().
 * @param {MockEntry} a First parameter to be compared.
 * @param {MockEntry} b Second parameter to be compared with.
 * @return {number} Negative if a < b, positive if a > b, or zero if a == b.
 */
MockFolderShortcutDataModel.prototype.compare = function(a, b) {
  return a.fullPath.localeCompare(b.fullPath);
};
