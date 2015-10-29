// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Represents each volume, such as "drive", "download directory", each "USB
 * flush storage", or "mounted zip archive" etc.
 *
 * @constructor
 * @struct
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType The type of the volume.
 * @param {string} volumeId ID of the volume.
 * @param {FileSystem} fileSystem The file system object for this volume.
 * @param {(string|undefined)} error The error if an error is found.
 * @param {(string|undefined)} deviceType The type of device
 *     ('usb'|'sd'|'optical'|'mobile'|'unknown') (as defined in
 *     chromeos/disks/disk_mount_manager.cc). Can be undefined.
 * @param {(string|undefined)} devicePath Identifier of the device that the
 *     volume belongs to. Can be undefined.
 * @param {boolean} isReadOnly True if the volume is read only.
 * @param {!{displayName:string, isCurrentProfile:boolean}} profile Profile
 *     information.
 * @param {string} label Label of the volume.
 * @param {(string|undefined)} extensionId Id of the extension providing this
 *     volume. Empty for native volumes.
 * @param {boolean} hasMedia When true the volume has been identified
 *     as containing media such as photos or videos.
 * @param {boolean} configurable When true, then the volume can be configured.
 * @param {VolumeManagerCommon.Source} source Source of the volume's data.
 */
function VolumeInfo(
    volumeType,
    volumeId,
    fileSystem,
    error,
    deviceType,
    devicePath,
    isReadOnly,
    profile,
    label,
    extensionId,
    hasMedia,
    configurable,
    watchable,
    source) {
  this.volumeType_ = volumeType;
  this.volumeId_ = volumeId;
  this.fileSystem_ = fileSystem;
  this.label_ = label;
  this.displayRoot_ = null;

  /** @type {Object<!FakeEntry>} */
  this.fakeEntries_ = {};

  /** @type {Promise.<!DirectoryEntry>} */
  this.displayRootPromise_ = null;

  if (volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
    // TODO(mtomasz): Convert fake entries to DirectoryProvider.
    this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_OFFLINE] = {
      isDirectory: true,
      rootType: VolumeManagerCommon.RootType.DRIVE_OFFLINE,
      toURL: function() { return 'fake-entry://drive_offline'; }
    };
    this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME] = {
      isDirectory: true,
      rootType: VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
      toURL: function() { return 'fake-entry://drive_shared_with_me'; }
    };
    this.fakeEntries_[VolumeManagerCommon.RootType.DRIVE_RECENT] = {
      isDirectory: true,
      rootType: VolumeManagerCommon.RootType.DRIVE_RECENT,
      toURL: function() { return 'fake-entry://drive_recent'; }
    };
  }

  // Note: This represents if the mounting of the volume is successfully done
  // or not. (If error is empty string, the mount is successfully done).
  // TODO(hidehiko): Rename to make this more understandable.
  this.error_ = error;
  this.deviceType_ = deviceType;
  this.devicePath_ = devicePath;
  this.isReadOnly_ = isReadOnly;
  this.profile_ = Object.freeze(profile);
  this.extensionId_ = extensionId;
  this.hasMedia_ = hasMedia;
  this.configurable_ = configurable;
  this.watchable_ = watchable;
  this.source_ = source;
}

VolumeInfo.prototype = /** @struct */ {
  /**
   * @return {VolumeManagerCommon.VolumeType} Volume type.
   */
  get volumeType() {
    return this.volumeType_;
  },
  /**
   * @return {string} Volume ID.
   */
  get volumeId() {
    return this.volumeId_;
  },
  /**
   * @return {FileSystem} File system object.
   */
  get fileSystem() {
    return this.fileSystem_;
  },
  /**
   * @return {DirectoryEntry} Display root path. It is null before finishing to
   * resolve the entry.
   */
  get displayRoot() {
    return this.displayRoot_;
  },
  /**
   * @return {Object<!FakeEntry>} Fake entries.
   */
  get fakeEntries() {
    return this.fakeEntries_;
  },
  /**
   * @return {(string|undefined)} Error identifier.
   */
  get error() {
    return this.error_;
  },
  /**
   * @return {(string|undefined)} Device type identifier.
   */
  get deviceType() {
    return this.deviceType_;
  },
  /**
   * @return {(string|undefined)} Device identifier.
   */
  get devicePath() {
    return this.devicePath_;
  },
  /**
   * @return {boolean} Whether read only or not.
   */
  get isReadOnly() {
    return this.isReadOnly_;
  },
  /**
   * @return {!{displayName:string, isCurrentProfile:boolean}} Profile data.
   */
  get profile() {
    return this.profile_;
  },
  /**
   * @return {string} Label for the volume.
   */
  get label() {
    return this.label_;
  },
  /**
   * @return {(string|undefined)} Id of an extennsion providing this volume.
   */
  get extensionId() {
    return this.extensionId_;
  },
  /**
   * @return {boolean} True if the volume contains media.
   */
  get hasMedia() {
    return this.hasMedia_;
  },
  /**
   * @return {boolean} True if the volume is configurable.
   */
  get configurable() {
    return this.configurable_;
  },
  /**
   * @return {boolean} True if the volume is watchable.
   */
  get watchable() {
    return this.watchable_;
  },
  /**
   * @return {VolumeManagerCommon.Source} Source of the volume's data.
   */
  get source() {
    return this.source_;
  }
};

