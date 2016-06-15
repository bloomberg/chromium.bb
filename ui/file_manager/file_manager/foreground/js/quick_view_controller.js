// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller for QuickView.
 *
 * @param {!FilesQuickView} quickView
 * @param {!MetadataModel} metadataModel File system metadata.
 * @param {!FileSelectionHandler} selectionHandler
 * @param {!ListContainer} listContainer
 * @param {!QuickViewModel} quickViewModel
 *
 * @constructor
 */
function QuickViewController(
    quickView, metadataModel, selectionHandler, listContainer, quickViewModel) {
  /**
   * @type {!FilesQuickView}
   * @private
   */
  this.quickView_ = quickView;

  /**
   * @type{!QuickViewModel}
   * @private
   */
  this.quickViewModel_ = quickViewModel;

  /**
   * @type {!MetadataModel}
   * @private
   */
  this.metadataModel_ = metadataModel;

  /**
   * @type {!ListContainer}
   * @private
   */
  this.listContainer_ = listContainer;

  /**
   * Current selection of selectionHandler.
   *
   * @type {!Array<!FileEntry>}
   * @private
   */
  this.entries_ = [];

  selectionHandler.addEventListener(
      FileSelectionHandler.EventType.CHANGE,
      this.onFileSelectionChanged_.bind(this));
  listContainer.element.addEventListener(
      'keypress', this.onKeyPressToOpen_.bind(this));
  quickView.addEventListener('keypress', this.onKeyPressToClose_.bind(this));
}

/**
 * Handles key event on listContainer if it's relevent to quick view.
 *
 * @param {!Event} event A keyboard event.
 * @private
 */
QuickViewController.prototype.onKeyPressToOpen_ = function(event) {
  if (this.entries_.length == 0)
    return;
  if (event.key === ' ') {
    event.preventDefault();
    this.display_();
  }
};

/**
 * Handles key event on quick view.
 *
 * @param {!Event} event A keyboard event.
 * @private
 */
QuickViewController.prototype.onKeyPressToClose_ = function(event) {
  if (event.key === ' ') {
    event.preventDefault();
    this.quickView_.close();
    this.listContainer_.focus();
  }
  // TODO(oka): Open previous/next file with Left/Right.
};

/**
 * Display quick view.
 *
 * @private
 */
QuickViewController.prototype.display_ = function() {
  this.updateQuickView_().then(function() {
    if (!this.quickView_.isOpened()) {
      this.quickView_.open();
    }
  }.bind(this));
};

/**
 * Update quick view on file selection change.
 *
 * @param {!Event} event an Event whose target is FileSelectionHandler.
 * @private
 */
QuickViewController.prototype.onFileSelectionChanged_ = function(event) {
  this.entries_ = event.target.selection.entries;
  if (this.quickView_.isOpened()) {
    assert(this.entries_.length > 0);
    var entry = this.entries_[0];
    this.quickViewModel_.setSelectedEntry(entry);
    this.display_();
  }
};

/**
 * Update quick view using current entries.
 *
 * @return {!Promise} Promise fulfilled after quick view is updated.
 * @private
 */
QuickViewController.prototype.updateQuickView_ = function() {
  assert(this.entries_.length > 0);
  // TODO(oka): Support multi-selection.
  this.quickViewModel_.setSelectedEntry(this.entries_[0]);

  var entry =
      (/** @type {!FileEntry} */ (this.quickViewModel_.getSelectedEntry()));
  assert(entry);
  return this.metadataModel_.get([entry], ['contentThumbnailUrl'])
      .then(this.onMetadataLoaded_.bind(this, entry));
};

/**
 * Update quick view using file entry and loaded metadata.
 *
 * @param {!FileEntry} entry
 * @param {Array<MetadataItem>} items
 * @private
 */
QuickViewController.prototype.onMetadataLoaded_ = function(entry, items) {
  var item = items[0];
  var type = FileType.getType(entry);
  var thumbnailUrl = item.thumbnailUrl || item.croppedThumbnailUrl;
  if (type.type === 'image') {
    if (item.externalFileUrl) {
      // TODO(oka): Support Drive.
    } else {
      var url = thumbnailUrl || entry.toURL();
      this.quickView_.setImageURL(url);
    }
  } else if (type.type === 'video') {
    // TODO(oka): Set thumbnail.
    if (item.externalFileUrl) {
      // TODO(oka): Support Drive.
    } else {
      var url = entry.toURL();
      this.quickView_.setVideoURL(url);
    }
    this.quickView_.setVideoURL(entry.toURL());
  } else if (type.type === 'audio') {
    this.quickView_.setAudioURL(entry.toURL());
    // TODO(oka): Set thumbnail.
    if (item.externalFileUrl) {
      // TODO(oka): Support Drive.
    } else {
      this.quickView_.setAudioURL(url);
    }
    this.quickView_.setAudioURL(entry.toURL());
  }
};
