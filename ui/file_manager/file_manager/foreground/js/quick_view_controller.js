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
 * @param {!TaskController} taskController
 * @param {!cr.ui.ListSelectionModel} fileListSelectionModel
 *
 * @constructor
 */
function QuickViewController(
    quickView, metadataModel, selectionHandler, listContainer, quickViewModel,
    taskController, fileListSelectionModel) {
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
   * @type {!TaskController}
   * @private
   */
  this.taskController_ = taskController;

  /**
   * @type {!cr.ui.ListSelectionModel}
   * @private
   */
  this.fileListSelectionModel_ = fileListSelectionModel;

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
      'keydown', this.onKeyDownToOpen_.bind(this));
  quickView.addEventListener('keydown', this.onQuickViewKeyDown_.bind(this));
  quickView.onOpenInNewButtonTap = this.onOpenInNewButtonTap_.bind(this);

  var toolTip = this.quickView_.$$('files-tooltip');
  var elems = this.quickView_.$.buttons.querySelectorAll('[has-tooltip]');
  toolTip.addTargets(elems);
}

/**
 * Handles open-in-new button tap.
 *
 * @param {!Event} event A button click event.
 * @private
 */
QuickViewController.prototype.onOpenInNewButtonTap_ = function(event) {
  this.taskController_.executeDefaultTask();
  this.quickView_.close();
}

/**
 * Handles key event on listContainer if it's relevent to quick view.
 *
 * @param {!Event} event A keyboard event.
 * @private
 */
QuickViewController.prototype.onKeyDownToOpen_ = function(event) {
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
QuickViewController.prototype.onQuickViewKeyDown_ = function(event) {
  switch (event.key) {
    case ' ':
      event.preventDefault();
      this.quickView_.close();
      this.listContainer_.focus();
      break;
    case 'ArrowRight':
      var index = this.fileListSelectionModel_.selectedIndex + 1;
      if (index >= this.fileListSelectionModel_.length)
        index = 0;
      this.fileListSelectionModel_.selectedIndex = index;
      break;
    case 'ArrowLeft':
      var index = this.fileListSelectionModel_.selectedIndex - 1;
      if (index < 0)
        index = this.fileListSelectionModel_.length - 1;
      this.fileListSelectionModel_.selectedIndex = index;
      break;
  }
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
  return this.metadataModel_.get([entry], ['thumbnailUrl', 'externalFileUrl'])
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
  var type = FileType.getType(entry).type;

  this.quickView_.clear();
  this.quickView_.filePath = entry.name;
  this.quickView_.type = type;
  if (type === 'image') {
    if (item.externalFileUrl) {
      if (item.thumbnailUrl) {
        this.loadThumbnailFromDrive_(item.thumbnailUrl, function(result) {
          if (result.status !== 'success') {
            return;
          }
          this.quickView_.contentUrl = result.data;
        }.bind(this));
      }
    } else {
      this.quickView_.contentUrl = item.thumbnailUrl || entry.toURL();
    }
  } else if (type === 'video') {
    if (item.externalFileUrl) {
      if (item.thumbnailUrl) {
        this.loadThumbnailFromDrive_(item.thumbnailUrl, function(result) {
          if (result.status !== 'success') {
            return;
          }
          this.quickView_.videoPoster = result.data;
        }.bind(this));
      }
    } else {
      this.quickView_.contentUrl = entry.toURL();
      if (item.thumbnailUrl)
        this.quickView_.videoPoster = item.thumbnailUrl;
      this.quickView_.autoplay = true;
    }
  } else if (type === 'audio') {
    if (item.externalFileUrl) {
      // If the file is in Drive, we ask user to open it with external app.
    } else {
      this.quickView_.contentUrl = entry.toURL();
      this.quickView_.autoplay = true;
      this.metadataModel_.get([entry], ['contentThumbnailUrl'])
          .then(function(entry, items) {
            var item = items[0];
            if (item.contentThumbnailUrl)
              this.quickView_.audioArtwork = item.contentThumbnailUrl;
          }.bind(this, entry));
    }
  }
};

/**
 * Loads a thumbnail from Drive.
 *
 * @param {string} url Thumbnail url
 * @param {function({status: string, data:string, width:number,
 *     height:number})} callback
 * @private
 */
QuickViewController.prototype.loadThumbnailFromDrive_ = function(
    url, callback) {
  ImageLoaderClient.getInstance().load(url, callback);
};