/**
 * Starts resolving the display root and obtains it.  It may take long time for
 * Drive. Once resolved, it is cached.
 *
 * @param {function(!DirectoryEntry)=} opt_onSuccess Success callback with the
 *     display root directory as an argument.
 * @param {function(*)=} opt_onFailure Failure callback.
 * @return {!Promise.<!DirectoryEntry>}
 */
VolumeInfo.prototype.resolveDisplayRoot = function(opt_onSuccess,
                                                   opt_onFailure) {
  if (!this.displayRootPromise_) {
    // TODO(mtomasz): Do not add VolumeInfo which failed to resolve root, and
    // remove this if logic. Call opt_onSuccess() always, instead.
    if (this.volumeType !== VolumeManagerCommon.VolumeType.DRIVE) {
      if (this.fileSystem_)
        this.displayRootPromise_ = /** @type {Promise.<!DirectoryEntry>} */ (
            Promise.resolve(this.fileSystem_.root));
      else
        this.displayRootPromise_ = /** @type {Promise.<!DirectoryEntry>} */ (
            Promise.reject(this.error));
    } else {
      // For Drive, we need to resolve.
      var displayRootURL = this.fileSystem_.root.toURL() + '/root';
      this.displayRootPromise_ = new Promise(
          window.webkitResolveLocalFileSystemURL.bind(null, displayRootURL));
    }

    // Store the obtained displayRoot.
    this.displayRootPromise_.then(function(displayRoot) {
      this.displayRoot_ = displayRoot;
    }.bind(this));
  }
  if (opt_onSuccess)
    this.displayRootPromise_.then(opt_onSuccess, opt_onFailure);
  return this.displayRootPromise_;
};

/**
 * Utilities for volume manager implementation.
 */
var volumeManagerUtil = {};

/**
 * @const {string}
 */
volumeManagerUtil.TIMEOUT_STR_REQUEST_FILE_SYSTEM =
    'timeout(requestFileSystem)';

/**
 * @const {string}
 */
volumeManagerUtil.TIMEOUT_STR_RESOLVE_ISOLATED_ENTRIES =
    'timeout(resolveIsolatedEntries)';

/**
 * Throws an Error when the given error is not in
 * VolumeManagerCommon.VolumeError.
 *
 * @param {string} error Status string usually received from APIs.
 */
volumeManagerUtil.validateError = function(error) {
  for (var key in VolumeManagerCommon.VolumeError) {
    if (error === VolumeManagerCommon.VolumeError[key])
      return;
  }

  throw new Error('Invalid mount error: ' + error);
};

/**
 * Builds the VolumeInfo data from VolumeMetadata.
 * @param {VolumeMetadata} volumeMetadata Metadata instance for the volume.
 * @return {!Promise.<VolumeInfo>} Promise settled with the VolumeInfo instance.
 */
