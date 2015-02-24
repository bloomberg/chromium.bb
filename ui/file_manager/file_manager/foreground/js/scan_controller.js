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
   * Whether a scan is updated at least once. If true, spinner should disappear.
   * @type {boolean}
   * @private
   */
  this.scanUpdatedAtLeastOnceOrCompleted_ = false;

  /**
   * Timer ID to delay UI refresh after a scan is completed.
   * @type {number}
   * @private
   */
  this.scanCompletedTimer_ = 0;

  /**
   * Timer ID to delay UI refresh after a scan is updated.
   * @type {number}
   * @private
   */
  this.scanUpdatedTimer_ = 0;

  /**
   * Last value of hosted files disabled.
   * @type {?boolean}
   * @private
   */
  this.lastHostedFilesDisabled_ = null;

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
  chrome.fileManagerPrivate.onPreferencesChanged.addListener(
      this.onPreferencesChanged_.bind(this));
  this.onPreferencesChanged_();

  this.spinnerController_.addEventListener(
      'spinner-shown', this.onSpinnerShown_.bind(this));
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

  this.scanUpdatedAtLeastOnceOrCompleted_ = false;
  if (this.scanCompletedTimer_) {
    clearTimeout(this.scanCompletedTimer_);
    this.scanCompletedTimer_ = 0;
  }

  if (this.scanUpdatedTimer_) {
    clearTimeout(this.scanUpdatedTimer_);
    this.scanUpdatedTimer_ = 0;
  }

  this.spinnerController_.showLater();
};

/**
 * @private
 */
ScanController.prototype.onScanCompleted_ = function() {
  if (!this.scanInProgress_) {
    console.error('Scan-completed event recieved. But scan is not started.');
    return;
  }

  if (this.commandHandler_)
    this.commandHandler_.updateAvailability();
  this.spinnerController_.hide();

  if (this.scanUpdatedTimer_) {
    clearTimeout(this.scanUpdatedTimer_);
    this.scanUpdatedTimer_ = 0;
  }

  // To avoid flickering postpone updating the ui by a small amount of time.
  // There is a high chance, that metadata will be received within 50 ms.
  this.scanCompletedTimer_ = setTimeout(function() {
    // Check if batch updates are already finished by onScanUpdated_().
    if (!this.scanUpdatedAtLeastOnceOrCompleted_) {
      this.scanUpdatedAtLeastOnceOrCompleted_ = true;
    }

    this.scanInProgress_ = false;
    this.listContainer_.endBatchUpdates();
    this.scanCompletedTimer_ = 0;
  }.bind(this), 50);
};

/**
 * @private
 */
ScanController.prototype.onScanUpdated_ = function() {
  if (!this.scanInProgress_) {
    console.error('Scan-updated event recieved. But scan is not started.');
    return;
  }

  if (this.scanUpdatedTimer_ || this.scanCompletedTimer_)
    return;

  // Show contents incrementally by finishing batch updated, but only after
  // 200ms elapsed, to avoid flickering when it is not necessary.
  this.scanUpdatedTimer_ = setTimeout(function() {
    // We need to hide the spinner only once.
    if (!this.scanUpdatedAtLeastOnceOrCompleted_) {
      this.scanUpdatedAtLeastOnceOrCompleted_ = true;
      this.spinnerController_.hide();
    }

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
    console.error('Scan-cancelled event recieved. But scan is not started.');
    return;
  }

  if (this.commandHandler_)
    this.commandHandler_.updateAvailability();
  this.spinnerController_.hide();
  if (this.scanCompletedTimer_) {
    clearTimeout(this.scanCompletedTimer_);
    this.scanCompletedTimer_ = 0;
  }
  if (this.scanUpdatedTimer_) {
    clearTimeout(this.scanUpdatedTimer_);
    this.scanUpdatedTimer_ = 0;
  }
  // Finish unfinished batch updates.
  if (!this.scanUpdatedAtLeastOnceOrCompleted_) {
    this.scanUpdatedAtLeastOnceOrCompleted_ = true;
  }

  this.scanInProgress_ = false;
  this.listContainer_.endBatchUpdates();
};

/**
 * Handle the 'rescan-completed' from the DirectoryModel.
 * @private
 */
ScanController.prototype.onRescanCompleted_ = function() {
  this.selectionHandler_.onFileSelectionChanged();
};

/**
 * Handles preferences change and starts rescan if needed.
 * @private
 */
ScanController.prototype.onPreferencesChanged_ = function() {
  chrome.fileManagerPrivate.getPreferences(function(prefs) {
    if (chrome.runtime.lastError)
      return;
    if (this.lastHostedFilesDisabled_ !== null &&
        this.lastHostedFilesDisabled_ !== prefs.hostedFilesDisabled &&
        this.directoryModel_.isOnDrive()) {
      this.directoryModel_.rescan(false);
    }
    this.lastHostedFilesDisabled_ = prefs.hostedFilesDisabled;
  }.bind(this));
};

/**
 * When a spinner is shown, updates the UI to remove items in the previous
 * directory.
 */
ScanController.prototype.onSpinnerShown_ = function() {
  if (this.scanInProgress_) {
    this.listContainer_.endBatchUpdates();
    this.listContainer_.startBatchUpdates();
  }
};
