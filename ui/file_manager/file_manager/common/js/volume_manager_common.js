// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for common types shared between VolumeManager and
 * VolumeManagerWrapper.
 */
var VolumeManagerCommon = {};

/**
 * Type of a root directory.
 * @enum {string}
 * @const
 */
VolumeManagerCommon.RootType = Object.freeze({
  // Root for a downloads directory.
  DOWNLOADS: 'downloads',

  // Root for a mounted archive volume.
  ARCHIVE: 'archive',

  // Root for a removable volume.
  REMOVABLE: 'removable',

  // Root for a drive volume.
  DRIVE: 'drive',

  // Root for a privet storage volume.
  CLOUD_DEVICE: 'cloud_device',

  // Root for a MTP volume.
  MTP: 'mtp',

  // Root for a provided volume.
  PROVIDED: 'provided',

  // Root for entries that is not located under RootType.DRIVE. e.g. shared
  // files.
  DRIVE_OTHER: 'drive_other',

  // Fake root for offline available files on the drive.
  DRIVE_OFFLINE: 'drive_offline',

  // Fake root for shared files on the drive.
  DRIVE_SHARED_WITH_ME: 'drive_shared_with_me',

  // Fake root for recent files on the drive.
  DRIVE_RECENT: 'drive_recent'
});

/**
 * Error type of VolumeManager.
 * @enum {string}
 * @const
 */
VolumeManagerCommon.VolumeError = Object.freeze({
  /* Internal errors */
  TIMEOUT: 'timeout',

  /* System events */
  UNKNOWN: 'error_unknown',
  INTERNAL: 'error_internal',
  INVALID_ARGUMENT: 'error_invalid_argument',
  INVALID_PATH: 'error_invalid_path',
  ALREADY_MOUNTED: 'error_path_already_mounted',
  PATH_NOT_MOUNTED: 'error_path_not_mounted',
  DIRECTORY_CREATION_FAILED: 'error_directory_creation_failed',
  INVALID_MOUNT_OPTIONS: 'error_invalid_mount_options',
  INVALID_UNMOUNT_OPTIONS: 'error_invalid_unmount_options',
  INSUFFICIENT_PERMISSIONS: 'error_insufficient_permissions',
  MOUNT_PROGRAM_NOT_FOUND: 'error_mount_program_not_found',
  MOUNT_PROGRAM_FAILED: 'error_mount_program_failed',
  INVALID_DEVICE_PATH: 'error_invalid_device_path',
  UNKNOWN_FILESYSTEM: 'error_unknown_filesystem',
  UNSUPPORTED_FILESYSTEM: 'error_unsupported_filesystem',
  INVALID_ARCHIVE: 'error_invalid_archive',
  AUTHENTICATION: 'error_authentication',
  PATH_UNMOUNTED: 'error_path_unmounted'
});

/**
 * List of connection types of drive.
 *
 * Keep this in sync with the kDriveConnectionType* constants in
 * private_api_dirve.cc.
 *
 * @enum {string}
 * @const
 */
VolumeManagerCommon.DriveConnectionType = Object.freeze({
  OFFLINE: 'offline',  // Connection is offline or drive is unavailable.
  METERED: 'metered',  // Connection is metered. Should limit traffic.
  ONLINE: 'online'     // Connection is online.
});

/**
 * List of reasons of DriveConnectionType.
 *
 * Keep this in sync with the kDriveConnectionReason constants in
 * private_api_drive.cc.
 *
 * @enum {string}
 * @const
 */
VolumeManagerCommon.DriveConnectionReason = Object.freeze({
  NOT_READY: 'not_ready',    // Drive is not ready or authentication is failed.
  NO_NETWORK: 'no_network',  // Network connection is unavailable.
  NO_SERVICE: 'no_service'   // Drive service is unavailable.
});

/**
 * The type of each volume.
 * @enum {string}
 * @const
 */
VolumeManagerCommon.VolumeType = Object.freeze({
  DRIVE: 'drive',
  DOWNLOADS: 'downloads',
  REMOVABLE: 'removable',
  ARCHIVE: 'archive',
  CLOUD_DEVICE: 'cloud_device',
  MTP: 'mtp',
  PROVIDED: 'provided'
});