volumeManagerUtil.createVolumeInfo = function(volumeMetadata) {
  var localizedLabel;
  switch (volumeMetadata.volumeType) {
    case VolumeManagerCommon.VolumeType.DOWNLOADS:
      localizedLabel = str('DOWNLOADS_DIRECTORY_LABEL');
      break;
    case VolumeManagerCommon.VolumeType.DRIVE:
      localizedLabel = str('DRIVE_DIRECTORY_LABEL');
      break;
    default:
      // TODO(mtomasz): Calculate volumeLabel for all types of volumes in the
      // C++ layer.
      localizedLabel = volumeMetadata.volumeLabel ||
          volumeMetadata.volumeId.split(':', 2)[1];
      break;
  }

  console.debug('Requesting file system.');
  return util.timeoutPromise(
      new Promise(function(resolve, reject) {
        chrome.fileSystem.requestFileSystem(
            {
              volumeId: volumeMetadata.volumeId,
              writable: !volumeMetadata.isReadOnly
            },
            function(isolatedFileSystem) {
              if (chrome.runtime.lastError)
                reject(chrome.runtime.lastError.message);
              else
                resolve(isolatedFileSystem);
            });
      }),
      VolumeManager.TIMEOUT,
      volumeManagerUtil.TIMEOUT_STR_REQUEST_FILE_SYSTEM +
          ': ' + volumeMetadata.volumeId)
  .then(
      /**
       * @param {!FileSystem} isolatedFileSystem
       */
      function(isolatedFileSystem) {
        // Since File System API works on isolated entries only, we need to
        // convert it back to external one.
        // TODO(mtomasz): Make Files app work on isolated entries.
        return util.timeoutPromise(
            new Promise(function(resolve, reject) {
              chrome.fileManagerPrivate.resolveIsolatedEntries(
                  [isolatedFileSystem.root],
                  function(entries) {
                    if (chrome.runtime.lastError)
                      reject(chrome.runtime.lastError.message);
                    else if (!entries[0])
                      reject('Resolving for external context failed.');
                    else
                      resolve(entries[0].filesystem);
                  });
            }),
            VolumeManager.TIMEOUT,
            volumeManagerUtil.TIMEOUT_STR_RESOLVE_ISOLATED_ENTRIES +
                ': ' + volumeMetadata.volumeId);
       })
  .then(
      /**
       * @param {!FileSystem} fileSystem
       */
      function(fileSystem) {
        console.debug('File system obtained: ' + volumeMetadata.volumeId);
        if (volumeMetadata.volumeType ===
            VolumeManagerCommon.VolumeType.DRIVE) {
          // After file system is mounted, we "read" drive grand root
          // entry at first. This triggers full feed fetch on background.
          // Note: we don't need to handle errors here, because even if
          // it fails, accessing to some path later will just become
          // a fast-fetch and it re-triggers full-feed fetch.
          fileSystem.root.createReader().readEntries(
              function() { /* do nothing */ },
              function(error) {
                console.error(
                    'Triggering full feed fetch is failed: ' +
                    error.name);
              });
        }
        return new VolumeInfo(
            /** @type {VolumeManagerCommon.VolumeType} */
            (volumeMetadata.volumeType),
            volumeMetadata.volumeId,
            fileSystem,
            volumeMetadata.mountCondition,
            volumeMetadata.deviceType,
            volumeMetadata.devicePath,
            volumeMetadata.isReadOnly,
            volumeMetadata.profile,
            localizedLabel,
            volumeMetadata.extensionId,
            volumeMetadata.hasMedia,
            volumeMetadata.configurable,
            volumeMetadata.watchable,
            /** @type {VolumeManagerCommon.Source} */
            (volumeMetadata.source));
  })
  .catch(
      /**
       * @param {*} error
       */
      function(error) {
        console.error('Failed to mount a file system: ' +
            volumeMetadata.volumeId + ' because of: ' +
            (error.stack || error));
        volumeManagerUtil.reportMountError(volumeMetadata, error);

        return new VolumeInfo(
            /** @type {VolumeManagerCommon.VolumeType} */
            (volumeMetadata.volumeType),
            volumeMetadata.volumeId,
            null,  // File system is not found.
            volumeMetadata.mountCondition,
            volumeMetadata.deviceType,
            volumeMetadata.devicePath,
            volumeMetadata.isReadOnly,
            volumeMetadata.profile,
            localizedLabel,
            volumeMetadata.extensionId,
            volumeMetadata.hasMedia,
            volumeMetadata.configurable,
            volumeMetadata.watchable,
            /** @type {VolumeManagerCommon.Source} */
            (volumeMetadata.source));
      });
};


/**
 * Reports a mount error to analytics in the form of
 * "mount {errorType} {volumeType}", like
 * "mount timeout(resolveIsolatedEntries) provided:ZipUnpacker".
 * Note that errorType and volumeType must be an element of fixed set of strings
 * to avoid sending dynamic strings to analytics.
 *
 * @param {VolumeMetadata} volumeMetadata
 * @param {*} error
 */
volumeManagerUtil.reportMountError = function(volumeMetadata, error) {
  var errorType = 'error';
  if (error instanceof Error) {
    if (error.message.startsWith(
        volumeManagerUtil.TIMEOUT_STR_REQUEST_FILE_SYSTEM)) {
      errorType = volumeManagerUtil.TIMEOUT_STR_REQUEST_FILE_SYSTEM;
    }
    if (error.message.startsWith(
        volumeManagerUtil.TIMEOUT_STR_RESOLVE_ISOLATED_ENTRIES)) {
      errorType = volumeManagerUtil.TIMEOUT_STR_RESOLVE_ISOLATED_ENTRIES;
    }
  }
  var volumeType = volumeMetadata.volumeType;
  if (volumeMetadata.volumeType === VolumeManagerCommon.VolumeType.PROVIDED) {
    volumeType += ':' + metrics.getFileSystemProviderName(
        volumeMetadata.extensionId);
  }
  var description = 'mount ' + errorType + ' ' + volumeType;
  var fatal =
      volumeMetadata.volumeType === VolumeManagerCommon.VolumeType.DOWNLOADS ||
      volumeMetadata.volumeType === VolumeManagerCommon.VolumeType.DRIVE;

  if (window.background && window.background.tracker)
    window.background.tracker.sendException(description, fatal);
};

/**
 * The order of the volume list based on root type.
 * @type {Array<VolumeManagerCommon.VolumeType>}
 * @const
 * @private
 */
volumeManagerUtil.volumeListOrder_ = [
  VolumeManagerCommon.VolumeType.DRIVE,
  VolumeManagerCommon.VolumeType.DOWNLOADS,
  VolumeManagerCommon.VolumeType.ARCHIVE,
  VolumeManagerCommon.VolumeType.REMOVABLE,
  VolumeManagerCommon.VolumeType.MTP,
  VolumeManagerCommon.VolumeType.PROVIDED,
];

