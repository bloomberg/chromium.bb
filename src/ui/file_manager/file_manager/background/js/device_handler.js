// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Handler of device event. */
class DeviceHandler extends cr.EventTarget {
  constructor() {
    super();

    /**
     * Map of device path and mount status of devices.
     * @private {Object<DeviceHandler.MountStatus>}
     */
    this.mountStatus_ = {};

    chrome.fileManagerPrivate.onDeviceChanged.addListener(
        this.onDeviceChanged_.bind(this));
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));
    chrome.notifications.onClicked.addListener(
        this.onNotificationClicked_.bind(this));
    chrome.notifications.onButtonClicked.addListener(
        this.onNotificationButtonClicked_.bind(this));
  }

  /**
   * Handles notifications from C++ sides.
   * @param {chrome.fileManagerPrivate.DeviceEvent} event Device event.
   * @private
   */
  onDeviceChanged_(event) {
    util.doIfPrimaryContext(() => {
      this.onDeviceChangedInternal_(event);
    });
  }

  /**
   * @param {chrome.fileManagerPrivate.DeviceEvent} event Device event.
   * @private
   */
  onDeviceChangedInternal_(event) {
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
        DeviceHandler.Notification.DEVICE_HARD_UNPLUGGED.show(event.devicePath);
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
      case 'rename_fail':
        DeviceHandler.Notification.RENAME_FAIL.show(event.devicePath);
        break;
      default:
        console.error('Unknown event type: ' + event.type);
        break;
    }
  }

  /**
   * Handles mount completed events to show notifications for removable devices.
   * @param {chrome.fileManagerPrivate.MountCompletedEvent} event Mount
   *     completed event.
   * @private
   */
  onMountCompleted_(event) {
    util.doIfPrimaryContext(() => {
      this.onMountCompletedInternal_(event);
    });
  }

  onMountCompletedInternal_(event) {
    const volume = event.volumeMetadata;

    if (event.status === 'success' && event.shouldNotify) {
      if (event.eventType === 'mount') {
        this.onMount_(event);
      } else if (event.eventType === 'unmount') {
        this.onUnmount_(event);
      }
    }

    if (!volume.deviceType || !volume.devicePath || !event.shouldNotify) {
      return;
    }

    const getFirstStatus = event => {
      if (event.status === 'success') {
        return DeviceHandler.MountStatus.SUCCESS;
      } else if (event.volumeMetadata.isParentDevice) {
        return DeviceHandler.MountStatus.ONLY_PARENT_ERROR;
      } else {
        return DeviceHandler.MountStatus.CHILD_ERROR;
      }
    };

    // Update the current status.
    if (!this.mountStatus_[volume.devicePath]) {
      this.mountStatus_[volume.devicePath] =
          DeviceHandler.MountStatus.NO_RESULT;
    }
    switch (this.mountStatus_[volume.devicePath]) {
      // If the multipart error message has already shown, do nothing because
      // the message does not changed by the following mount results.
      case DeviceHandler.MountStatus.MULTIPART_ERROR:
        return;
      // If this is the first result, hide the scanning notification.
      case DeviceHandler.MountStatus.NO_RESULT:
        this.mountStatus_[volume.devicePath] = getFirstStatus(event);
        break;
      // If there are only parent errors, and the new result is child's one,
      // hide the parent error. (parent device contains partition table, which
      // is unmountable)
      case DeviceHandler.MountStatus.ONLY_PARENT_ERROR:
        if (!volume.isParentDevice) {
          DeviceHandler.Notification.DEVICE_FAIL.hide(
              /** @type {string} */ (volume.devicePath));
        }
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

    if (event.eventType === 'unmount') {
      return;
    }

    // Show the notification for the current errors.
    // If there is no error, do not show/update the notification.
    let message;
    switch (this.mountStatus_[volume.devicePath]) {
      case DeviceHandler.MountStatus.MULTIPART_ERROR:
        message = volume.deviceLabel ?
            strf('MULTIPART_DEVICE_UNSUPPORTED_MESSAGE', volume.deviceLabel) :
            str('MULTIPART_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE');
        DeviceHandler.Notification.DEVICE_FAIL.show(
            /** @type {string} */ (volume.devicePath), message);
        break;
      case DeviceHandler.MountStatus.CHILD_ERROR:
      case DeviceHandler.MountStatus.ONLY_PARENT_ERROR:
        if (event.status === 'error_unsupported_filesystem') {
          message = volume.deviceLabel ?
              strf('DEVICE_UNSUPPORTED_MESSAGE', volume.deviceLabel) :
              str('DEVICE_UNSUPPORTED_DEFAULT_MESSAGE');
          DeviceHandler.Notification.DEVICE_FAIL.show(
              /** @type {string} */ (volume.devicePath), message);
        } else {
          message = volume.deviceLabel ?
              strf('DEVICE_UNKNOWN_MESSAGE', volume.deviceLabel) :
              str('DEVICE_UNKNOWN_DEFAULT_MESSAGE');
          if (event.volumeMetadata.isReadOnly) {
            DeviceHandler.Notification.DEVICE_FAIL_UNKNOWN_READONLY.show(
                /** @type {string} */ (volume.devicePath), message);
          } else {
            DeviceHandler.Notification.DEVICE_FAIL_UNKNOWN.show(
                /** @type {string} */ (volume.devicePath), message);
          }
        }
    }
  }

  /**
   * Handles mount events.
   * @param {chrome.fileManagerPrivate.MountCompletedEvent} event
   * @private
   */
  onMount_(event) {
    // If this is remounting, which happens when resuming Chrome OS, the device
    // has already inserted to the computer. So we suppress the notification.
    const metadata = event.volumeMetadata;
    volumeManagerFactory.getInstance()
        .then(
            /**
             * @param {!VolumeManager} volumeManager
             * @return {!Promise<!VolumeInfo>}
             */
            (volumeManager) => {
              if (!metadata.volumeId) {
                return Promise.reject('No volume id associated with event.');
              }
              return volumeManager.whenVolumeInfoReady(metadata.volumeId);
            })
        .then(
            /**
             * @param {!VolumeInfo} volumeInfo
             * @return {!Promise<!DirectoryEntry>} The root directory
             *     of the volume.
             */
            volumeInfo => {
              if (importer.isEligibleVolume(volumeInfo)) {
                return volumeInfo.resolveDisplayRoot();
              }
              return Promise.reject('Cloud import disabled.');
            })
        .then(
            /**
             * @param {!DirectoryEntry} root
             * @return {!Promise<!DirectoryEntry>}
             */
            root => {
              return importer.getMediaDirectory(root);
            })
        .then(/**
               * @param {!DirectoryEntry} directory
               */
              directory => {
                return importer.isPhotosAppImportEnabled().then(
                    /**
                     * @param {boolean} appEnabled
                     */
                    appEnabled => {
                      // We don't want to auto-open two windows when a user
                      // inserts a removable device.  Only open Files app if
                      // auto-import is disabled in Photos app.
                      if (!appEnabled) {
                        this.openMediaDirectory_(
                            metadata.volumeId, null, directory.fullPath);
                      }
                    });
              })
        .catch(error => {
          if (metadata.deviceType && metadata.devicePath) {
            if (metadata.isReadOnly && !metadata.isReadOnlyRemovableDevice) {
              DeviceHandler.Notification.DEVICE_NAVIGATION_READONLY_POLICY.show(
                  /** @type {string} */ (metadata.devicePath));
            } else {
              chrome.fileManagerPrivate.getPreferences(pref => {
                if (!pref.arcEnabled || !util.isArcUsbStorageUIEnabled()) {
                  DeviceHandler.Notification.DEVICE_NAVIGATION.show(
                      /** @type {string} */ (metadata.devicePath));
                } else if (pref.arcRemovableMediaAccessEnabled) {
                  DeviceHandler.Notification.DEVICE_NAVIGATION_APPS_HAVE_ACCESS
                      .show(
                          /** @type {string} */ (metadata.devicePath));
                } else {
                  DeviceHandler.Notification.DEVICE_NAVIGATION_ALLOW_APP_ACCESS
                      .show(
                          /** @type {string} */ (metadata.devicePath));
                }
              });
            }
          }
        });
  }

  onUnmount_(event) {
    DeviceHandler.Notification.DEVICE_NAVIGATION.hide(
        /** @type {string} */ (event.devicePath));
  }

  /**
   * Handles notification body click.
   * @param {string} id ID of the notification.
   * @private
   */
  onNotificationClicked_(id) {
    util.doIfPrimaryContext(() => {
      this.onNotificationClickedInternal_(id, -1 /* index */);
    });
  }

  /**
   * Handles notification button click.
   * @param {string} id ID of the notification.
   * @param {number} index index of the button.
   * @private
   */
  onNotificationButtonClicked_(id, index) {
    util.doIfPrimaryContext(() => {
      this.onNotificationClickedInternal_(id, index);
    });
  }

  /**
   * @param {string} id ID of the notification.
   * @param {number} index index of the button.
   * @private
   */
  onNotificationClickedInternal_(id, index) {
    const pos = id.indexOf(':');
    const type = id.substr(0, pos);
    const devicePath = id.substr(pos + 1);
    if (type === 'deviceNavigation' || type === 'deviceFail') {
      chrome.notifications.clear(id, () => {});
      this.openMediaDirectory_(null, devicePath, null);
      return;
    }
    if (type === 'deviceImport') {
      chrome.notifications.clear(id, () => {});
      this.openMediaDirectory_(null, devicePath, 'DCIM');
      return;
    }
    if (type !== 'deviceNavigationAppAccess') {
      return;
    }
    chrome.notifications.clear(id, () => {});
    const secondButtonIndex = 1;
    if (index === secondButtonIndex) {
      chrome.fileManagerPrivate.openSettingsSubpage(
          'storage/externalStoragePreferences');
      return;
    }
    this.openMediaDirectory_(null, devicePath, null);
  }

  /**
   * Opens a directory on removable media.
   * @param {?string} volumeId
   * @param {?string} devicePath
   * @param {?string} filePath
   * @private
   */
  openMediaDirectory_(volumeId, devicePath, filePath) {
    const event = new Event(DeviceHandler.VOLUME_NAVIGATION_REQUESTED);
    event.volumeId = volumeId;
    event.devicePath = devicePath;
    event.filePath = filePath;
    this.dispatchEvent(event);
  }
}


