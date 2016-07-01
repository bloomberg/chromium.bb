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
  this.metadataBox_.clear();
  var entry = this.quickViewModel_.getSelectedEntry();
  if (!entry)
    return;
  this.metadataModel_
      .get(
          [entry], MetadataBoxController.GENERAL_METADATA_NAME.concat(
                       ['hosted', 'externalFileUrl']))
      .then(this.onGeneralMetadataLoaded_.bind(this, entry));
};

/**
 * Update metadata box with general file information.
 * Then retrieve file specific metadata if any.
 *
 * @param {!FileEntry} entry
 * @param {!Array<!MetadataItem>} items
 *
 * @private
 */
MetadataBoxController.prototype.onGeneralMetadataLoaded_ = function(
    entry, items) {
  var type = FileType.getType(entry).type;
  var item = items[0];

  this.metadataBox_.type = type;
  if (item.size) {
    this.metadataBox_.size =
        this.fileMetadataFormatter_.formatSize(item.size, item.hosted);
  }
  if (item.modificationTime) {
    this.metadataBox_.modificationTime =
        this.fileMetadataFormatter_.formatModDate(item.modificationTime);
  }

  if (item.externalFileUrl) {
    this.metadataModel_.get([entry], ['contentMimeType']).then(function(items) {
      var item = items[0];
      this.metadataBox_.mediaMimeType = item.contentMimeType;
    }.bind(this));
  } else {
    this.metadataModel_.get([entry], ['mediaMimeType']).then(function(items) {
      var item = items[0];
      this.metadataBox_.mediaMimeType = item.mediaMimeType;
    }.bind(this));
  }

  var type = FileType.getType(entry).type;
  this.metadataBox_.type = type;
  if (['image', 'video', 'audio'].includes(type)) {
    if (item.externalFileUrl) {
      this.metadataModel_.get([entry], ['imageHeight', 'imageWidth'])
          .then(function(items) {
            var item = items[0];
            this.metadataBox_.imageHeight = item.imageHeight;
            this.metadataBox_.imageWidth = item.imageWidth;
          }.bind(this));
    } else {
      this.metadataModel_
          .get(
              [entry],
              ['imageHeight', 'imageWidth', 'mediaArtist', 'mediaTitle'])
          .then(function(items) {
            var item = items[0];
            this.metadataBox_.imageHeight = item.imageHeight;
            this.metadataBox_.imageWidth = item.imageWidth;
            this.metadataBox_.mediaArtist = item.mediaArtist;
            this.metadataBox_.mediaTitle = item.mediaTitle;
          }.bind(this));
    }
  }
};