/**
 * Orders two volumes by volumeType and volumeId.
 *
 * The volumes at first are compared by volume type in the order of
 * volumeListOrder_.  Then they are compared by volume ID.
 *
 * @param {!VolumeInfo} volumeInfo1 Volume info to be compared.
 * @param {!VolumeInfo} volumeInfo2 Volume info to be compared.
 * @return {number} Returns -1 if volume1 < volume2, returns 1 if volume2 >
 *     volume1, returns 0 if volume1 === volume2.
 * @private
 */
volumeManagerUtil.compareVolumeInfo_ = function(volumeInfo1, volumeInfo2) {
  var typeIndex1 =
      volumeManagerUtil.volumeListOrder_.indexOf(volumeInfo1.volumeType);
  var typeIndex2 =
      volumeManagerUtil.volumeListOrder_.indexOf(volumeInfo2.volumeType);
  if (typeIndex1 !== typeIndex2)
    return typeIndex1 < typeIndex2 ? -1 : 1;
  if (volumeInfo1.volumeId !== volumeInfo2.volumeId)
    return volumeInfo1.volumeId < volumeInfo2.volumeId ? -1 : 1;
  return 0;
};

/**
 * The container of the VolumeInfo for each mounted volume.
 * @constructor
 */
function VolumeInfoList() {
  var field = 'volumeType,volumeId';

  /**
   * Holds VolumeInfo instances.
   * @type {cr.ui.ArrayDataModel}
   * @private
   */
  this.model_ = new cr.ui.ArrayDataModel([]);
  this.model_.setCompareFunction(field,
                                 /** @type {function(*, *): number} */
                                 (volumeManagerUtil.compareVolumeInfo_));
  this.model_.sort(field, 'asc');

  Object.freeze(this);
}

VolumeInfoList.prototype = {
  get length() { return this.model_.length; }
};

/**
 * Adds the event listener to listen the change of volume info.
 * @param {string} type The name of the event.
 * @param {function(Event)} handler The handler for the event.
 */
VolumeInfoList.prototype.addEventListener = function(type, handler) {
  this.model_.addEventListener(type, handler);
};

/**
 * Removes the event listener.
 * @param {string} type The name of the event.
 * @param {function(Event)} handler The handler to be removed.
 */
VolumeInfoList.prototype.removeEventListener = function(type, handler) {
  this.model_.removeEventListener(type, handler);
};

/**
 * Adds the volumeInfo to the appropriate position. If there already exists,
 * just replaces it.
 * @param {VolumeInfo} volumeInfo The information of the new volume.
 */
VolumeInfoList.prototype.add = function(volumeInfo) {
  var index = this.findIndex(volumeInfo.volumeId);
  if (index !== -1)
    this.model_.splice(index, 1, volumeInfo);
  else
    this.model_.push(volumeInfo);
};

/**
 * Removes the VolumeInfo having the given ID.
 * @param {string} volumeId ID of the volume.
 */
VolumeInfoList.prototype.remove = function(volumeId) {
  var index = this.findIndex(volumeId);
  if (index !== -1)
    this.model_.splice(index, 1);
};

/**
 * Obtains an index from the volume ID.
 * @param {string} volumeId Volume ID.
 * @return {number} Index of the volume.
 */
VolumeInfoList.prototype.findIndex = function(volumeId) {
  for (var i = 0; i < this.model_.length; i++) {
    if (this.model_.item(i).volumeId === volumeId)
      return i;
  }
  return -1;
};

/**
 * Searches the information of the volume that contains the passed entry.
 * @param {!Entry|!FakeEntry} entry Entry on the volume to be found.
 * @return {VolumeInfo} The volume's information, or null if not found.
 */
VolumeInfoList.prototype.findByEntry = function(entry) {
  for (var i = 0; i < this.length; i++) {
    var volumeInfo = this.item(i);
    if (volumeInfo.fileSystem &&
        util.isSameFileSystem(volumeInfo.fileSystem, entry.filesystem)) {
      return volumeInfo;
    }
    // Additionally, check fake entries.
    for (var key in volumeInfo.fakeEntries_) {
      var fakeEntry = volumeInfo.fakeEntries_[key];
      if (util.isSameEntry(fakeEntry, entry))
        return volumeInfo;
    }
  }
  return null;
};

/**
 * Searches the information of the volume that exists on the given device path.
 * @param {string} devicePath Path of the device to search.
 * @return {VolumeInfo} The volume's information, or null if not found.
 */
VolumeInfoList.prototype.findByDevicePath = function(devicePath) {
  for (var i = 0; i < this.length; i++) {
    var volumeInfo = this.item(i);
    if (volumeInfo.devicePath &&
        volumeInfo.devicePath == devicePath) {
      return volumeInfo;
    }
  }
  return null;
};

/**
 * Returns a VolumInfo for the volume ID, or null if not found.
 *
 * @param {string} volumeId
 * @return {VolumeInfo} The volume's information, or null if not found.
 * @private
 */
VolumeInfoList.prototype.findByVolumeId_ = function(volumeId) {
  var index = this.findIndex(volumeId);
  return (index !== -1) ?
      /** @type {VolumeInfo} */ (this.model_.item(index)) :
      null;
};

