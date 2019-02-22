// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handler for scan related events of DirectoryModel.
 *
 * @param {!DirectoryModel} directoryModel
 * @param {!ListContainer} listContainer
 * @param {!SpinnerController} spinnerController
 * @param {!CommandHandler} commandHandler
 * @param {!FileSelectionHandler} selectionHandler
 * @constructor
 * @struct
 */
function ScanController(
    directoryModel,
    listContainer,
    spinnerController,
    commandHandler,
    selectionHandler) {
  /**
   * @type {!DirectoryModel}
   * @const
   * @private
   */
  this.directoryModel_ = directoryModel;

  /**
   * @type {!ListContainer}
   * @const
   * @private
   */
  this.listContainer_ = listContainer;

  /**
   * @type {!SpinnerController}
   * @const
   * @private
   */
  this.spinnerController_ = spinnerController;

  /**
   * @type {!CommandHandler}
   * @const
   * @private
   */
  this.commandHandler_ = commandHandler;

  /**
   * @type {!FileSelectionHandler}
   * @const
   * @private
   */
  this.selectionHandler_ = selectionHandler;

  /**
   * Whether a scan is in progress.
   * @type {boolean}
   * @private
   */
  this.scanInProgress_ = false;

  /**
   * Timer ID to delay UI refresh after a scan is updated.
   * @type {number}
   * @private
   */
  this.scanUpdatedTimer_ = 0;

  /**
   * @type {?function()}
   * @private
   */
  this.spinnerHideCallback_ = null;

  this.directoryModel_.addEventListener(
      'scan-started', this.onScanStarted_.bind(this));
  this.directoryModel_.addEventListener(
      'scan-completed', this.onScanCompleted_.bind(this));
  this.directoryModel_.addEventListener(
      'scan-failed', this.onScanCancelled_.bind(this));
  this.directoryModel_.addEventListener(
      'scan-cancelled', this.onScanCancelled_.bind(this));
  this.directoryModel_.addEventListener(
      'scan-updated', this.onScanUpdated_.bind(this));
  this.directoryModel_.addEventListener(
      'rescan-completed', this.onRescanCompleted_.bind(this));
}

/**
 * @private
 */
ScanController.prototype.onScanStarted_ = function() {
  if (this.scanInProgress_)
    this.listContainer_.endBatchUpdates();

  if (this.commandHandler_)
    this.commandHandler_.updateAvailability();

  this.listContainer_.startBatchUpdates();
  this.scanInProgress_ = true;

  if (this.scanUpdatedTimer_) {
    clearTimeout(this.scanUpdatedTimer_);
    this.scanUpdatedTimer_ = 0;
  }

  this.hideSpinner_();
  this.spinnerHideCallback_ = this.spinnerController_.showWithDelay(
      500, this.onSpinnerShown_.bind(this));
};

/**
 * @private
 */
ScanController.prototype.onScanCompleted_ = function() {
  if (!this.scanInProgress_) {
    console.error('Scan-completed event received. But scan is not started.');
    return;
  }

  this.hideSpinner_();

  if (this.scanUpdatedTimer_) {
    clearTimeout(this.scanUpdatedTimer_);
    this.scanUpdatedTimer_ = 0;
  }

  this.scanInProgress_ = false;
  this.listContainer_.endBatchUpdates();

  if (this.commandHandler_)
    this.commandHandler_.updateAvailability();
};

/**
 * @private
 */
ScanController.prototype.onScanUpdated_ = function() {
  if (!this.scanInProgress_) {
    console.error('Scan-updated event received. But scan is not started.');
    return;
  }

  if (this.scanUpdatedTimer_)
    return;

  // Show contents incrementally by finishing batch updated, but only after
  // 200ms elapsed, to avoid flickering when it is not necessary.
  this.scanUpdatedTimer_ = setTimeout(function() {
    this.hideSpinner_();

    // Update the UI.
    if (this.scanInProgress_) {
      this.listContainer_.endBatchUpdates();
      this.listContainer_.startBatchUpdates();
    }
    this.scanUpdatedTimer_ = 0;
  }.bind(this), 200);
};

/**
 * @private
 */
ScanController.prototype.onScanCancelled_ = function() {
  if (!this.scanInProgress_) {
    console.error('Scan-cancelled event received. But scan is not started.');
    return;
  }

  this.hideSpinner_();

  if (this.scanUpdatedTimer_) {
    clearTimeout(this.scanUpdatedTimer_);
    this.scanUpdatedTimer_ = 0;
  }

  this.scanInProgress_ = false;
  this.listContainer_.endBatchUpdates();

  if (this.commandHandler_)
    this.commandHandler_.updateAvailability();
};

/**
 * Handle the 'rescan-completed' from the DirectoryModel.
 * @private
 */
ScanController.prototype.onRescanCompleted_ = function() {
  this.selectionHandler_.onFileSelectionChanged();
};

/**
 * When a spinner is shown, updates the UI to remove items in the previous
 * directory.
 * @private
 */
ScanController.prototype.onSpinnerShown_ = function() {
  if (this.scanInProgress_) {
    this.listContainer_.endBatchUpdates();
    this.listContainer_.startBatchUpdates();
  }
};

/**
 * Hides the spinner if it's shown or scheduled to be shown.
 * @private
 */
ScanController.prototype.hideSpinner_ = function() {
  if (this.spinnerHideCallback_) {
    this.spinnerHideCallback_();
    this.spinnerHideCallback_ = null;
  }
};
