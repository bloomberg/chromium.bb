// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Handler of device event.
 * @constructor
 * @extends {cr.EventTarget}
 */
function DeviceHandler() {
  cr.EventTarget.call(this);

  /**
   * Map of device path and mount status of devices.
   * @type {Object.<string, DeviceHandler.MountStatus>}
   * @private
   */
  this.mountStatus_ = {};

  /**
   * Map of device path and volumeId of the volume that should be navigated to
   * from the 'device inserted' notification.
   * @type {Object.<string, DirectoryEntry>}
   * @private
   */
  this.navigationVolumes_ = {};

  /**
   * Whether the device is just after starting up or not.
   *
   * @type {boolean}
   * @private
   */
  this.isStartup_ = false;

  chrome.fileBrowserPrivate.onDeviceChanged.addListener(
      this.onDeviceChanged_.bind(this));
  chrome.fileBrowserPrivate.onMountCompleted.addListener(
      this.onMountCompleted_.bind(this));
  chrome.notifications.onButtonClicked.addListener(
      this.onNotificationButtonClicked_.bind(this));
  chrome.runtime.onStartup.addListener(
      this.onStartup_.bind(this));
}

DeviceHandler.prototype = {
  __proto__: cr.EventTarget.prototype
};

/**
 * An event name trigerred when a user requests to navigate to a volume.
 * The event object must have a volumeId property.
 * @type {string}
 * @const
 */
DeviceHandler.VOLUME_NAVIGATION_REQUESTED = 'volumenavigationrequested';

/**
 * Notification type.
 * @param {string} prefix Prefix of notification ID.
 * @param {string} title String ID of title.
 * @param {string} message String ID of message.
 * @param {string=} opt_buttonLabel String ID of the button label.
 * @constructor
 */