/**
 * Returns a promise that will be resolved when volume info, identified
 * by {@code volumeId} is created.
 *
 * @param {string} volumeId
 * @return {!Promise.<!VolumeInfo>} The VolumeInfo. Will not resolve
 *     if the volume is never mounted.
 */
VolumeInfoList.prototype.whenVolumeInfoReady = function(volumeId) {
  return new Promise(function(fulfill) {
    var handler = function() {
      var info = this.findByVolumeId_(volumeId);
      if (info) {
        fulfill(info);
        this.model_.removeEventListener('splice', handler);
      }
    }.bind(this);
    this.model_.addEventListener('splice', handler);
    handler();
  }.bind(this));
};

/**
 * @param {number} index The index of the volume in the list.
 * @return {!VolumeInfo} The VolumeInfo instance.
 */
VolumeInfoList.prototype.item = function(index) {
  return /** @type {!VolumeInfo} */ (this.model_.item(index));
};

/**
 * VolumeManager is responsible for tracking list of mounted volumes.
 *
 * @constructor
 * @implements {VolumeManagerCommon.VolumeInfoProvider}
 * @extends {cr.EventTarget}
 */
function VolumeManager() {
  /**
   * The list of archives requested to mount. We will show contents once
   * archive is mounted, but only for mounts from within this filebrowser tab.
   * @type {Object<Object>}
   * @private
   */
  this.requests_ = {};

  /**
   * The list of VolumeInfo instances for each mounted volume.
   * @type {VolumeInfoList}
   */
  this.volumeInfoList = new VolumeInfoList();

  /**
   * Queue for mounting.
   * @type {AsyncUtil.Queue}
   * @private
   */
  this.mountQueue_ = new AsyncUtil.Queue();

  // The status should be merged into VolumeManager.
  // TODO(hidehiko): Remove them after the migration.
  /**
   * Connection state of the Drive.
   * @type {VolumeManagerCommon.DriveConnectionState}
   * @private
   */
  this.driveConnectionState_ = {
    type: VolumeManagerCommon.DriveConnectionType.OFFLINE,
    reason: VolumeManagerCommon.DriveConnectionReason.NO_SERVICE,
    hasCellularNetworkAccess: false
  };

  chrome.fileManagerPrivate.onDriveConnectionStatusChanged.addListener(
      this.onDriveConnectionStatusChanged_.bind(this));
  this.onDriveConnectionStatusChanged_();
}

/**
 * Invoked when the drive connection status is changed.
 * @private
 */
VolumeManager.prototype.onDriveConnectionStatusChanged_ = function() {
  chrome.fileManagerPrivate.getDriveConnectionState(function(state) {
    this.driveConnectionState_ = state;
    cr.dispatchSimpleEvent(this, 'drive-connection-changed');
  }.bind(this));
};

/**
 * Returns the drive connection state.
 * @return {VolumeManagerCommon.DriveConnectionState} Connection state.
 */
VolumeManager.prototype.getDriveConnectionState = function() {
  return this.driveConnectionState_;
};

/**
 * VolumeManager extends cr.EventTarget.
 */
VolumeManager.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Time in milliseconds that we wait a response for. If no response on
 * mount/unmount received the request supposed failed.
 */
VolumeManager.TIMEOUT = 15 * 60 * 1000;

/**
 * The singleton instance of VolumeManager. Initialized by the first invocation
 * of getInstance().
 * @type {VolumeManager}
 * @private
 */
VolumeManager.instance_ = null;

/**
 * Returns instance of VolumeManager for debug purpose.
 * This method returns VolumeManager.instance_ which may not be initialized.
 *
 * @return {VolumeManager} Volume manager.
 */
VolumeManager.getInstanceForDebug = function() {
  return VolumeManager.instance_;
};

/**
 * @type {Promise}
 * @private
 */
VolumeManager.instancePromise_ = null;

/**
 * Returns the VolumeManager instance asynchronously. If it is not created or
 * under initialization, it will waits for the finish of the initialization.
 * @param {function(VolumeManager)=} opt_callback Called with the VolumeManager
 *     instance. TODO(hirono): Remove the callback and use Promise instead.
 * @return {Promise} Promise to be fulfilled with the volume manager.
 */
VolumeManager.getInstance = function(opt_callback) {
  if (!VolumeManager.instancePromise_) {
    VolumeManager.instance_ = new VolumeManager();
    VolumeManager.instancePromise_ = new Promise(function(fulfill) {
      VolumeManager.instance_.initialize_(function() {
        return fulfill(VolumeManager.instance_);
      });
    });
  }
  if (opt_callback)
    VolumeManager.instancePromise_.then(opt_callback);
  return VolumeManager.instancePromise_;
};

/**
 * Revokes the singleton instance for testing.
 */
VolumeManager.revokeInstanceForTesting = function() {
  VolumeManager.instancePromise_ = null;
  VolumeManager.instance_ = null;
};

/**
 * Adds new volume info from the given volumeMetadata. If the corresponding
 * volume info has already been added, the volumeMetadata is ignored.
 * @param {!VolumeMetadata} volumeMetadata
 * @return {!Promise<!VolumeInfo>}
 * @private
 */
