// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Thin wrapper for VolumeManager. This should be an interface proxy to talk
 * to VolumeManager. This class also filters Drive related data/events if
 * driveEnabled is set to false.
 *
 * @param {VolumeManagerWrapper.DriveEnabledStatus} driveEnabled DRIVE_ENABLED
 *     if drive should be available. DRIVE_DISABLED if drive related
 *     data/events should be hidden.
 * @param {DOMWindow=} opt_backgroundPage Window object of the background
 *     page. If this is specified, the class skips to get background page.
 *     TOOD(hirono): Let all clients of the class pass the background page and
 *     make the argument not optional.
 * @constructor
 * @extends {cr.EventTarget}
 */
function VolumeManagerWrapper(driveEnabled, opt_backgroundPage) {
  cr.EventTarget.call(this);

  this.driveEnabled_ = driveEnabled;
  this.volumeInfoList = new cr.ui.ArrayDataModel([]);

  this.volumeManager_ = null;
  this.pendingTasks_ = [];
  this.onEventBound_ = this.onEvent_.bind(this);
  this.onVolumeInfoListUpdatedBound_ =
      this.onVolumeInfoListUpdated_.bind(this);

  this.disposed_ = false;

  // Start initialize the VolumeManager.
  var queue = new AsyncUtil.Queue();

  if (opt_backgroundPage) {
    this.backgroundPage_ = opt_backgroundPage;
  } else {
    queue.run(function(callNextStep) {
      chrome.runtime.getBackgroundPage(function(backgroundPage) {
        this.backgroundPage_ = backgroundPage;
        callNextStep();
      }.bind(this));
    }.bind(this));
  }

  queue.run(function(callNextStep) {
    this.backgroundPage_.VolumeManager.getInstance(function(volumeManager) {
      this.onReady_(volumeManager);
      callNextStep();
    }.bind(this));
  }.bind(this));
}

/**
 * If the drive is enabled on the wrapper.
 * @enum {boolean}
 */
VolumeManagerWrapper.DriveEnabledStatus = {
  DRIVE_ENABLED: true,
  DRIVE_DISABLED: false
};

/**
 * Extends cr.EventTarget.
 */
VolumeManagerWrapper.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Called when the VolumeManager gets ready for post initialization.
 * @param {VolumeManager} volumeManager The initialized VolumeManager instance.
 * @private
 */
VolumeManagerWrapper.prototype.onReady_ = function(volumeManager) {
  if (this.disposed_)
    return;

  this.volumeManager_ = volumeManager;

  // Subscribe to VolumeManager.
  this.volumeManager_.addEventListener(
      'drive-connection-changed', this.onEventBound_);
  this.volumeManager_.addEventListener(
      'externally-unmounted', this.onEventBound_);

  // Cache volumeInfoList.
  var volumeInfoList = [];
  for (var i = 0; i < this.volumeManager_.volumeInfoList.length; i++) {
    var volumeInfo = this.volumeManager_.volumeInfoList.item(i);
    // TODO(hidehiko): Filter mounted volumes located on Drive File System.
    if (!this.driveEnabled_ && volumeInfo.volumeType ===
        VolumeManagerCommon.VolumeType.DRIVE)
      continue;
    volumeInfoList.push(volumeInfo);
  }
  this.volumeInfoList.splice.apply(
      this.volumeInfoList,
      [0, this.volumeInfoList.length].concat(volumeInfoList));

  // Subscribe to VolumeInfoList.
  // In VolumeInfoList, we only use 'splice' event.
  this.volumeManager_.volumeInfoList.addEventListener(
      'splice', this.onVolumeInfoListUpdatedBound_);

  // Run pending tasks.
  var pendingTasks = this.pendingTasks_;
  this.pendingTasks_ = null;
  for (var i = 0; i < pendingTasks.length; i++)
    pendingTasks[i]();
};

/**
 * Disposes the instance. After the invocation of this method, any other
 * method should not be called.
 */
VolumeManagerWrapper.prototype.dispose = function() {
  this.disposed_ = true;

  if (!this.volumeManager_)
    return;
  this.volumeManager_.removeEventListener(
      'drive-connection-changed', this.onEventBound_);
  this.volumeManager_.removeEventListener(
      'externally-unmounted', this.onEventBound_);
  this.volumeManager_.volumeInfoList.removeEventListener(
      'splice', this.onVolumeInfoListUpdatedBound_);
};

/**
 * Called on events sent from VolumeManager. This has responsibility to
 * re-dispatch the event to the listeners.
 * @param {Event} event Event object sent from VolumeManager.
 * @private
 */
VolumeManagerWrapper.prototype.onEvent_ = function(event) {
  if (!this.driveEnabled_) {
    // If the drive is disabled, ignore all drive related events.
    if (event.type === 'drive-connection-changed' ||
        (event.type === 'externally-unmounted' &&
         event.volumeInfo.volumeType ===
             VolumeManagerCommon.VolumeType.DRIVE)) {
      return;
    }
  }

  this.dispatchEvent(event);
};

/**
 * Called on events of modifying VolumeInfoList.
 * @param {Event} event Event object sent from VolumeInfoList.
 * @private
 */