DeviceHandler.Notification = function(prefix, title, message, opt_buttonLabel) {
  /**
   * Prefix of notification ID.
   * @type {string}
   */
  this.prefix = prefix;

  /**
   * String ID of title.
   * @type {string}
   */
  this.title = title;

  /**
   * String ID of message.
   * @type {string}
   */
  this.message = message;

  /**
   * String ID of button label.
   * @type {?string}
   */
  this.buttonLabel = opt_buttonLabel || null;

  /**
   * Queue of API call.
   * @type {AsyncUtil.Queue}
   * @private
   */
  this.queue_ = new AsyncUtil.Queue();

  /**
   * Timeout ID.
   * @type {number}
   * @private
   */
  this.pendingShowTimerId_ = 0;

  Object.seal(this);
};

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE = new DeviceHandler.Notification(
    'device',
    'REMOVABLE_DEVICE_DETECTION_TITLE',
    'REMOVABLE_DEVICE_SCANNING_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_NAVIGATION = new DeviceHandler.Notification(
    'deviceNavigation',
    'REMOVABLE_DEVICE_DETECTION_TITLE',
    'REMOVABLE_DEVICE_NAVIGATION_MESSAGE',
    'REMOVABLE_DEVICE_NAVIGATION_BUTTON_LABEL');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_FAIL = new DeviceHandler.Notification(
    'deviceFail',
    'REMOVABLE_DEVICE_DETECTION_TITLE',
    'DEVICE_UNSUPPORTED_DEFAULT_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_EXTERNAL_STORAGE_DISABLED =
    new DeviceHandler.Notification(
        'deviceFail',
        'REMOVABLE_DEVICE_DETECTION_TITLE',
        'EXTERNAL_STORAGE_DISABLED_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_HARD_UNPLUGGED =
    new DeviceHandler.Notification(
        'hardUnplugged',
        'DEVICE_HARD_UNPLUGGED_TITLE',
        'DEVICE_HARD_UNPLUGGED_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.FORMAT_START = new DeviceHandler.Notification(
    'formatStart',
    'FORMATTING_OF_DEVICE_PENDING_TITLE',
    'FORMATTING_OF_DEVICE_PENDING_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.FORMAT_SUCCESS = new DeviceHandler.Notification(
    'formatSuccess',
    'FORMATTING_OF_DEVICE_FINISHED_TITLE',
    'FORMATTING_FINISHED_SUCCESS_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.FORMAT_FAIL = new DeviceHandler.Notification(
    'formatFail',
    'FORMATTING_OF_DEVICE_FAILED_TITLE',
    'FORMATTING_FINISHED_FAILURE_MESSAGE');

/**
 * Shows the notification for the device path.
 * @param {string} devicePath Device path.
 * @param {string=} opt_message Message overrides the default message.
 * @return {string} Notification ID.
 */
DeviceHandler.Notification.prototype.show = function(devicePath, opt_message) {
  this.clearTimeout_();
  var notificationId = this.makeId_(devicePath);
  this.queue_.run(function(callback) {
    var buttons =
        this.buttonLabel ? [{title: str(this.buttonLabel)}] : undefined;
    chrome.notifications.create(
        notificationId,
        {
          type: 'basic',
          title: str(this.title),
          message: opt_message || str(this.message),
          iconUrl: chrome.runtime.getURL('/common/images/icon96.png'),
          buttons: buttons
        },
        callback);
  }.bind(this));
  return notificationId;
};

/**
 * Shows the notification after 5 seconds.
 * @param {string} devicePath Device path.
 */
DeviceHandler.Notification.prototype.showLater = function(devicePath) {
  this.clearTimeout_();
  this.pendingShowTimerId_ = setTimeout(this.show.bind(this, devicePath), 5000);
};

/**
 * Hides the notification for the device path.
 * @param {string} devicePath Device path.
 */
DeviceHandler.Notification.prototype.hide = function(devicePath) {
  this.clearTimeout_();
  this.queue_.run(function(callback) {
    chrome.notifications.clear(this.makeId_(devicePath), callback);
  }.bind(this));
};

/**
 * Makes a notification ID for the device path.
 * @param {string} devicePath Device path.
 * @return {string} Notification ID.
 * @private
 */
DeviceHandler.Notification.prototype.makeId_ = function(devicePath) {
  return this.prefix + ':' + devicePath;
};

/**
 * Cancels the timeout request.
 * @private
 */
DeviceHandler.Notification.prototype.clearTimeout_ = function() {
  if (this.pendingShowTimerId_) {
    clearTimeout(this.pendingShowTimerId_);
    this.pendingShowTimerId_ = 0;
  }
};

/**
 * Handles notifications from C++ sides.
 * @param {DeviceEvent} event Device event.
 * @private
 */
DeviceHandler.prototype.onDeviceChanged_ = function(event) {
  switch (event.type) {
    case 'added':
      if (!this.isStartup_)
        DeviceHandler.Notification.DEVICE.showLater(event.devicePath);
      this.mountStatus_[event.devicePath] = DeviceHandler.MountStatus.NO_RESULT;
      break;
    case 'disabled':
      DeviceHandler.Notification.DEVICE_EXTERNAL_STORAGE_DISABLED.show(
          event.devicePath);
      break;
    case 'scan_canceled':
      DeviceHandler.Notification.DEVICE.hide(event.devicePath);
      break;
    case 'removed':
      DeviceHandler.Notification.DEVICE.hide(event.devicePath);
      DeviceHandler.Notification.DEVICE_FAIL.hide(event.devicePath);
      DeviceHandler.Notification.DEVICE_EXTERNAL_STORAGE_DISABLED.hide(
          event.devicePath);
      delete this.mountStatus_[event.devicePath];
      break;
    case 'hard_unplugged':
      if (!this.isStartup_) {
        DeviceHandler.Notification.DEVICE_HARD_UNPLUGGED.show(
            event.devicePath);
      }
      break;
    case 'format_start':
      DeviceHandler.Notification.FORMAT_START.show(event.devicePath);
      break;
    case 'format_success':
      DeviceHandler.Notification.FORMAT_START.hide(event.devicePath);
      DeviceHandler.Notification.FORMAT_SUCCESS.show(event.devicePath);
      break;
    case 'format_fail':
      DeviceHandler.Notification.FORMAT_START.hide(event.devicePath);
      DeviceHandler.Notification.FORMAT_FAIL.show(event.devicePath);
      break;
  }
};

/**
 * Mount status for the device.
 * Each multi-partition devices can obtain multiple mount completed events.
 * This status shows what results are already obtained for the device.
 * @enum {string}
 * @const
 */
DeviceHandler.MountStatus = Object.freeze({
  // There is no mount results on the device.
  NO_RESULT: 'noResult',
  // There is no error on the device.
  SUCCESS: 'success',
  // There is only parent errors, that can be overridden by child results.
  ONLY_PARENT_ERROR: 'onlyParentError',
  // There is one child error.
  CHILD_ERROR: 'childError',
  // There is multiple child results and at least one is failure.
  MULTIPART_ERROR: 'multipartError'
});

/**
 * Handles mount completed events to show notifications for removable devices.
 * @param {MountCompletedEvent} event Mount completed event.
 * @private
 */
DeviceHandler.prototype.onMountCompleted_ = function(event) {
  // If this is remounting, which happens when resuming ChromeOS, the device has
  // already inserted to the computer. So we suppress the notification.
  var volume = event.volumeMetadata;
  if (!volume.deviceType || event.isRemounting)
    return;

  // If the current volume status is succeed and it should be handled in
  // Files.app, show the notification to navigate the volume.
  if (event.status === 'success' && event.shouldNotify) {
    if (this.navigationVolumes_[event.volumeMetadata.devicePath]) {
      // The notification has already shown for the device. It seems the device
      // has multiple volumes. The order of mount events of volumes are
      // undetermind, so it compares the volume Id and uses the earier order ID
      // to prevent Files.app from navigating to different volumes for each
      // time.
      if (event.volumeMetadata.volumeId <
          this.navigationVolumes_[event.volumeMetadata.devicePath]) {
        this.navigationVolumes_[event.volumeMetadata.devicePath] =
            event.volumeMetadata.volumeId;
      }
    } else {
      if (!this.isStartup_) {
        this.navigationVolumes_[event.volumeMetadata.devicePath] =
            event.volumeMetadata.volumeId;
        DeviceHandler.Notification.DEVICE_NAVIGATION.show(
            event.volumeMetadata.devicePath);
      }
    }
  }

  if (event.eventType === 'unmount') {
    this.navigationVolumes_[volume.devicePath] = null;
    DeviceHandler.Notification.DEVICE_NAVIGATION.hide(volume.devicePath);
  }

  var getFirstStatus = function(event) {
    if (event.status === 'success')
      return DeviceHandler.MountStatus.SUCCESS;
    else if (event.volumeMetadata.isParentDevice)
      return DeviceHandler.MountStatus.ONLY_PARENT_ERROR;
    else
      return DeviceHandler.MountStatus.CHILD_ERROR;
  };

  // Update the current status.
  switch (this.mountStatus_[volume.devicePath]) {
    // If there is no related device, do nothing.
    case undefined:
      return;
    // If the multipart error message has already shown, do nothing because the
    // message does not changed by the following mount results.
    case DeviceHandler.MountStatus.MULTIPART_ERROR:
      return;
    // If this is the first result, hide the scanning notification.
    case DeviceHandler.MountStatus.NO_RESULT:
      DeviceHandler.Notification.DEVICE.hide(volume.devicePath);
      this.mountStatus_[volume.devicePath] = getFirstStatus(event);
      break;
    // If there are only parent errors, and the new result is child's one, hide
    // the parent error. (parent device contains partition table, which is
    // unmountable)
    case DeviceHandler.MountStatus.ONLY_PARENT_ERROR:
      if (!volume.isParentDevice)
        DeviceHandler.Notification.DEVICE_FAIL.hide(volume.devicePath);
      this.mountStatus_[volume.devicePath] = getFirstStatus(event);
      break;
    // We have a multi-partition device for which at least one mount
    // failed.
    case DeviceHandler.MountStatus.SUCCESS:
    case DeviceHandler.MountStatus.CHILD_ERROR:
      if (this.mountStatus_[volume.devicePath] ===
              DeviceHandler.MountStatus.SUCCESS &&
          event.status === 'success') {
        this.mountStatus_[volume.devicePath] =
            DeviceHandler.MountStatus.SUCCESS;
      } else {
        this.mountStatus_[volume.devicePath] =
            DeviceHandler.MountStatus.MULTIPART_ERROR;
      }
      break;
  }

  if (event.eventType === 'unmount')
    return;

  // Show the notification for the current errors.
  // If there is no error, do not show/update the notification.
  var message;
  switch (this.mountStatus_[volume.devicePath]) {
    case DeviceHandler.MountStatus.MULTIPART_ERROR:
      message = volume.deviceLabel ?
          strf('MULTIPART_DEVICE_UNSUPPORTED_MESSAGE', volume.deviceLabel) :
          str('MULTIPART_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE');
      break;
    case DeviceHandler.MountStatus.CHILD_ERROR:
    case DeviceHandler.MountStatus.ONLY_PARENT_ERROR:
      if (event.status === 'error_unsupported_filesystem') {
        message = volume.deviceLabel ?
            strf('DEVICE_UNSUPPORTED_MESSAGE', volume.deviceLabel) :
            str('DEVICE_UNSUPPORTED_DEFAULT_MESSAGE');
      } else {
        message = volume.deviceLabel ?
            strf('DEVICE_UNKNOWN_MESSAGE', volume.deviceLabel) :
            str('DEVICE_UNKNOWN_DEFAULT_MESSAGE');
      }
      break;
  }
  if (message) {
    DeviceHandler.Notification.DEVICE_FAIL.hide(volume.devicePath);
    DeviceHandler.Notification.DEVICE_FAIL.show(volume.devicePath, message);
  }
};

/**
 * Handles notification button click.
 * @param {string} id ID of the notification.
 * @private
 */
DeviceHandler.prototype.onNotificationButtonClicked_ = function(id) {
  var match = /^deviceNavigation:(.*)$/.exec(id);
  if (match) {
    chrome.notifications.clear(id, function() {});
    var event = new Event(DeviceHandler.VOLUME_NAVIGATION_REQUESTED);
    event.volumeId = this.navigationVolumes_[match[1]];
    this.dispatchEvent(event);
  }
};

DeviceHandler.prototype.onStartup_ = function() {
  this.isStartup_ = true;
  setTimeout(function() {
    this.isStartup_ = false;
  }.bind(this), 5000);
};
