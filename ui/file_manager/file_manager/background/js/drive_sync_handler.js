// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Handler of the background page for the drive sync events.
 * @param {ProgressCenter} progressCenter Progress center to submit the
 *     progressing items.
 * @constructor
 */
function DriveSyncHandler(progressCenter) {
  /**
   * Progress center to submit the progressing item.
   * @type {ProgressCenter}
   * @private
   */
  this.progressCenter_ = progressCenter;

  /**
   * Counter for error ID.
   * @type {number}
   * @private
   */
  this.errorIdCounter_ = 0;

  /**
   * Progress center item.
   * @type {ProgressCenterItem}
   * @private
   */
  this.item_ = new ProgressCenterItem();
  this.item_.id = 'drive-sync';

  /**
   * If the property is true, this item is syncing.
   * @type {boolean}
   * @private
   */
  this.syncing_ = false;

  /**
   * Async queue.
   * @type {AsyncUtil.Queue}
   * @private
   */
  this.queue_ = new AsyncUtil.Queue();

  // Register events.
  chrome.fileManagerPrivate.onFileTransfersUpdated.addListener(
      this.onFileTransfersUpdated_.bind(this));
  chrome.fileManagerPrivate.onDriveSyncError.addListener(
      this.onDriveSyncError_.bind(this));
}

/**
 * Completed event name.
 * @type {string}
 * @const
 */
DriveSyncHandler.COMPLETED_EVENT = 'completed';

/**
 * Progress ID of the drive sync error.
 * @type {string}
 * @const
 */
DriveSyncHandler.DRIVE_SYNC_ERROR_PREFIX = 'drive-sync-error-';

DriveSyncHandler.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * @return {boolean} Whether the handler is having syncing items or not.
   */
  get syncing() {
    return this.syncing_;
  }
};

/**
 * Handles file transfer updated events.
 * @param {FileTransferStatus} status Transfer status.
 * @private
 */
DriveSyncHandler.prototype.onFileTransfersUpdated_ = function(status) {
  switch (status.transferState) {
    case 'added':
    case 'in_progress':
    case 'started':
      this.updateItem_(status);
      break;
    case 'completed':
    case 'failed':
      if (status.num_total_jobs === 1)
        this.removeItem_(status);
      break;
    default:
      throw new Error(
          'Invalid transfer state: ' + status.transferState + '.');
  }
};

/**
 * Updates the item involved with the given status.
 * @param {FileTransferStatus} status Transfer status.
 * @private
 */
DriveSyncHandler.prototype.updateItem_ = function(status) {
  this.queue_.run(function(callback) {
    webkitResolveLocalFileSystemURL(status.fileUrl, function(entry) {
      this.item_.state = ProgressItemState.PROGRESSING;
      this.item_.type = ProgressItemType.SYNC;
      this.item_.quiet = true;
      this.syncing_ = true;
      if (status.num_total_jobs > 1)
        this.item_.message = strf('SYNC_FILE_NUMBER', status.num_total_jobs);
      else
        this.item_.message = strf('SYNC_FILE_NAME', entry.name);
      this.item_.cancelCallback = this.requestCancel_.bind(this, entry);
      this.item_.progressValue = status.processed;
      this.item_.progressMax = status.total;
      this.progressCenter_.updateItem(this.item_);
      callback();
    }.bind(this), function(error) {
      console.warn('Resolving URL ' + status.fileUrl + ' is failed: ', error);
      callback();
    });
  }.bind(this));
};

/**
 * Removes the item involved with the given status.
 * @param {FileTransferStatus} status Transfer status.
 * @private
 */
DriveSyncHandler.prototype.removeItem_ = function(status) {
  this.queue_.run(function(callback) {
    this.item_.state = status.transferState === 'completed' ?
        ProgressItemState.COMPLETED : ProgressItemState.CANCELED;
    this.progressCenter_.updateItem(this.item_);
    this.syncing_ = false;
    this.dispatchEvent(new Event(DriveSyncHandler.COMPLETED_EVENT));
    callback();
  }.bind(this));
};

/**
 * Requests to cancel for the given files' drive sync.
 * @param {Entry} entry Entry to be canceled.
 * @private
 */
DriveSyncHandler.prototype.requestCancel_ = function(entry) {
  // Cancel all jobs.
  chrome.fileManagerPrivate.cancelFileTransfers(function() {});
};

/**
 * Handles drive's sync errors.
 * @param {DriveSyncErrorEvent} event Drive sync error event.
 * @private
 */
DriveSyncHandler.prototype.onDriveSyncError_ = function(event) {
  webkitResolveLocalFileSystemURL(event.fileUrl, function(entry) {
    var item = new ProgressCenterItem();
    item.id =
        DriveSyncHandler.DRIVE_SYNC_ERROR_PREFIX + (this.errorIdCounter_++);
    item.type = ProgressItemType.SYNC;
    item.quiet = true;
    item.state = ProgressItemState.ERROR;
    switch (event.type) {
      case 'delete_without_permission':
        item.message =
            strf('SYNC_DELETE_WITHOUT_PERMISSION_ERROR', entry.name);
        break;
      case 'service_unavailable':
        item.message = str('SYNC_SERVICE_UNAVAILABLE_ERROR');
        break;
      case 'misc':
        item.message = strf('SYNC_MISC_ERROR', entry.name);
        break;
    }
    this.progressCenter_.updateItem(item);
  }.bind(this));
};
