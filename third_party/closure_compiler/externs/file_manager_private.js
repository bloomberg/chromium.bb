// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: fileManagerPrivate */

/**
 * @typedef {{
 *   taskId: string,
 *   title: string,
 *   iconUrl: string,
 *   isDefault: boolean
 * }}
 */
var FileTask;

/**
 * @typedef {{
 *   size: (number|undefined),
 *   modificationTime: (number|undefined),
 *   thumbnailUrl: (string|undefined),
 *   externalFileUrl: (string|undefined),
 *   imageWidth: (number|undefined),
 *   imageHeight: (number|undefined),
 *   imageRotation: (number|undefined),
 *   pinned: (boolean|undefined),
 *   present: (boolean|undefined),
 *   hosted: (boolean|undefined),
 *   dirty: (boolean|undefined),
 *   availableOffline: (boolean|undefined),
 *   availableWhenMetered: (boolean|undefined),
 *   customIconUrl: (string|undefined),
 *   contentMimeType: (string|undefined),
 *   sharedWithMe: (boolean|undefined),
 *   shared: (boolean|undefined)
 * }}
 */
var EntryProperties;

/**
 * @typedef {{
 *   totalSize: number,
 *   remainingSize: number
 * }}
 */
var MountPointSizeStats;

/**
 * @typedef {{
 *   profileId: string,
 *   displayName: string,
 *   isCurrentProfile: boolean
 * }}
 */
var ProfileInfo;

/**
 * @typedef {{
 *   volumeId: string,
 *   fileSystemId: (string|undefined),
 *   extensionId: (string|undefined),
 *   volumeLabel: (string|undefined),
 *   profile: ProfileInfo,
 *   sourcePath: (string|undefined),
 *   volumeType: string,
 *   deviceType: (string|undefined),
 *   devicePath: (string|undefined),
 *   isParentDevice: (boolean|undefined),
 *   isReadOnly: boolean,
 *   hasMedia: boolean,
 *   mountCondition: (string|undefined),
 *   mountContext: (string|undefined)
 * }}
 */
var VolumeMetadata;

/**
 * @typedef {{
 *   eventType: string,
 *   status: string,
 *   volumeMetadata: VolumeMetadata,
 *   shouldNotify: boolean
 * }}
 */
var MountCompletedEvent;

/**
 * @typedef {{
 *   fileUrl: string,
 *   transferState: string,
 *   transferType: string,
 *   processed: number,
 *   total: number,
 *   num_total_jobs: number
 * }}
 */
var FileTransferStatus;

/**
 * @typedef {{
 *   type: string,
 *   fileUrl: string
 * }}
 */
var DriveSyncErrorEvent;

/**
 * @typedef {{
 *   type: string,
 *   sourceUrl: (string|undefined),
 *   destinationUrl: (string|undefined),
 *   size: (number|undefined),
 *   error: (string|undefined)
 * }}
 */
var CopyProgressStatus;

/**
 * @typedef {{
 *   fileUrl: string,
 *   canceled: boolean
 * }}
 */
var FileTransferCancelStatus;

/**
 * @typedef {{
 *   url: string,
 *   changes: Array
 * }}
 */
var FileChange;

/**
 * @typedef {{
 *   eventType: string,
 *   entry: Object,
 *   changedFiles: (Array|undefined)
 * }}
 */
var FileWatchEvent;

/**
 * @typedef {{
 *   driveEnabled: boolean,
 *   cellularDisabled: boolean,
 *   hostedFilesDisabled: boolean,
 *   use24hourClock: boolean,
 *   allowRedeemOffers: boolean
 * }}
 */
var Preferences;

/**
 * @typedef {{
 *   cellularDisabled: (boolean|undefined),
 *   hostedFilesDisabled: (boolean|undefined)
 * }}
 */
var PreferencesChange;

/**
 * @typedef {{
 *   query: string,
 *   nextFeed: string
 * }}
 */
var SearchParams;

/**
 * @typedef {{
 *   query: string,
 *   types: string,
 *   maxResults: number
 * }}
 */
var SearchMetadataParams;

/**
 * @typedef {{
 *   entry: Object,
 *   highlightedBaseName: string
 * }}
 */
var SearchResult;

/**
 * @typedef {{
 *   type: string,
 *   reason: (string|undefined)
 * }}
 */
var DriveConnectionState;

/**
 * @typedef {{
 *   type: string,
 *   devicePath: string
 * }}
 */
var DeviceEvent;

