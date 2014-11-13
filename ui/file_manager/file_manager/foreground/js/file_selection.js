// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The current selection object.
 *
 * @param {FileManager} fileManager FileManager instance.
 * @param {Array.<number>} indexes Selected indexes.
 * @constructor
 */
function FileSelection(fileManager, indexes) {
  this.fileManager_ = fileManager;
  this.computeBytesSequence_ = 0;
  this.indexes = indexes;
  this.entries = [];
  this.totalCount = 0;
  this.fileCount = 0;
  this.directoryCount = 0;
  this.bytes = 0;
  this.showBytes = false;
  this.allDriveFilesPresent = false,
  this.iconType = null;
  this.bytesKnown = false;
  this.mustBeHidden_ = false;
  this.mimeTypes = null;

  // Synchronously compute what we can.
  for (var i = 0; i < this.indexes.length; i++) {
    var entry = /** @type {!Entry} */
        (fileManager.getFileList().item(this.indexes[i]));
    if (!entry)
      continue;

    this.entries.push(entry);

    if (this.iconType == null) {
      this.iconType = FileType.getIcon(entry);
    } else if (this.iconType != 'unknown') {
      var iconType = FileType.getIcon(entry);
      if (this.iconType != iconType)
        this.iconType = 'unknown';
    }

    if (entry.isFile) {
      this.fileCount += 1;
    } else {
      this.directoryCount += 1;
    }
    this.totalCount++;
  }

  this.tasks = new FileTasks(this.fileManager_);

  Object.seal(this);
}

/**
 * Computes data required to get file tasks and requests the tasks.
 *
 * @param {function()} callback The callback.
 */
FileSelection.prototype.createTasks = function(callback) {
  if (!this.fileManager_.isOnDrive()) {
    this.tasks.init(this.entries);
    callback();
    return;
  }

  this.fileManager_.metadataCache_.get(
      this.entries, 'external', function(props) {
        var present = props.filter(function(p) {
          return p && p.availableOffline;
        });
        this.allDriveFilesPresent = present.length == props.length;

        // Collect all of the mime types and push that info into the selection.
        this.mimeTypes = props.map(function(value) {
          return (value && value.contentMimeType) || '';
        });

        this.tasks.init(this.entries, this.mimeTypes);
        callback();
      }.bind(this));
};

/**
 * Computes the total size of selected files.
 *
 * @param {function()} callback Completion callback. Not called when cancelled,
 *     or a new call has been invoked in the meantime.
 */
FileSelection.prototype.computeBytes = function(callback) {
  if (this.entries.length == 0) {
    this.bytesKnown = true;
    this.showBytes = false;
    this.bytes = 0;
    return;
  }

  var computeBytesSequence = ++this.computeBytesSequence_;
  var pendingMetadataCount = 0;

  var maybeDone = function() {
    if (pendingMetadataCount == 0) {
      this.bytesKnown = true;
      callback();
    }
  }.bind(this);

  var onProps = function(properties) {
    // Ignore if the call got cancelled, or there is another new one fired.
    if (computeBytesSequence != this.computeBytesSequence_)
      return;

    // It may happen that the metadata is not available because a file has been
    // deleted in the meantime.
    if (properties)
      this.bytes += properties.size;
    pendingMetadataCount--;
    maybeDone();
  }.bind(this);

  for (var index = 0; index < this.entries.length; index++) {
    var entry = this.entries[index];
    if (entry.isFile) {
      this.showBytes |= !FileType.isHosted(entry);
      pendingMetadataCount++;
      this.fileManager_.metadataCache_.getOne(entry, 'filesystem', onProps);
    } else if (entry.isDirectory) {
      // Don't compute the directory size as it's expensive.
      // crbug.com/179073.
      this.showBytes = false;
      break;
    }
  }
  maybeDone();
};

/**
 * Cancels any async computation by increasing the sequence number. Results
 * of any previous call to computeBytes() will be discarded.
 *
 * @private
 */
FileSelection.prototype.cancelComputing_ = function() {
  this.computeBytesSequence_++;
};

/**
 * This object encapsulates everything related to current selection.
 *
 * @param {FileManager} fileManager File manager instance.
 * @extends {cr.EventTarget}
 * @constructor
 * @struct
 * @suppress {checkStructDictInheritance}
 */
function FileSelectionHandler(fileManager) {
  cr.EventTarget.call(this);

  this.fileManager_ = fileManager;
  // TODO(dgozman): create a shared object with most of UI elements.
  this.previewPanel_ = fileManager.ui.previewPanel;
  this.taskMenuButton_ = fileManager.ui.taskMenuButton;
  this.selection = new FileSelection(this.fileManager_, []);

  /**
   * @private
   * @type {number}
   */
  this.selectionUpdateTimer_ = 0;

  /**
   * @private
   * @type {!Date}
   */
  this.lastFileSelectionTime_ = new Date();
}

