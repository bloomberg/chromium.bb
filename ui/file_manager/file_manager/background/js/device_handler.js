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

  chrome.fileManagerPrivate.onDeviceChanged.addListener(
      this.onDeviceChanged_.bind(this));
  chrome.fileManagerPrivate.onMountCompleted.addListener(
      this.onMountCompleted_.bind(this));
  chrome.notifications.onButtonClicked.addListener(
      this.onNotificationButtonClicked_.bind(this));
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

  Object.seal(this);
};

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
DeviceHandler.Notification.DEVICE_FAIL_UNKNOWN = new DeviceHandler.Notification(
    'deviceFail',
    'REMOVABLE_DEVICE_DETECTION_TITLE',
    'DEVICE_UNKNOWN_DEFAULT_MESSAGE',
    'DEVICE_UNKNOWN_BUTTON_LABEL');

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
  var notificationId = this.makeId_(devicePath);
  this.queue_.run(function(callback) {
    this.showInternal_(notificationId, opt_message || null, callback);
  }.bind(this));
  return notificationId;
};

/**
 * Shows the notification for the device path.
 * If the existing notification has been already shown, it does not anything.
 * @param {string} devicePath Device path.
 */
DeviceHandler.Notification.prototype.showOnce = function(devicePath) {
  var notificationId = this.makeId_(devicePath);
  this.queue_.run(function(callback) {
    chrome.notifications.getAll(function(idList) {
      if (idList.indexOf(notificationId) !== -1) {
        callback();
        return;
      }
      this.showInternal_(notificationId, null, callback);
    }.bind(this));
  });
};

/**
 * Shows the notificaiton without using AsyncQueue.
 * @param {string} notificaitonId Notification ID.
 * @param {?message} message Message overriding the normal message.
 * @param {function()} callback Callback to be invoked when the notification is
 *     created.
 * @private
 */
DeviceHandler.Notification.prototype.showInternal_ = function(
    notificationId, message, callback) {
  var buttons =
      this.buttonLabel ? [{title: str(this.buttonLabel)}] : undefined;
  chrome.notifications.create(
      notificationId,
      {
        type: 'basic',
        title: str(this.title),
        message: message || str(this.message),
        iconUrl: chrome.runtime.getURL('/common/images/icon96.png'),
        buttons: buttons
      },
      callback);
};

/**
 * Hides the notification for the device path.
 * @param {string} devicePath Device path.
 */
DeviceHandler.Notification.prototype.hide = function(devicePath) {
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
 * Handles notifications from C++ sides.
 * @param {DeviceEvent} event Device event.
 * @private
 */
DeviceHandler.prototype.onDeviceChanged_ = function(event) {
  switch (event.type) {
    case 'disabled':
      DeviceHandler.Notification.DEVICE_EXTERNAL_STORAGE_DISABLED.show(
          event.devicePath);
      break;
    case 'removed':
      DeviceHandler.Notification.DEVICE_FAIL.hide(event.devicePath);
      DeviceHandler.Notification.DEVICE_EXTERNAL_STORAGE_DISABLED.hide(
          event.devicePath);
      delete this.mountStatus_[event.devicePath];
      break;
    case 'hard_unplugged':
      DeviceHandler.Notification.DEVICE_HARD_UNPLUGGED.show(
          event.devicePath);
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
    default:
      console.error('Unknown event tyep: ' + event.type);
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
  if (!volume.deviceType || !event.shouldNotify)
    return;

  // If the current volume status is succeed and it should be handled in
  // Files.app, show the notification to navigate the volume.
  if (event.eventType === 'mount' && event.status === 'success') {
    DeviceHandler.Notification.DEVICE_NAVIGATION.show(
        event.volumeMetadata.devicePath);
  } else if (event.eventType === 'unmount') {
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
  if (!this.mountStatus_[volume.devicePath])
    this.mountStatus_[volume.devicePath] = DeviceHandler.MountStatus.NO_RESULT;
  switch (this.mountStatus_[volume.devicePath]) {
    // If the multipart error message has already shown, do nothing because the
    // message does not changed by the following mount results.
    case DeviceHandler.MountStatus.MULTIPART_ERROR:
      return;
    // If this is the first result, hide the scanning notification.
    case DeviceHandler.MountStatus.NO_RESULT:
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
      DeviceHandler.Notification.DEVICE_FAIL.show(
          volume.devicePath,
          message);
      break;
    case DeviceHandler.MountStatus.CHILD_ERROR:
    case DeviceHandler.MountStatus.ONLY_PARENT_ERROR:
      if (event.status === 'error_unsupported_filesystem') {
        message = volume.deviceLabel ?
            strf('DEVICE_UNSUPPORTED_MESSAGE', volume.deviceLabel) :
            str('DEVICE_UNSUPPORTED_DEFAULT_MESSAGE');
        DeviceHandler.Notification.DEVICE_FAIL.show(
            volume.devicePath,
            message);
      } else {
        message = volume.deviceLabel ?
            strf('DEVICE_UNKNOWN_MESSAGE', volume.deviceLabel) :
            str('DEVICE_UNKNOWN_DEFAULT_MESSAGE');
        DeviceHandler.Notification.DEVICE_FAIL_UNKNOWN.show(
            volume.devicePath,
            message);
      }
  }
};

/**
 * Handles notification button click.
 * @param {string} id ID of the notification.
 * @private
 */
DeviceHandler.prototype.onNotificationButtonClicked_ = function(id) {
  var pos = id.indexOf(':');
  var type = id.substr(0, pos);
  var devicePath = id.substr(pos + 1);
  if (type === 'deviceNavigation' || type === 'deviceFail') {
    chrome.notifications.clear(id, function() {});
    var event = new Event(DeviceHandler.VOLUME_NAVIGATION_REQUESTED);
    event.devicePath = devicePath;
    this.dispatchEvent(event);
  }
};
