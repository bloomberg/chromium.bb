// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller of metadata box.
 *
 * @param{!MetadataModel} metadataModel
 * @param{!FilesMetadataBox} metadataBox
 * @param{!FilesQuickView} quickView
 * @param{!QuickViewModel} quickViewModel
 * @param{!FileMetadataFormatter} fileMetadataFormatter
 *
 * @constructor
 */
function MetadataBoxController(
    metadataModel, metadataBox, quickView, quickViewModel,
    fileMetadataFormatter) {
  /**
   * @type {!MetadataModel}
   * @private
   */
  this.metadataModel_ = metadataModel;

  /**
   * @type {!FilesMetadataBox}
   * @private
   */
  this.metadataBox_ = metadataBox;

  /**
   * @type {!FilesQuickView}
   * @private
   */
  this.quickView_ = quickView;

  /**
   * @type {!FileMetadataFormatter}
   * @private
   */
  this.fileMetadataFormatter_ = fileMetadataFormatter;

  // TODO(oka): Add storage to persist the value of
  // quickViewModel_.metadataBoxActive.
  /**
   * @type {!QuickViewModel}
   * @private
   */
  this.quickViewModel_ = quickViewModel;

  fileMetadataFormatter.addEventListener(
      'date-time-format-changed', this.updateView_.bind(this));

  quickView.addEventListener(
      'metadata-box-active-changed', this.updateView_.bind(this));

  quickViewModel.addEventListener(
      'selected-entry-changed', this.updateView_.bind(this));
}

/**
 * @const {!Array<string>}
 */
MetadataBoxController.GENERAL_METADATA_NAME = [
  'size',
  'modificationTime',
];

/**
 * Update the view of metadata box.
 *
 * @private
 */
MetadataBoxController.prototype.updateView_ = function() {
  if (!this.quickView_.metadataBoxActive) {
    return;
  }
  var entry = this.quickViewModel_.getSelectedEntry();
  if (!entry)
    return;
  this.metadataModel_
      .get(
          [entry],
          MetadataBoxController.GENERAL_METADATA_NAME.concat(['hosted']))
      .then(this.onGeneralMetadataLoaded_.bind(this, entry));

  // TODO(oka): Add file type specific metadata.
};

/**
 * Update metadata box with general file information.
 *
 * @param{!FileEntry} entry
 * @param{!Array<!MetadataItem>} items
 * @private
 */
MetadataBoxController.prototype.onGeneralMetadataLoaded_ = function(
    entry, items) {
  var item = items[0];
  if (item.size) {
    this.metadataBox_.size =
        this.fileMetadataFormatter_.formatSize(item.size, item.hosted);
  }
  if (item.modificationTime) {
    this.metadataBox_.modificationTime =
        this.fileMetadataFormatter_.formatModDate(item.modificationTime);
  }
};
