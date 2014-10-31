// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller to handle naming.
 *
 * @param {!FileFilter} fileFilter
 * @param {!cr.ui.dialogs.AlertDialog} alertDialog
 * @constructor
 * @struct
 */
function NamingController(fileFilter, alertDialog) {
  /**
   * File filter.
   * @type {!FileFilter}
   * @const
   * @private
   */
  this.fileFilter_ = fileFilter;

  /**
   * Alert dialog.
   * @type {!cr.ui.dialogs.AlertDialog}
   * @const
   * @private
   */
  this.alertDialog_ = alertDialog;
}

/**
 * Verifies the user entered name for file or folder to be created or
 * renamed to. See also util.validateFileName.
 *
 * @param {DirectoryEntry} parentEntry The URL of the parent directory entry.
 * @param {string} name New file or folder name.
 * @param {function(boolean)} onDone Function to invoke when user closes the
 *    warning box or immediatelly if file name is correct. If the name was
 *    valid it is passed true, and false otherwise.
 */
NamingController.prototype.validateFileName = function(
    parentEntry, name, onDone) {
  var fileNameErrorPromise = util.validateFileName(
      parentEntry,
      name,
      this.fileFilter_.isFilterHiddenOn());
  fileNameErrorPromise.then(onDone.bind(null, true), function(message) {
    this.alertDialog_.show(message, onDone.bind(null, false));
  }.bind(this)).catch(function(error) {
    console.error(error.stack || error);
  });
};