VolumeManagerWrapper.prototype.onVolumeInfoListUpdated_ = function(event) {
  if (this.driveEnabled_) {
    // Apply the splice as is.
    this.volumeInfoList.splice.apply(
        this.volumeInfoList,
        [event.index, event.removed.length].concat(event.added));
  } else {
    // Filters drive related volumes.
    var index = event.index;
    for (var i = 0; i < event.index; i++) {
      if (this.volumeManager_.volumeInfoList.item(i).volumeType ===
          VolumeManagerCommon.VolumeType.DRIVE)
        index--;
    }

    var numRemovedVolumes = 0;
    for (var i = 0; i < event.removed.length; i++) {
      if (event.removed[i].volumeType !== VolumeManagerCommon.VolumeType.DRIVE)
        numRemovedVolumes++;
    }

    var addedVolumes = [];
    for (var i = 0; i < event.added.length; i++) {
      var volumeInfo = event.added[i];
      if (volumeInfo.volumeType !== VolumeManagerCommon.VolumeType.DRIVE)
        addedVolumes.push(volumeInfo);
    }

    this.volumeInfoList.splice.apply(
        this.volumeInfoList,
        [index, numRemovedVolumes].concat(addedVolumes));
  }
};

/**
 * Ensures the VolumeManager is initialized, and then invokes callback.
 * If the VolumeManager is already initialized, callback will be called
 * immediately.
 * @param {function()} callback Called on initialization completion.
 */
VolumeManagerWrapper.prototype.ensureInitialized = function(callback) {
  if (this.pendingTasks_) {
    this.pendingTasks_.push(this.ensureInitialized.bind(this, callback));
    return;
  }

  callback();
};

/**
 * @return {VolumeManagerCommon.DriveConnectionType} Current drive connection
 *     state.
 */
VolumeManagerWrapper.prototype.getDriveConnectionState = function() {
  if (!this.driveEnabled_ || !this.volumeManager_) {
    return {
      type: VolumeManagerCommon.DriveConnectionType.OFFLINE,
      reason: VolumeManagerCommon.DriveConnectionReason.NO_SERVICE
    };
  }

  return this.volumeManager_.getDriveConnectionState();
};

/**
 * Obtains a volume info containing the passed entry.
 * @param {Entry} entry Entry on the volume to be returned.
 * @return {VolumeInfo} The VolumeInfo instance or null if not found.
 */
VolumeManagerWrapper.prototype.getVolumeInfo = function(entry) {
  return this.filterDisabledDriveVolume_(
      this.volumeManager_ && this.volumeManager_.getVolumeInfo(entry));
};

/**
 * Obtains a volume information of the current profile.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {VolumeInfo} Found volume info.
 */
VolumeManagerWrapper.prototype.getCurrentProfileVolumeInfo =
    function(volumeType) {
  return this.filterDisabledDriveVolume_(
      this.volumeManager_ &&
      this.volumeManager_.getCurrentProfileVolumeInfo(volumeType));
};

/**
 * Obtains the default display root entry.
 * @param {function(Entry)} callback Callback passed the default display root.
 */
VolumeManagerWrapper.prototype.getDefaultDisplayRoot =
    function(callback) {
  this.ensureInitialized(function() {
    var defaultVolume = this.getCurrentProfileVolumeInfo(
        VolumeManagerCommon.VolumeType.DOWNLOADS);
    defaultVolume.resolveDisplayRoot(callback, function() {
      // defaultVolume is DOWNLOADS and resolveDisplayRoot should succeed.
      throw new Error(
          'Unexpectedly failed to obtain the default display root.');
    });
  }.bind(this));
};

/**
 * Obtains location information from an entry.
 *
 * @param {Entry} entry File or directory entry.
 * @return {EntryLocation} Location information.
 */
VolumeManagerWrapper.prototype.getLocationInfo = function(entry) {
  var locationInfo =
      this.volumeManager_ && this.volumeManager_.getLocationInfo(entry);
  if (!locationInfo)
    return null;
  if (!this.filterDisabledDriveVolume_(locationInfo.volumeInfo))
    return null;
  return locationInfo;
};

/**
 * Requests to mount the archive file.
 * @param {string} fileUrl The path to the archive file to be mounted.
 * @param {function(VolumeInfo)} successCallback Called with the VolumeInfo
 *     instance.
 * @param {function(VolumeManagerCommon.VolumeError)} errorCallback Called when
 *     an error occurs.
 */
VolumeManagerWrapper.prototype.mountArchive = function(
    fileUrl, successCallback, errorCallback) {
  if (this.pendingTasks_) {
    this.pendingTasks_.push(
        this.mountArchive.bind(this, fileUrl, successCallback, errorCallback));
    return;
  }

  this.volumeManager_.mountArchive(fileUrl, successCallback, errorCallback);
};

/**
 * Requests unmount the specified volume.
 * @param {!VolumeInfo} volumeInfo Volume to be unmounted.
 * @param {function()} successCallback Called on success.
 * @param {function(VolumeManagerCommon.VolumeError)} errorCallback Called when
 *     an error occurs.
 */
VolumeManagerWrapper.prototype.unmount = function(
    volumeInfo, successCallback, errorCallback) {
  if (this.pendingTasks_) {
    this.pendingTasks_.push(
        this.unmount.bind(this, volumeInfo, successCallback, errorCallback));
    return;
  }

  this.volumeManager_.unmount(volumeInfo, successCallback, errorCallback);
};

/**
 * Filters volume info by referring driveEnabled.
 *
 * @param {VolumeInfo} volumeInfo Volume info.
 * @return {VolumeInfo} Null if the drive is disabled and the given volume is
 *     drive. Otherwise just returns the volume.
 * @private
 */
VolumeManagerWrapper.prototype.filterDisabledDriveVolume_ =
    function(volumeInfo) {
  var isDrive = volumeInfo && volumeInfo.volumeType ===
      VolumeManagerCommon.VolumeType.DRIVE;
  return this.driveEnabled_ || !isDrive ? volumeInfo : null;
};