/**
 * Mount status for the device.
 * Each multi-partition devices can obtain multiple mount completed events.
 * This status shows what results are already obtained for the device.
 * @enum {string}
 * @const
 */
DeviceHandler.MountStatus = {
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
};
Object.freeze(DeviceHandler.MountStatus);

/**
 * An event name trigerred when a user requests to navigate to a volume.
 * The event object must have a volumeId property.
 * @type {string}
 * @const
 */
DeviceHandler.VOLUME_NAVIGATION_REQUESTED = 'volumenavigationrequested';

/**
 * Notification type.
 */
DeviceHandler.Notification = class {
  /**
   * @param {string} prefix Prefix of notification ID.
   * @param {string} title String ID of title.
   * @param {string} message String ID of message.
   * @param {string=} opt_buttonLabel String ID of the button label.
   * @param {boolean=} opt_isClickable True if the notification body is
   *     clickable.
   * @param {string=} opt_additionalMessage String ID of additional message.
   * @param {string=} opt_secondButtonLabel String ID of the second button
   *     label.
   */
  constructor(
      prefix, title, message, opt_buttonLabel, opt_isClickable,
      opt_additionalMessage, opt_secondButtonLabel) {
    // Check that second button is used with primary button, because
    // notifications API is based in button index, so the second button index
    // is always 1.
    if (opt_secondButtonLabel) {
      console.assert(opt_buttonLabel !== undefined);
    }

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
     * True if the notification body is clickable.
     * @type {boolean}
     */
    this.isClickable = opt_isClickable || false;

    /**
     * String ID of additional message.
     * @type {?string}
     */
    this.additionalMessage = opt_additionalMessage || null;

    /**
     * String ID of second button label.
     * @type {?string}
     */
    this.secondButtonLabel = opt_secondButtonLabel || null;

    /**
     * Queue of API call.
     * @type {AsyncUtil.Queue}
     * @private
     */
    this.queue_ = new AsyncUtil.Queue();
  }

  /**
   * Shows the notification for the device path.
   * @param {string} devicePath Device path.
   * @param {string=} opt_message Message overrides the default message.
   * @return {string} Notification ID.
   */
  show(devicePath, opt_message) {
    const notificationId = this.makeId_(devicePath);
    this.queue_.run(callback => {
      this.showInternal_(notificationId, opt_message || null, callback);
    });
    return notificationId;
  }

  /**
   * Shows the notification for the device path.
   * If the existing notification has been already shown, it does not anything.
   * @param {string} devicePath Device path.
   */
  showOnce(devicePath) {
    const notificationId = this.makeId_(devicePath);
    this.queue_.run(function(callback) {
      chrome.notifications.getAll(idList => {
        if (idList.indexOf(notificationId) !== -1) {
          callback();
          return;
        }
        this.showInternal_(notificationId, null, callback);
      });
    });
  }

  /**
   * Shows the notificaiton without using AsyncQueue.
   * @param {string} notificationId Notification ID.
   * @param {?string} message Message overriding the normal message.
   * @param {function()} callback Callback to be invoked when the notification
   *     is created.
   * @private
   */
  showInternal_(notificationId, message, callback) {
    const buttons =
        this.buttonLabel ? [{title: str(this.buttonLabel)}] : undefined;
    if (this.secondButtonLabel) {
      buttons.push({title: str(this.secondButtonLabel)});
    }
    const additionalMessage =
        this.additionalMessage ? (' ' + str(this.additionalMessage)) : '';
    chrome.notifications.create(
        notificationId, {
          type: 'basic',
          title: str(this.title),
          message: message || (str(this.message) + additionalMessage),
          iconUrl: chrome.runtime.getURL('/common/images/icon96.png'),
          buttons: buttons,
          isClickable: this.isClickable
        },
        callback);
  }

  /**
   * Hides the notification for the device path.
   * @param {string} devicePath Device path.
   */
  hide(devicePath) {
    this.queue_.run(callback => {
      chrome.notifications.clear(this.makeId_(devicePath), callback);
    });
  }

  /**
   * Makes a notification ID for the device path.
   * @param {string} devicePath Device path.
   * @return {string} Notification ID.
   * @private
   */
  makeId_(devicePath) {
    return this.prefix + ':' + devicePath;
  }
};

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_NAVIGATION_ALLOW_APP_ACCESS =
    new DeviceHandler.Notification(
        'deviceNavigationAppAccess', 'REMOVABLE_DEVICE_DETECTION_TITLE',
        'REMOVABLE_DEVICE_NAVIGATION_MESSAGE',
        'REMOVABLE_DEVICE_NAVIGATION_BUTTON_LABEL', true,
        'REMOVABLE_DEVICE_ALLOW_PLAY_STORE_ACCESS_MESSAGE',
        'REMOVABLE_DEVICE_OPEN_SETTTINGS_BUTTON_LABEL');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_NAVIGATION_APPS_HAVE_ACCESS =
    new DeviceHandler.Notification(
        'deviceNavigationAppAccess', 'REMOVABLE_DEVICE_DETECTION_TITLE',
        'REMOVABLE_DEVICE_NAVIGATION_MESSAGE',
        'REMOVABLE_DEVICE_NAVIGATION_BUTTON_LABEL', true,
        'REMOVABLE_DEVICE_PLAY_STORE_APPS_HAVE_ACCESS_MESSAGE',
        'REMOVABLE_DEVICE_OPEN_SETTTINGS_BUTTON_LABEL');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_NAVIGATION = new DeviceHandler.Notification(
    'deviceNavigation', 'REMOVABLE_DEVICE_DETECTION_TITLE',
    'REMOVABLE_DEVICE_NAVIGATION_MESSAGE',
    'REMOVABLE_DEVICE_NAVIGATION_BUTTON_LABEL', true);

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_NAVIGATION_READONLY_POLICY =
    new DeviceHandler.Notification(
        'deviceNavigation', 'REMOVABLE_DEVICE_DETECTION_TITLE',
        'REMOVABLE_DEVICE_NAVIGATION_MESSAGE_READONLY_POLICY',
        'REMOVABLE_DEVICE_NAVIGATION_BUTTON_LABEL', true);

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_IMPORT = new DeviceHandler.Notification(
    'deviceImport', 'REMOVABLE_DEVICE_DETECTION_TITLE',
    'REMOVABLE_DEVICE_IMPORT_MESSAGE', 'REMOVABLE_DEVICE_IMPORT_BUTTON_LABEL',
    true);

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_FAIL = new DeviceHandler.Notification(
    'deviceFail', 'REMOVABLE_DEVICE_DETECTION_TITLE',
    'DEVICE_UNSUPPORTED_DEFAULT_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_FAIL_UNKNOWN = new DeviceHandler.Notification(
    'deviceFail', 'REMOVABLE_DEVICE_DETECTION_TITLE',
    'DEVICE_UNKNOWN_DEFAULT_MESSAGE', 'DEVICE_UNKNOWN_BUTTON_LABEL');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_FAIL_UNKNOWN_READONLY =
    new DeviceHandler.Notification(
        'deviceFail', 'REMOVABLE_DEVICE_DETECTION_TITLE',
        'DEVICE_UNKNOWN_DEFAULT_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_EXTERNAL_STORAGE_DISABLED =
    new DeviceHandler.Notification(
        'deviceFail', 'REMOVABLE_DEVICE_DETECTION_TITLE',
        'EXTERNAL_STORAGE_DISABLED_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.DEVICE_HARD_UNPLUGGED =
    new DeviceHandler.Notification(
        'hardUnplugged', 'DEVICE_HARD_UNPLUGGED_TITLE',
        'DEVICE_HARD_UNPLUGGED_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.FORMAT_START = new DeviceHandler.Notification(
    'formatStart', 'FORMATTING_OF_DEVICE_PENDING_TITLE',
    'FORMATTING_OF_DEVICE_PENDING_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.FORMAT_SUCCESS = new DeviceHandler.Notification(
    'formatSuccess', 'FORMATTING_OF_DEVICE_FINISHED_TITLE',
    'FORMATTING_FINISHED_SUCCESS_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.FORMAT_FAIL = new DeviceHandler.Notification(
    'formatFail', 'FORMATTING_OF_DEVICE_FAILED_TITLE',
    'FORMATTING_FINISHED_FAILURE_MESSAGE');

/**
 * @type {DeviceHandler.Notification}
 * @const
 */
DeviceHandler.Notification.RENAME_FAIL = new DeviceHandler.Notification(
    'renameFail', 'RENAMING_OF_DEVICE_FAILED_TITLE',
    'RENAMING_OF_DEVICE_FINISHED_FAILURE_MESSAGE');