VolumeManager.prototype.addVolumeMetadata_ = function(volumeMetadata) {
  return volumeManagerUtil.createVolumeInfo(volumeMetadata).then(
      /**
       * @param {!VolumeInfo} volumeInfo
       * @return {!VolumeInfo}
       */
      function(volumeInfo) {
        // We don't show Downloads and Drive on volume list if they have mount
        // error, since users can do nothing in this situation.
        // We show Removable and Provided volumes regardless of mount error so
        // that users can unmount or format the volume.
        // TODO(fukino): Once Files.app get ready, show erroneous Drive volume
        // so that users can see auth warning banner on the volume.
        // crbug.com/517772.
        var shouldShow = true;
        switch (volumeInfo.volumeType) {
          case VolumeManagerCommon.VolumeType.DOWNLOADS:
          case VolumeManagerCommon.VolumeType.DRIVE:
            shouldShow = !!volumeInfo.fileSystem;
            break;
        }
        if (shouldShow &&
            this.volumeInfoList.findIndex(volumeInfo.volumeId) === -1) {
          this.volumeInfoList.add(volumeInfo);

          // Update the network connection status, because until the drive is
          // initialized, the status is set to not ready.
          // TODO(mtomasz): The connection status should be migrated into
          // VolumeMetadata.
          if (volumeMetadata.volumeType ===
              VolumeManagerCommon.VolumeType.DRIVE) {
            this.onDriveConnectionStatusChanged_();
          }
        }
        return volumeInfo;
      }.bind(this));
};

/**
 * Initializes mount points.
 * @param {function()} callback Called upon the completion of the
 *     initialization.
 * @private
 */
VolumeManager.prototype.initialize_ = function(callback) {
  chrome.fileManagerPrivate.onMountCompleted.addListener(
      this.onMountCompleted_.bind(this));
  console.debug('Requesting volume list.');
  chrome.fileManagerPrivate.getVolumeMetadataList(function(volumeMetadataList) {
    console.debug('Volume list fetched with: ' + volumeMetadataList.length +
        ' items.');
    // We must subscribe to the mount completed event in the callback of
    // getVolumeMetadataList. crbug.com/330061.
    // But volumes reported by onMountCompleted events must be added after the
    // volumes in the volumeMetadataList are mounted. crbug.com/135477.
    this.mountQueue_.run(function(inCallback) {
      // Create VolumeInfo for each volume.
      Promise.all(
          volumeMetadataList.map(function(volumeMetadata) {
            console.debug(
                'Initializing volume: ' + volumeMetadata.volumeId);
            return this.addVolumeMetadata_(volumeMetadata).then(
                function(volumeInfo) {
                  console.debug('Initialized volume: ' + volumeInfo.volumeId);
                });
          }.bind(this)))
          .then(function() {
            console.debug('Initialized all volumes.');
            // Call the callback of the initialize function.
            callback();
            // Call the callback of AsyncQueue. Maybe it invokes callbacks
            // registered by mountCompleted events.
            inCallback();
          });
    }.bind(this));
  }.bind(this));
};

/**
 * Event handler called when some volume was mounted or unmounted.
 * @param {MountCompletedEvent} event Received event.
 * @private
 */
VolumeManager.prototype.onMountCompleted_ = function(event) {
  this.mountQueue_.run(function(callback) {
    switch (event.eventType) {
      case 'mount':
        var requestKey = this.makeRequestKey_(
            'mount',
            event.volumeMetadata.sourcePath || '');

        if (event.status === 'success' ||
            event.status ===
                VolumeManagerCommon.VolumeError.UNKNOWN_FILESYSTEM ||
            event.status ===
                VolumeManagerCommon.VolumeError.UNSUPPORTED_FILESYSTEM) {
          this.addVolumeMetadata_(event.volumeMetadata).then(
              function(volumeInfo) {
                this.finishRequest_(requestKey, event.status, volumeInfo);
                callback();
              }.bind(this));
        } else {
          console.warn('Failed to mount a volume: ' + event.status);
          this.finishRequest_(requestKey, event.status);
          callback();
        }
        break;

      case 'unmount':
        var volumeId = event.volumeMetadata.volumeId;
        var status = event.status;
        if (status === VolumeManagerCommon.VolumeError.PATH_UNMOUNTED) {
          console.warn('Volume already unmounted: ', volumeId);
          status = 'success';
        }
        var requestKey = this.makeRequestKey_('unmount', volumeId);
        var requested = requestKey in this.requests_;
        var volumeInfoIndex =
            this.volumeInfoList.findIndex(volumeId);
        var volumeInfo = volumeInfoIndex !== -1 ?
            this.volumeInfoList.item(volumeInfoIndex) : null;
        if (event.status === 'success' && !requested && volumeInfo) {
          console.warn('Mounted volume without a request: ' + volumeId);
          var e = new Event('externally-unmounted');
          e.volumeInfo = volumeInfo;
          this.dispatchEvent(e);
        }

        this.finishRequest_(requestKey, status);
        if (event.status === 'success')
          this.volumeInfoList.remove(event.volumeMetadata.volumeId);
        callback();
        break;
    }
  }.bind(this));
};

