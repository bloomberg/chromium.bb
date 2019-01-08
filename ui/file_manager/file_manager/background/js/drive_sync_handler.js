// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handler of the background page for the Drive sync events.
 *
 * @constructor
 * @struct
 * @implements {DriveSyncHandler}
 * @extends {cr.EventTarget}
 */
function DriveSyncHandlerImpl(progressCenter) {
  /**
   * Progress center to submit the progressing item.
   * @type {ProgressCenter}
   * @const
   * @private
   */
  this.progressCenter_ = progressCenter;

  /**
   * Predefined error ID for out of quota messages.
   * @type {number}
   * @const
   * @private
   */
  this.driveErrorIdOutOfQuota_ = 1;

  /**
   * Maximum reserved ID for predefined errors.
   * @type {number}
   * @const
   * @private
   */
  this.driveErrorIdMax_ = this.driveErrorIdOutOfQuota_;

  /**
   * Counter for error ID.
   * @type {number}
   * @private
   */
  this.errorIdCounter_ = this.driveErrorIdMax_ + 1;

  /**
   * Progress center item.
   * @type {ProgressCenterItem}
   * @const
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
   * Whether the sync is disabled on cellular network or not.
   * @type {boolean}
   * @private
   */
  this.cellularDisabled_ = false;

  /**
   * Async queue.
   * @type {AsyncUtil.Queue}
   * @const
   * @private
   */
  this.queue_ = new AsyncUtil.Queue();

  // Register events.
  chrome.fileManagerPrivate.onFileTransfersUpdated.addListener(
      this.onFileTransfersUpdated_.bind(this));
  chrome.fileManagerPrivate.onDriveSyncError.addListener(
      this.onDriveSyncError_.bind(this));
  chrome.notifications.onButtonClicked.addListener(
      this.onNotificationButtonClicked_.bind(this));
  chrome.fileManagerPrivate.onPreferencesChanged.addListener(
      this.onPreferencesChanged_.bind(this));
  chrome.fileManagerPrivate.onDriveConnectionStatusChanged.addListener(
      this.onDriveConnectionStatusChanged_.bind(this));

  // Set initial values.
  this.onPreferencesChanged_();
}

DriveSyncHandlerImpl.prototype = /** @struct */ {
  __proto__: cr.EventTarget.prototype,

  /**
   * @return {boolean} Whether the handler is syncing items or not.
   */
  get syncing() {
    return this.syncing_;
  }
};

/**
 * Completed event name.
 * @type {string}
 * @private
 * @const
 */
DriveSyncHandlerImpl.DRIVE_SYNC_COMPLETED_EVENT = 'completed';

/**
 * Returns the completed event name.
 * @return {string}
 */
DriveSyncHandlerImpl.prototype.getCompletedEventName = function() {
  return DriveSyncHandlerImpl.DRIVE_SYNC_COMPLETED_EVENT;
};

/**
 * Returns whether the Drive sync is currently suppressed or not.
 * @return {boolean}
 */
DriveSyncHandlerImpl.prototype.isSyncSuppressed = function() {
  return navigator.connection.type === 'cellular' &&
      this.cellularDisabled_;
};

/**
 * Notification ID of the disabled mobile sync notification.
 * @type {string}
 * @private
 * @const
 */
DriveSyncHandlerImpl.DISABLED_MOBILE_SYNC_NOTIFICATION_ID_ =
    'disabled-mobile-sync';

/**
 * Shows a notification that Drive sync is disabled on cellular networks.
 */
DriveSyncHandlerImpl.prototype.showDisabledMobileSyncNotification = function() {
  chrome.notifications.create(
      DriveSyncHandlerImpl.DISABLED_MOBILE_SYNC_NOTIFICATION_ID_, {
        type: 'basic',
        title: chrome.runtime.getManifest().name,
        message: str('DISABLED_MOBILE_SYNC_NOTIFICATION_MESSAGE'),
        iconUrl: chrome.runtime.getURL('/common/images/icon96.png'),
        buttons:
            [{title: str('DISABLED_MOBILE_SYNC_NOTIFICATION_ENABLE_BUTTON')}]
      },
      function() {});
};

/**
 * Handles file transfer updated events.
 * @param {chrome.fileManagerPrivate.FileTransferStatus} status Transfer status.
 * @private
 */
DriveSyncHandlerImpl.prototype.onFileTransfersUpdated_ = function(status) {
  switch (status.transferState) {
    case 'in_progress':
      this.updateItem_(status);
      break;
    case 'completed':
    case 'failed':
      if ((status.hideWhenZeroJobs && status.num_total_jobs === 0) ||
          (!status.hideWhenZeroJobs && status.num_total_jobs === 1)) {
        this.removeItem_(status);
      }
      break;
    default:
      throw new Error(
          'Invalid transfer state: ' + status.transferState + '.');
  }
};

