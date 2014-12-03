// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The current selection object.
 *
 * @param {!FileManager} fileManager FileManager instance.
 * @param {!Array.<number>} indexes Selected indexes.
 * @constructor
 * @struct
 */
function FileSelection(fileManager, indexes) {
  /**
   * @type {!FileManager}
   * @private
   * @const
   */
  this.fileManager_ = fileManager;

  /**
   * @type {number}
   * @private
   */
  this.computeBytesSequence_ = 0;

  /**
   * @type {!Array.<number>}
   * @const
   */
  this.indexes = indexes;

  /**
   * @type {!Array.<!Entry>}
   * @const
   */
  this.entries = [];

  /**
   * @type {number}
   */
  this.totalCount = 0;

  /**
   * @type {number}
   */
  this.fileCount = 0;

  /**
   * @type {number}
   */
  this.directoryCount = 0;

  /**
   * @type {number}
   */
  this.bytes = 0;

  /**
   * @type {boolean}
   */
  this.showBytes = false;

  /**
   * @type {boolean}
   */
  this.allDriveFilesPresent = false;

  /**
   * @type {?string}
   */
  this.iconType = null;

  /**
   * @type {boolean}
   */
  this.bytesKnown = false;

  /**
   * @type {boolean}
   * @private
   */
  this.mustBeHidden_ = false;

  /**
   * @type {Array.<string>}
   */
  this.mimeTypes = null;

  /**
   * @type {!FileTasks}
   */
  this.tasks = new FileTasks(this.fileManager_);

  /**
   * @type {Promise}
   * @private
   */
  this.asyncInitPromise_ = null;

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
}

/**
 * Computes data required to get file tasks and requests the tasks.
 * @return {!Promise}
 */
FileSelection.prototype.completeInit = function() {
  if (!this.asyncInitPromise_) {
    if (!this.fileManager_.isOnDrive()) {
      this.asyncInitPromise_ = Promise.resolve();
      this.allDriveFilesPresent = true;
      return this.tasks.init(this.entries);
    } else {
      this.asyncInitPromise_ = new Promise(function(fulfill) {
        this.fileManager_.metadataCache.get(this.entries, 'external', fulfill);
      }.bind(this)).then(function(props) {
        var present = props.filter(function(p) {
          return p && p.availableOffline;
        });
        this.allDriveFilesPresent = present.length == props.length;
        // Collect all of the mime types and push that info into the
        // selection.
        this.mimeTypes = props.map(function(value) {
          return (value && value.contentMimeType) || '';
        });
        return this.tasks.init(this.entries, this.mimeTypes);
      }.bind(this));
    }
  }
  return this.asyncInitPromise_;
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
 * @param {!FileManager} fileManager File manager instance.
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

  util.addEventListenerToBackgroundComponent(
      assert(fileManager.fileOperationManager),
      'entries-changed',
      this.onFileSelectionChanged.bind(this));
  // Register evnets to update file selections.
  fileManager.directoryModel.addEventListener(
      'directory-changed', this.onFileSelectionChanged.bind(this));
}

/**
 * @enum {string}
 */
FileSelectionHandler.EventType = {
  /**
   * Dispatched every time when selection is changed.
   */
  CHANGE: 'change',

  /**
   * Dispatched 200ms later after the selecton is changed.
   * If multiple changes are happened during the term, only one CHANGE_THROTTLED
   * event is dispatched.
   */
  CHANGE_THROTTLED: 'changethrottled'
};

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

  this.selectionUpdateTimer_ = setTimeout(function() {
    this.selectionUpdateTimer_ = null;
    if (this.selection == selection)
      this.updateFileSelectionAsync_(selection);
  }.bind(this), updateDelay);

  cr.dispatchSimpleEvent(this, FileSelectionHandler.EventType.CHANGE);
};

/**
 * Calculates async selection stats and updates secondary UI elements.
 *
 * @param {FileSelection} selection The selection object.
 * @private
 */
FileSelectionHandler.prototype.updateFileSelectionAsync_ = function(selection) {
  if (this.selection !== selection)
    return;

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

  cr.dispatchSimpleEvent(this, FileSelectionHandler.EventType.CHANGE_THROTTLED);
};

/**
 * Returns whether all the selected files are available currently or not.
 * Should be called after the selection initialized.
 * @return {boolean}
 */
FileSelectionHandler.prototype.isAvailable = function() {
  return !this.fileManager_.isOnDrive() ||
      this.fileManager_.volumeManager.getDriveConnectionState().type !==
          VolumeManagerCommon.DriveConnectionType.OFFLINE ||
      this.selection.allDriveFilesPresent;
};