/**
 * Creates string to match mount events with requests.
 * @param {string} requestType 'mount' | 'unmount'. TODO(hidehiko): Replace by
 *     enum.
 * @param {string} argument Argument describing the request, eg. source file
 *     path of the archive to be mounted, or a volumeId for unmounting.
 * @return {string} Key for |this.requests_|.
 * @private
 */
VolumeManager.prototype.makeRequestKey_ = function(requestType, argument) {
  return requestType + ':' + argument;
};

/**
 * @param {string} fileUrl File url to the archive file.
 * @param {function(VolumeInfo)} successCallback Success callback.
 * @param {function(VolumeManagerCommon.VolumeError)} errorCallback Error
 *     callback.
 */
VolumeManager.prototype.mountArchive = function(
    fileUrl, successCallback, errorCallback) {
  chrome.fileManagerPrivate.addMount(fileUrl, function(sourcePath) {
    console.info(
        'Mount request: url=' + fileUrl + '; sourcePath=' + sourcePath);
    var requestKey = this.makeRequestKey_('mount', sourcePath);
    this.startRequest_(requestKey, successCallback, errorCallback);
  }.bind(this));
};

/**
 * Unmounts a volume.
 * @param {!VolumeInfo} volumeInfo Volume to be unmounted.
 * @param {function()} successCallback Success callback.
 * @param {function(VolumeManagerCommon.VolumeError)} errorCallback Error
 *     callback.
 */
VolumeManager.prototype.unmount = function(volumeInfo,
                                           successCallback,
                                           errorCallback) {
  chrome.fileManagerPrivate.removeMount(volumeInfo.volumeId);
  var requestKey = this.makeRequestKey_('unmount', volumeInfo.volumeId);
  this.startRequest_(requestKey, successCallback, errorCallback);
};

/**
 * Configures a volume.
 * @param {!VolumeInfo} volumeInfo Volume to be configured.
 * @return {!Promise} Fulfilled on success, otherwise rejected with an error
 *     message.
 */
VolumeManager.prototype.configure = function(volumeInfo) {
  return new Promise(function(fulfill, reject) {
    chrome.fileManagerPrivate.configureVolume(
        volumeInfo.volumeId,
        function() {
          if (chrome.runtime.lastError)
            reject(chrome.runtime.lastError.message);
          else
            fulfill();
        });
  });
};

/** @override */
VolumeManager.prototype.getVolumeInfo = function(entry) {
  return this.volumeInfoList.findByEntry(entry);
};

/**
 * Obtains volume information of the current profile.
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {VolumeInfo} Volume info.
 */
VolumeManager.prototype.getCurrentProfileVolumeInfo = function(volumeType) {
  for (var i = 0; i < this.volumeInfoList.length; i++) {
    var volumeInfo = this.volumeInfoList.item(i);
    if (volumeInfo.profile.isCurrentProfile &&
        volumeInfo.volumeType === volumeType)
      return volumeInfo;
  }
  return null;
};

/**
 * Obtains location information from an entry.
 *
 * @param {(!Entry|!FakeEntry)} entry File or directory entry. It can be a fake
 *     entry.
 * @return {EntryLocation} Location information.
 */
VolumeManager.prototype.getLocationInfo = function(entry) {
  var volumeInfo = this.volumeInfoList.findByEntry(entry);
  if (!volumeInfo)
    return null;

  if (util.isFakeEntry(entry)) {
    return new EntryLocation(
        volumeInfo,
        entry.rootType,
        true /* the entry points a root directory. */,
        true /* fake entries are read only. */);
  }

  var rootType;
  var isReadOnly;
  var isRootEntry;
  if (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
    // For Drive, the roots are /root and /other, instead of /. Root URLs
    // contain trailing slashes.
    if (entry.fullPath == '/root' || entry.fullPath.indexOf('/root/') === 0) {
      rootType = VolumeManagerCommon.RootType.DRIVE;
      isReadOnly = volumeInfo.isReadOnly;
      isRootEntry = entry.fullPath === '/root';
    } else if (entry.fullPath == '/other' ||
               entry.fullPath.indexOf('/other/') === 0) {
      rootType = VolumeManagerCommon.RootType.DRIVE_OTHER;
      isReadOnly = true;
      isRootEntry = entry.fullPath === '/other';
    } else {
      // Accessing Drive files outside of /drive/root and /drive/other is not
      // allowed, but can happen. Therefore returning null.
      return null;
    }
  } else {
    switch (volumeInfo.volumeType) {
      case VolumeManagerCommon.VolumeType.DOWNLOADS:
        rootType = VolumeManagerCommon.RootType.DOWNLOADS;
        break;
      case VolumeManagerCommon.VolumeType.REMOVABLE:
        rootType = VolumeManagerCommon.RootType.REMOVABLE;
        break;
      case VolumeManagerCommon.VolumeType.ARCHIVE:
        rootType = VolumeManagerCommon.RootType.ARCHIVE;
        break;
      case VolumeManagerCommon.VolumeType.MTP:
        rootType = VolumeManagerCommon.RootType.MTP;
        break;
      case VolumeManagerCommon.VolumeType.PROVIDED:
        rootType = VolumeManagerCommon.RootType.PROVIDED;
        break;
      default:
        // Programming error, throw an exception.
        throw new Error('Invalid volume type: ' + volumeInfo.volumeType);
    }
    isReadOnly = volumeInfo.isReadOnly;
    isRootEntry = util.isSameEntry(entry, volumeInfo.fileSystem.root);
  }

  return new EntryLocation(volumeInfo, rootType, isRootEntry, isReadOnly);
};