/**
 * @const
 */
chrome.fileManagerPrivate = {};

/**
 * Logout the current user for navigating to the re-authentication screen for
 * the Google account.
 */
chrome.fileManagerPrivate.logoutUserForReauthentication = function() {};

/**
 * Cancels file selection.
 */
chrome.fileManagerPrivate.cancelDialog = function() {};

/**
 * Executes file browser task over selected files. |taskId| The unique
 * identifier of task to execute. |fileUrls| Array of file URLs |callback|
 * @param {string} taskId
 * @param {Array} fileUrls
 * @param {Function=} callback |result| Result of the task execution.
 */
chrome.fileManagerPrivate.executeTask = function(taskId, fileUrls, callback) {};

/**
 * Sets the default task for the supplied MIME types and suffixes of the
 * supplied file URLs. Lists of MIME types and URLs may contain duplicates.
 * |taskId| The unique identifier of task to mark as default. |fileUrls| Array
 * of selected file URLs to extract suffixes from. |mimeTypes| Array of
 * selected file MIME types. |callback|
 * @param {string} taskId
 * @param {Array} fileUrls
 * @param {Array=} mimeTypes
 * @param {Function=} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.setDefaultTask = function(taskId, fileUrls, mimeTypes, callback) {};

/**
 * Gets the list of tasks that can be performed over selected files. |fileUrls|
 * Array of selected file URLs |callback|
 * @param {Array} fileUrls
 * @param {Function} callback |tasks| The list of matched file URL patterns for
 * this task.
 */
chrome.fileManagerPrivate.getFileTasks = function(fileUrls, callback) {};

/**
 * Gets localized strings and initialization data. |callback|
 * @param {Function} callback |result| Hash containing the string assets.
 */
chrome.fileManagerPrivate.getStrings = function(callback) {};

/**
 * Adds file watch. |fileUrl| URL of file to watch |callback|
 * @param {string} fileUrl
 * @param {Function} callback |success| True when file watch is successfully
 * added.
 */
chrome.fileManagerPrivate.addFileWatch = function(fileUrl, callback) {};

/**
 * Removes file watch. |fileUrl| URL of watched file to remove |callback|
 * @param {string} fileUrl
 * @param {Function} callback |success| True when file watch is successfully
 * removed.
 */
chrome.fileManagerPrivate.removeFileWatch = function(fileUrl, callback) {};

/**
 * Enables the extenal file scheme necessary to initiate drags to the browser
 * window for files on the external backend.
 */
chrome.fileManagerPrivate.enableExternalFileScheme = function() {};

/**
 * Requests R/W access to the specified entries as |entryUrls|. Note, that only
 * files backed by external file system backend will be granted the access.
 * @param {!Array<string>} entryUrls
 * @param {function()} callback Completion callback.
 */
chrome.fileManagerPrivate.grantAccess = function(entryUrls, callback) {};

/**
 * Selects multiple files. |selectedPaths| Array of selected paths
 * |shouldReturnLocalPath| true if paths need to be resolved to local paths.
 * |callback|
 * @param {Array} selectedPaths
 * @param {boolean} shouldReturnLocalPath
 * @param {Function} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.selectFiles = function(selectedPaths, shouldReturnLocalPath, callback) {};

/**
 * Selects a file. |selectedPath| A selected path |index| Index of Filter
 * |forOpening| true if paths are selected for opening. false if for saving.
 * |shouldReturnLocalPath| true if paths need to be resolved to local paths.
 * |callback|
 * @param {string} selectedPath
 * @param {number} index
 * @param {boolean} forOpening
 * @param {boolean} shouldReturnLocalPath
 * @param {Function} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.selectFile = function(selectedPath, index, forOpening, shouldReturnLocalPath, callback) {};

/**
 * Requests additional properties for files. |fileUrls| list of URLs of files
 * |callback|
 * @param {!Array<string>} fileUrls
 * @param {!Array<string>} names
 * @param {!Function} callback |entryProperties| A dictionary containing
 * properties of the requested entries.
 */
chrome.fileManagerPrivate.getEntryProperties = function(fileUrls, names, callback) {};

