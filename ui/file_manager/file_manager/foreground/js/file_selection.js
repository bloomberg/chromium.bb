// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The current selection object.
 * @param {!Array<number>} indexes
 * @param {!Array<Entry>} entries
 * @constructor
 * @struct
 */
function FileSelection(indexes, entries) {
  /**
   * @public {!Array<number>}
   * @const
   */
  this.indexes = indexes;

  /**
   * @public {!Array<!Entry>}
   * @const
   */
  this.entries = entries;

  /**
   * @public {!Array<string>}
   */
  this.mimeTypes = [];

  /**
   * @public {number}
   */
  this.totalCount = 0;

  /**
   * @public {number}
   */
  this.fileCount = 0;

  /**
   * @public {number}
   */
  this.directoryCount = 0;

  /**
   * @public {boolean}
   */
  this.anyFilesNotInCache = true;

  /**
   * @public {boolean}
   */
  this.anyFilesHosted = true;

  /**
   * @public {?string}
   */
  this.iconType = null;

  /**
   * @private {Promise<boolean>}
   */
  this.additionalPromise_ = null;

  entries.forEach(function(entry) {
    if (this.iconType == null) {
      this.iconType = FileType.getIcon(entry);
    } else if (this.iconType != 'unknown') {
      var iconType = FileType.getIcon(entry);
      if (this.iconType != iconType) {
        this.iconType = 'unknown';
      }
    }

    if (entry.isFile) {
      this.fileCount += 1;
    } else {
      this.directoryCount += 1;
    }
    this.totalCount++;
  }.bind(this));
}

FileSelection.prototype.computeAdditional = function(metadataModel) {
  if (!this.additionalPromise_) {
    this.additionalPromise_ =
        metadataModel
            .get(
                this.entries,
                constants.FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES)
            .then(function(props) {
              var present = props.filter(function(p) {
                // If no availableOffline property, then assume it's available.
                return !('availableOffline' in p) || p.availableOffline;
              });
              const hosted = props.filter(function(p) {
                return p.hosted;
              });
              this.anyFilesNotInCache = present.length !== props.length;
              this.anyFilesHosted = !!hosted.length;
              this.mimeTypes = props.map(function(value) {
                return value.contentMimeType || '';
              });
              return true;
            }.bind(this));
  }
  return this.additionalPromise_;
};

/**
 * This object encapsulates everything related to current selection.
 *
 * @param {!DirectoryModel} directoryModel
 * @param {!FileOperationManager} fileOperationManager
 * @param {!ListContainer} listContainer
 * @param {!MetadataModel} metadataModel
 * @param {!VolumeManager} volumeManager
 * @param {!AllowedPaths} allowedPaths
 * @extends {cr.EventTarget}
 * @constructor
 * @struct
 */