/**
 * @param {string} key Key produced by |makeRequestKey_|.
 * @param {function(VolumeInfo)} successCallback To be called when the request
 *     finishes successfully.
 * @param {function(VolumeManagerCommon.VolumeError)} errorCallback To be called
 *     when the request fails.
 * @private
 */
VolumeManager.prototype.startRequest_ = function(key,
    successCallback, errorCallback) {
  if (key in this.requests_) {
    var request = this.requests_[key];
    request.successCallbacks.push(successCallback);
    request.errorCallbacks.push(errorCallback);
  } else {
    this.requests_[key] = {
      successCallbacks: [successCallback],
      errorCallbacks: [errorCallback],

      timeout: setTimeout(this.onTimeout_.bind(this, key),
                          VolumeManager.TIMEOUT)
    };
  }
};

/**
 * Called if no response received in |TIMEOUT|.
 * @param {string} key Key produced by |makeRequestKey_|.
 * @private
 */
VolumeManager.prototype.onTimeout_ = function(key) {
  this.invokeRequestCallbacks_(this.requests_[key],
                               VolumeManagerCommon.VolumeError.TIMEOUT);
  delete this.requests_[key];
};

/**
 * @param {string} key Key produced by |makeRequestKey_|.
 * @param {VolumeManagerCommon.VolumeError|string} status Status received
 *     from the API.
 * @param {VolumeInfo=} opt_volumeInfo Volume info of the mounted volume.
 * @private
 */
VolumeManager.prototype.finishRequest_ = function(key, status, opt_volumeInfo) {
  var request = this.requests_[key];
  if (!request)
    return;

  clearTimeout(request.timeout);
  this.invokeRequestCallbacks_(request, status, opt_volumeInfo);
  delete this.requests_[key];
};

/**
 * @param {Object} request Structure created in |startRequest_|.
 * @param {VolumeManagerCommon.VolumeError|string} status If status ===
 *     'success' success callbacks are called.
 * @param {VolumeInfo=} opt_volumeInfo Volume info of the mounted volume.
 * @private
 */
VolumeManager.prototype.invokeRequestCallbacks_ = function(
    request, status, opt_volumeInfo) {
  var callEach = function(callbacks, self, args) {
    for (var i = 0; i < callbacks.length; i++) {
      callbacks[i].apply(self, args);
    }
  };
  if (status === 'success') {
    callEach(request.successCallbacks, this, [opt_volumeInfo]);
  } else {
    volumeManagerUtil.validateError(status);
    callEach(request.errorCallbacks, this, [status]);
  }
};

/**
 * Returns current state of VolumeManager.
 * @return {string} Current state of VolumeManager.
 */
VolumeManager.prototype.toString = function() {
  return 'VolumeManager\n' +
      '- MountQueue_:\n' +
      '  ' + this.mountQueue_.toString().replace(/\n/g, '\n  ');
};

/**
 * Location information which shows where the path points in FileManager's
 * file system.
 *
 * @param {!VolumeInfo} volumeInfo Volume information.
 * @param {VolumeManagerCommon.RootType} rootType Root type.
 * @param {boolean} isRootEntry Whether the entry is root entry or not.
 * @param {boolean} isReadOnly Whether the entry is read only or not.
 * @constructor
 */
function EntryLocation(volumeInfo, rootType, isRootEntry, isReadOnly) {
  /**
   * Volume information.
   * @type {!VolumeInfo}
   */
  this.volumeInfo = volumeInfo;

  /**
   * Root type.
   * @type {VolumeManagerCommon.RootType}
   */
  this.rootType = rootType;

  /**
   * Whether the entry is root entry or not.
   * @type {boolean}
   */
  this.isRootEntry = isRootEntry;

  /**
   * Whether the location obtained from the fake entry correspond to special
   * searches.
   * @type {boolean}
   */
  this.isSpecialSearchRoot =
      this.rootType === VolumeManagerCommon.RootType.DRIVE_OFFLINE ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_RECENT;

  /**
   * Whether the location is under Google Drive or a special search root which
   * represents a special search from Google Drive.
   * @type {boolean}
   */
  this.isDriveBased =
      this.rootType === VolumeManagerCommon.RootType.DRIVE ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_OTHER ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_RECENT ||
      this.rootType === VolumeManagerCommon.RootType.DRIVE_OFFLINE;

  /**
   * Whether the entry is read only or not.
   * @type {boolean}
   */
  this.isReadOnly = isReadOnly;

  Object.freeze(this);
}