/**
 * Pins/unpins a Drive file in the cache. |fileUrl| URL of a file to pin/unpin.
 * |pin| Pass true to pin the file. |callback| Completion callback.
 * $(ref:runtime.lastError) will be set if     there was an error.
 * @param {string} fileUrl
 * @param {boolean} pin
 * @param {Function=} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.pinDriveFile = function(fileUrl, pin, callback) {};

/**
 * Resolves file entries in the isolated file system and returns corresponding
 * entries in the external file system mounted to Chrome OS file manager
 * backend. If resolving entry fails, the entry will be just ignored and the
 * corresponding entry does not appear in the result.
 * @param {Array} entries
 * @param {Function} callback |entryUrl| URL of an entry in a normal file
 * system.
 */
chrome.fileManagerPrivate.resolveIsolatedEntries = function(entries, callback) {};

/**
 * Mount a resource or a file. |source| Mount point source. For compressed
 * files it is relative file path     within external file system |callback|
 * @param {string} source
 * @param {Function} callback |sourcePath| Source path of the mount.
 */
chrome.fileManagerPrivate.addMount = function(source, callback) {};

/**
 * Unmounts a mounted resource. |volumeId| An ID of the volume.
 * @param {string} volumeId
 */
chrome.fileManagerPrivate.removeMount = function(volumeId) {};

/**
 * Get the list of mounted volumes. |callback|
 * @param {Function} callback |volumeMetadataList| The list of VolumeMetadata
 * representing mounted volumes.
 */
chrome.fileManagerPrivate.getVolumeMetadataList = function(callback) {};

/**
 * Cancels ongoing file transfers for selected files. |fileUrls| Array of files
 * for which ongoing transfer should be canceled.     If this is absent, all
 * jobs are canceled.  |callback|
 * @param {Array=} fileUrls
 * @param {Function=} callback |fileTransferCancelStatuses| The list of
 * FileTransferCancelStatus.
 */
chrome.fileManagerPrivate.cancelFileTransfers = function(fileUrls, callback) {};

/**
 * Starts to copy an entry. If the source is a directory, the copy is done
 * recursively. |sourceUrl| URL of the source entry to be copied. |parent| URL
 * of the destination directory. |newName| Name of the new entry. It shouldn't
 * contain '/'. |callback| Completion callback.
 * @param {string} sourceUrl
 * @param {string} parent
 * @param {string} newName
 * @param {Function} callback |copyId| ID of the copy task. Can be used to
 * identify the progress, and to cancel the task.
 */
chrome.fileManagerPrivate.startCopy = function(sourceUrl, parent, newName, callback) {};

/**
 * Cancels the running copy task. |copyId| ID of the copy task to be cancelled.
 * |callback| Completion callback of the cancel.
 * @param {number} copyId
 * @param {Function=} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.cancelCopy = function(copyId, callback) {};

/**
 * Retrieves total and remaining size of a mount point. |volumeId| ID of the
 * volume to be checked. |callback|
 * @param {string} volumeId
 * @param {Function} callback |sizeStats| Name/value pairs of size stats. Will
 * be undefined if stats could not be determined.
 */
chrome.fileManagerPrivate.getSizeStats = function(volumeId, callback) {};

/**
 * Formats a mounted volume. |volumeId| ID of the volume to be formatted.
 * @param {string} volumeId
 */
chrome.fileManagerPrivate.formatVolume = function(volumeId) {};

/**
 * Retrieves file manager preferences. |callback|
 * @param {Function} callback
 */
chrome.fileManagerPrivate.getPreferences = function(callback) {};

/**
 * Sets file manager preferences. |changeInfo|
 * @param {PreferencesChange} changeInfo
 */
chrome.fileManagerPrivate.setPreferences = function(changeInfo) {};

/**
 * Performs drive content search. |searchParams| |callback|
 * @param {SearchParams} searchParams
 * @param {Function} callback |entries| |nextFeed| ID of the feed that contains
 * next chunk of the search result.     Should be sent to the next searchDrive
 * request to perform     incremental search.
 */
chrome.fileManagerPrivate.searchDrive = function(searchParams, callback) {};

/**
 * Performs drive metadata search. |searchParams| |callback|
 * @param {SearchMetadataParams} searchParams
 * @param {Function} callback
 */
chrome.fileManagerPrivate.searchDriveMetadata = function(searchParams, callback) {};

/**
 * Search for files in the given volume, whose content hash matches the list of
 * given hashes.
 * @param {string} volumeId
 * @param {!Array<string>} hashes
 * @param {function(!Object<string, !Array<string>>)} callback
 */
chrome.fileManagerPrivate.searchFilesByHashes = function(volumeId, hashes, callback) {};