function FileSelectionHandler(
    directoryModel, fileOperationManager, listContainer, metadataModel,
    volumeManager, allowedPaths) {
  cr.EventTarget.call(this);

  /**
   * @private {DirectoryModel}
   * @const
   */
  this.directoryModel_ = directoryModel;

  /**
   * @private {ListContainer}
   * @const
   */
  this.listContainer_ = listContainer;

  /**
   * @private {MetadataModel}
   * @const
   */
  this.metadataModel_ = metadataModel;

  /**
   * @private {VolumeManager}
   * @const
   */
  this.volumeManager_ = volumeManager;

  /**
   * @type {FileSelection}
   */
  this.selection = new FileSelection([], []);

  /**
   * @private {?number}
   */
  this.selectionUpdateTimer_ = 0;

  /**
   * @private {number}
   */
  this.lastFileSelectionTime_ = Date.now();

  /**
   * @private {AllowedPaths}
   * @const
   */
  this.allowedPaths_ = allowedPaths;

  util.addEventListenerToBackgroundComponent(
      assert(fileOperationManager), 'entries-changed',
      this.onFileSelectionChanged.bind(this));
  // Register evnets to update file selections.
  directoryModel.addEventListener(
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
   * Dispatched |UPDATE_DELAY| ms after the selecton is changed.
   * If multiple changes are happened during the term, only one CHANGE_THROTTLED
   * event is dispatched.
   */
  CHANGE_THROTTLED: 'changethrottled'
};

/**
 * Delay in milliseconds before recalculating the selection in case the
 * selection is changed fast, or there are many items. Used to avoid freezing
 * the UI.
 * @const {number}
 */
FileSelectionHandler.UPDATE_DELAY = 200;

/**
 * Number of items in the selection which triggers the update delay. Used to
 * let the Material Design animations complete before performing a heavy task
 * which would cause the UI freezing.
 * @const {number}
 */
FileSelectionHandler.NUMBER_OF_ITEMS_HEAVY_TO_COMPUTE = 100;

/**
 * FileSelectionHandler extends cr.EventTarget.
 */
FileSelectionHandler.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Update the UI when the selection model changes.
 */
FileSelectionHandler.prototype.onFileSelectionChanged = function() {
  var indexes = this.listContainer_.selectionModel.selectedIndexes;
  var entries = indexes.map(function(index) {
    return /** @type {!Entry} */ (
        this.directoryModel_.getFileList().item(index));
  }.bind(this));
  this.selection = new FileSelection(indexes, entries);

  if (this.selectionUpdateTimer_) {
    clearTimeout(this.selectionUpdateTimer_);
    this.selectionUpdateTimer_ = null;
  }

  // The rest of the selection properties are computed via (sometimes lengthy)
  // asynchronous calls. We initiate these calls after a timeout. If the
  // selection is changing quickly we only do this once when it slows down.

  var updateDelay = FileSelectionHandler.UPDATE_DELAY;
  var now = Date.now();

  if (now > (this.lastFileSelectionTime_ || 0) + updateDelay &&
      indexes.length < FileSelectionHandler.NUMBER_OF_ITEMS_HEAVY_TO_COMPUTE) {
    // The previous selection change happened a while ago and there is few
    // selected items, so computation is lightweight. Update the UI without
    // delay.
    updateDelay = 0;
  }
  this.lastFileSelectionTime_ = now;

  var selection = this.selection;
  this.selectionUpdateTimer_ = setTimeout(function() {
    this.selectionUpdateTimer_ = null;
    if (this.selection === selection) {
      this.updateFileSelectionAsync_(selection);
    }
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
  if (this.selection !== selection) {
    return;
  }

  // Calculate all additional and heavy properties.
  selection.computeAdditional(this.metadataModel_).then(() => {
    if (this.selection !== selection) {
      return;
    }

    cr.dispatchSimpleEvent(
        this, FileSelectionHandler.EventType.CHANGE_THROTTLED);
  });
};

/**
 * Returns true if all files in the selection files are selectable.
 * @return {boolean}
 */
FileSelectionHandler.prototype.isAvailable = function() {
  if (!this.directoryModel_.isOnDrive()) {
    return true;
  }

  return !(
      this.isOfflineWithUncachedFilesSelected_() ||
      this.isDialogWithHostedFilesSelected_());
};


/**
 * Returns true if we're offline with any selected files absent from the cache.
 * @return {boolean}
 * @private
 */
FileSelectionHandler.prototype.isOfflineWithUncachedFilesSelected_ =
    function() {
  return this.volumeManager_.getDriveConnectionState().type ===
      VolumeManagerCommon.DriveConnectionType.OFFLINE &&
      this.selection.anyFilesNotInCache;
};

/**
 * Returns true if we're a dialog requiring real files with hosted files
 * selected.
 * @return {boolean}
 * @private
 */
FileSelectionHandler.prototype.isDialogWithHostedFilesSelected_ = function() {
  return this.allowedPaths_ !== AllowedPaths.ANY_PATH_OR_URL &&
      this.selection.anyFilesHosted;
};