/**
 * Create the temporary disabled action item.
 * @return {Object} Created disabled item.
 * @private
 */
FileSelectionHandler.createTemporaryDisabledActionItem_ = function() {
  if (!FileSelectionHandler.cachedDisabledActionItem_) {
    FileSelectionHandler.cachedDisabledActionItem_ = {
      title: str('ACTION_OPEN'),
      disabled: true,
      taskId: null
    };
  }

  return FileSelectionHandler.cachedDisabledActionItem_;
};

/**
 * Cached the temporary disabled action item. Used inside
 * FileSelectionHandler.createTemporaryDisabledActionItem_().
 * @type {Object}
 * @private
 */
FileSelectionHandler.cachedDisabledActionItem_ = null;

/**
 * FileSelectionHandler extends cr.EventTarget.
 */
FileSelectionHandler.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Maximum amount of thumbnails in the preview pane.
 *
 * @const
 * @type {number}
 */
FileSelectionHandler.MAX_PREVIEW_THUMBNAIL_COUNT = 4;

/**
 * Maximum width or height of an image what pops up when the mouse hovers
 * thumbnail in the bottom panel (in pixels).
 *
 * @const
 * @type {number}
 */
FileSelectionHandler.IMAGE_HOVER_PREVIEW_SIZE = 200;

/**
 * Update the UI when the selection model changes.
 */
FileSelectionHandler.prototype.onFileSelectionChanged = function() {
  var indexes =
      this.fileManager_.getCurrentList().selectionModel.selectedIndexes;
  if (this.selection)
    this.selection.cancelComputing_();
  var selection = new FileSelection(this.fileManager_, indexes);
  this.selection = selection;

  if (this.selectionUpdateTimer_) {
    clearTimeout(this.selectionUpdateTimer_);
    this.selectionUpdateTimer_ = null;
  }

  // The rest of the selection properties are computed via (sometimes lengthy)
  // asynchronous calls. We initiate these calls after a timeout. If the
  // selection is changing quickly we only do this once when it slows down.

  var updateDelay = 200;
  var now = Date.now();
  if (now > (this.lastFileSelectionTime_ || 0) + updateDelay) {
    // The previous selection change happened a while ago. Update the UI soon.
    updateDelay = 0;
  }
  this.lastFileSelectionTime_ = now;

  if (this.fileManager_.dialogType === DialogType.FULL_PAGE &&
      selection.directoryCount === 0 && selection.fileCount > 0) {
    // Show disabled items for position calculation of the menu. They will be
    // overridden in this.updateFileSelectionAsync().
    this.fileManager_.updateContextMenuActionItems(
        [FileSelectionHandler.createTemporaryDisabledActionItem_()]);
  } else {
    // Update context menu.
    this.fileManager_.updateContextMenuActionItems();
  }

  this.selectionUpdateTimer_ = setTimeout(function() {
    this.selectionUpdateTimer_ = null;
    if (this.selection == selection)
      this.updateFileSelectionAsync(selection);
  }.bind(this), updateDelay);

  cr.dispatchSimpleEvent(this, 'change');
};

/**
 * Calculates async selection stats and updates secondary UI elements.
 *
 * @param {FileSelection} selection The selection object.
 */
FileSelectionHandler.prototype.updateFileSelectionAsync = function(selection) {
  if (this.selection != selection) return;

  // Update the file tasks.
  if (this.fileManager_.dialogType === DialogType.FULL_PAGE &&
      selection.directoryCount === 0 && selection.fileCount > 0) {
    selection.createTasks(function() {
      if (this.selection != selection)
        return;
      selection.tasks.display(this.taskMenuButton_);
      selection.tasks.updateMenuItem();
    }.bind(this));
  } else {
    this.taskMenuButton_.hidden = true;
  }

  // Update preview panels.
  var wasVisible = this.previewPanel_.visible;
  this.previewPanel_.setSelection(selection);

  // Scroll to item
  if (!wasVisible && this.selection.totalCount == 1) {
    var list = this.fileManager_.getCurrentList();
    list.scrollIndexIntoView(list.selectionModel.selectedIndex);
  }

  // Sync the commands availability.
  if (this.fileManager_.commandHandler)
    this.fileManager_.commandHandler.updateAvailability();

  // Inform tests it's OK to click buttons now.
  if (selection.totalCount > 0)
    util.testSendMessage('selection-change-complete');
};