/**
 * Updates the item involved with the given status.
 * @param {chrome.fileManagerPrivate.FileTransferStatus} status Transfer status.
 * @private
 */
DriveSyncHandlerImpl.prototype.updateItem_ = function(status) {
  this.queue_.run(function(callback) {
    window.webkitResolveLocalFileSystemURL(
        status.fileUrl,
        function(entry) {
          this.item_.state = ProgressItemState.PROGRESSING;
          this.item_.type = ProgressItemType.SYNC;
          this.item_.quiet = true;
          this.syncing_ = true;
          if (status.num_total_jobs > 1) {
            this.item_.message =
                strf('SYNC_FILE_NUMBER', status.num_total_jobs);
          } else {
            this.item_.message = strf('SYNC_FILE_NAME', entry.name);
          }
          this.item_.progressValue = status.processed || 0;
          this.item_.progressMax = status.total || 0;
          this.progressCenter_.updateItem(this.item_);
          callback();
        }.bind(this),
        function(error) {
          console.warn(
              'Resolving URL ' + status.fileUrl + ' is failed: ', error);
          callback();
        });
  }.bind(this));
};

/**
 * Removes the item involved with the given status.
 * @param {chrome.fileManagerPrivate.FileTransferStatus} status Transfer status.
 * @private
 */
DriveSyncHandlerImpl.prototype.removeItem_ = function(status) {
  this.queue_.run(function(callback) {
    this.item_.state = status.transferState === 'completed' ?
        ProgressItemState.COMPLETED : ProgressItemState.CANCELED;
    this.progressCenter_.updateItem(this.item_);
    this.syncing_ = false;
    this.dispatchEvent(new Event(this.getCompletedEventName()));
    callback();
  }.bind(this));
};

/**
 * Drive sync error prefix.
 * @type {string}
 * @private
 * @const
 */
DriveSyncHandlerImpl.DRIVE_SYNC_ERROR_PREFIX = 'drive-sync-error-';

/**
 * Handles drive's sync errors.
 * @param {chrome.fileManagerPrivate.DriveSyncErrorEvent} event Drive sync
 * error event.
 * @private
 */
DriveSyncHandlerImpl.prototype.onDriveSyncError_ = function(event) {
  window.webkitResolveLocalFileSystemURL(event.fileUrl, function(entry) {
    var item = new ProgressCenterItem();
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
      case 'no_server_space':
        item.message = strf('SYNC_NO_SERVER_SPACE', entry.name);
        // This error will reappear every time sync is retried, so we use a
        // fixed ID to avoid spamming the user.
        item.id = DriveSyncHandlerImpl.DRIVE_SYNC_ERROR_PREFIX +
            this.driveErrorIdOutOfQuota_;
        break;
      case 'misc':
        item.message = strf('SYNC_MISC_ERROR', entry.name);
        break;
    }
    if (!item.id) {
      item.id = DriveSyncHandlerImpl.DRIVE_SYNC_ERROR_PREFIX +
          (this.errorIdCounter_++);
    }
    this.progressCenter_.updateItem(item);
  }.bind(this));
};

/**
 * Handles notification's button click.
 * @param {string} notificationId Notification ID.
 * @param {number} buttonIndex Index of the button.
 * @private
 */
DriveSyncHandlerImpl.prototype.onNotificationButtonClicked_ = function(
    notificationId, buttonIndex) {
  var expectedId = DriveSyncHandlerImpl.DISABLED_MOBILE_SYNC_NOTIFICATION_ID_;
  if (notificationId !== expectedId) {
    return;
  }
  chrome.notifications.clear(notificationId, function() {});
  chrome.fileManagerPrivate.setPreferences({cellularDisabled: false});
};

/**
 * Handles preferences change.
 * @private
 */
DriveSyncHandlerImpl.prototype.onPreferencesChanged_ = function() {
  chrome.fileManagerPrivate.getPreferences(function(pref) {
    this.cellularDisabled_ = pref.cellularDisabled;
  }.bind(this));
};

/**
 * Handles connection state change.
 * @private
 */
DriveSyncHandlerImpl.prototype.onDriveConnectionStatusChanged_ = function() {
  chrome.fileManagerPrivate.getDriveConnectionState((state) => {
    // If offline, hide any sync progress notifications. When online again, the
    // Drive sync client may retry syncing and trigger onFileTransfersUpdated
    // events, causing it to be shown again.
    if (state.type == 'offline' && state.reason == 'no_network' &&
        this.syncing_) {
      this.syncing_ = false;
      this.item_.state = ProgressItemState.CANCELED;
      this.progressCenter_.updateItem(this.item_);
      this.dispatchEvent(new Event(this.getCompletedEventName()));
    }
  });
};
