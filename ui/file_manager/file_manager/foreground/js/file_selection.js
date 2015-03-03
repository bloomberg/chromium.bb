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
   * @type {boolean}
   */
  this.allDriveFilesPresent = false;

  /**
   * @type {?string}
   */
  this.iconType = null;

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
    this.asyncInitPromise_ = this.fileManager_.getMetadataModel().get(
        this.entries, ['availableOffline', 'contentMimeType']
    ).then(function(props) {
      var present = props.filter(function(p) { return p.availableOffline; });
      this.allDriveFilesPresent = present.length == props.length;
      // Collect all of the mime types and push that info into the
      // selection.
      this.mimeTypes = props.map(function(value) {
        return value.contentMimeType || '';
      });
      return this.tasks.init(this.entries, this.mimeTypes);
    }.bind(this));
  }
  return this.asyncInitPromise_;
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
 * Update the UI when the selection model changes.
 */
FileSelectionHandler.prototype.onFileSelectionChanged = function() {
  var indexes =
      this.fileManager_.getCurrentList().selectionModel.selectedIndexes;
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