/**
 * Create a zip file for the selected files. |dirURL| URL of the directory
 * containing the selected files. |selectionUrls| URLs of the selected files.
 * The files must be under the     directory specified by dirURL. |destName|
 * Name of the destination zip file. The zip file will be created     under the
 * directory specified by dirURL. |callback|
 * @param {string} dirURL
 * @param {Array} selectionUrls
 * @param {string} destName
 * @param {Function=} callback
 */
chrome.fileManagerPrivate.zipSelection = function(dirURL, selectionUrls, destName, callback) {};

/**
 * Retrieves the state of the current drive connection. |callback|
 * @param {Function} callback
 */
chrome.fileManagerPrivate.getDriveConnectionState = function(callback) {};

/**
 * Checks whether the path name length fits in the limit of the filesystem.
 * |parent_directory_url| The URL of the parent directory entry. |name| The
 * name of the file. |callback| Called back when the check is finished.
 * @param {string} parent_directory_url
 * @param {string} name
 * @param {Function} callback |result| true if the length is in the valid
 * range, false otherwise.
 */
chrome.fileManagerPrivate.validatePathNameLength = function(parent_directory_url, name, callback) {};

/**
 * Changes the zoom factor of the Files.app. |operation| Zooming mode.
 * @param {string} operation
 */
chrome.fileManagerPrivate.zoom = function(operation) {};

/**
 * Requests a Drive API OAuth2 access token. |refresh| Whether the token should
 * be refetched instead of using the cached     one. |callback|
 * @param {boolean} refresh
 * @param {Function} callback |accessToken| OAuth2 access token, or an empty
 * string if failed to fetch.
 */
chrome.fileManagerPrivate.requestAccessToken = function(refresh, callback) {};

/**
 * Requests a Webstore API OAuth2 access token. |callback|
 * @param {Function} callback |accessToken| OAuth2 access token, or an empty
 * string if failed to fetch.
 */
chrome.fileManagerPrivate.requestWebStoreAccessToken = function(callback) {};

/**
 * Requests a share dialog url for the specified file. |url| Url for the file.
 * |callback|
 * @param {string} url
 * @param {Function} callback |url| Result url.
 */
chrome.fileManagerPrivate.getShareUrl = function(url, callback) {};

/**
 * Requests a download url to download the file contents. |url| Url for the
 * file. |callback|
 * @param {string} url
 * @param {Function} callback |url| Result url.
 */
chrome.fileManagerPrivate.getDownloadUrl = function(url, callback) {};

/**
 * Requests to share drive files. |url| URL of a file to be shared. |shareType|
 * Type of access that is getting granted.
 * @param {string} url
 * @param {string} shareType
 * @param {Function} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.requestDriveShare = function(url, shareType, callback) {};

/**
 * Requests to install a webstore item. |item_id| The id of the item to
 * install. |silentInstallation| False to show installation prompt. True not to
 * show. |callback|
 * @param {string} itemId
 * @param {boolean} silentInstallation
 * @param {Function} callback Callback that does not take arguments.
 */
chrome.fileManagerPrivate.installWebstoreItem = function(itemId, silentInstallation, callback) {};

/**
 * Obtains a list of profiles that are logged-in.
 * @param {Function} callback |profiles| List of profile information.
 * |runningProfile| ID of the profile that runs the application instance.
 * |showingProfile| ID of the profile that shows the application window.
 */
chrome.fileManagerPrivate.getProfiles = function(callback) {};

/**
 * Opens inspector window. |type| InspectionType which specifies how to open
 * inspector.
 * @param {string} type
 */
chrome.fileManagerPrivate.openInspector = function(type) {};

/**
 * Computes an MD5 checksum for the given file.
 * @param {string} fileUrl
 * @param {function(string)} callback
 */
chrome.fileManagerPrivate.computeChecksum = function(fileUrl, callback) {};

/**
 * Gets the MIME type of a file.
 * @param {string} fileUrl File url.
 * @param {function(string)} callback Callback that MIME type of the file is
 *     passed.
 */
chrome.fileManagerPrivate.getMimeType = function(fileUrl, callback) {};

/**
 * Gets a flag indicating whether user metrics reporting is enabled.
 * @param {function(boolean)} callback
 */
chrome.fileManagerPrivate.isUMAEnabled = function(callback) {};

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onMountCompleted;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onFileTransfersUpdated;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onCopyProgress;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onDirectoryChanged;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onPreferencesChanged;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onDriveConnectionStatusChanged;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onDeviceChanged;

/** @type {!ChromeEvent} */
chrome.fileManagerPrivate.onDriveSyncError;
