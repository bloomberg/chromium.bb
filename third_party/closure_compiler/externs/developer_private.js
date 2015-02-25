// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: developerPrivate */

// Note: hand-modified to change Array to !Array in ItemInfo typedef.

/**
 * @typedef {{
 *   path: string,
 *   render_process_id: number,
 *   render_view_id: number,
 *   incognito: boolean,
 *   generatedBackgroundPage: boolean
 * }}
 */
var ItemInspectView;

/**
 * @typedef {{
 *   message: string
 * }}
 */
var InstallWarning;

/**
 * @typedef {{
 *   id: string,
 *   name: string,
 *   version: string,
 *   description: string,
 *   may_disable: boolean,
 *   enabled: boolean,
 *   disabled_reason: (string|undefined),
 *   isApp: boolean,
 *   type: ItemType,
 *   allow_activity: boolean,
 *   allow_file_access: boolean,
 *   wants_file_access: boolean,
 *   incognito_enabled: boolean,
 *   is_unpacked: boolean,
 *   allow_reload: boolean,
 *   terminated: boolean,
 *   allow_incognito: boolean,
 *   icon_url: string,
 *   path: (string|undefined),
 *   options_url: (string|undefined),
 *   app_launch_url: (string|undefined),
 *   homepage_url: (string|undefined),
 *   update_url: (string|undefined),
 *   install_warnings: !Array,
 *   manifest_errors: !Array,
 *   runtime_errors: !Array,
 *   offline_enabled: boolean,
 *   views: !Array
 * }}
 */
var ItemInfo;

/**
 * @typedef {{
 *   extension_id: string,
 *   render_process_id: string,
 *   render_view_id: string,
 *   incognito: boolean
 * }}
 */
var InspectOptions;

/**
 * @typedef {{
 *   message: string,
 *   item_path: string,
 *   pem_path: string,
 *   override_flags: number,
 *   status: PackStatus
 * }}
 */
var PackDirectoryResponse;

/**
 * @typedef {{
 *   name: string
 * }}
 */
var ProjectInfo;

/**
 * @typedef {{
 *   event_type: EventType,
 *   item_id: string
 * }}
 */
var EventData;

/**
 * @const
 */
chrome.developerPrivate = {};

/**
 * Runs auto update for extensions and apps immediately.
 * @param {Function} callback Called with the boolean result, true if
 * autoUpdate is successful.
 */
chrome.developerPrivate.autoUpdate = function(callback) {};

/**
 * Returns information of all the extensions and apps installed.
 * @param {boolean} include_disabled include disabled items.
 * @param {boolean} include_terminated include terminated items.
 * @param {Function} callback Called with items info.
 */
chrome.developerPrivate.getItemsInfo = function(include_disabled, include_terminated, callback) {};

/**
 * Opens a permissions dialog for given |itemId|.
 * @param {string} itemId
 * @param {Function=} callback
 */
chrome.developerPrivate.showPermissionsDialog = function(itemId, callback) {};

/**
 * Opens an inspect window for given |options|
 * @param {InspectOptions} options
 * @param {Function=} callback
 */
chrome.developerPrivate.inspect = function(options, callback) {};

/**
 * Enable / Disable file access for a given |item_id|
 * @param {string} item_id
 * @param {boolean} allow
 * @param {Function=} callback
 */
chrome.developerPrivate.allowFileAccess = function(item_id, allow, callback) {};

/**
 * Reloads a given item with |itemId|.
 * @param {string} itemId
 * @param {Function=} callback
 */
chrome.developerPrivate.reload = function(itemId, callback) {};

/**
 * Enable / Disable a given item with id |itemId|.
 * @param {string} itemId
 * @param {boolean} enable
 * @param {Function=} callback
 */
chrome.developerPrivate.enable = function(itemId, enable, callback) {};

/**
 * Allow / Disallow item with |item_id| in incognito mode.
 * @param {string} item_id
 * @param {boolean} allow
 * @param {Function} callback
 */
chrome.developerPrivate.allowIncognito = function(item_id, allow, callback) {};

/**
 * Load a user selected unpacked item
 * @param {Function=} callback
 */
chrome.developerPrivate.loadUnpacked = function(callback) {};

/**
 * Loads an extension / app from a given |directory|
 * @param {Object} directory
 * @param {Function} callback
 */
chrome.developerPrivate.loadDirectory = function(directory, callback) {};

/**
 * Open Dialog to browse to an entry.
 * @param {SelectType} select_type Select a file or a folder.
 * @param {FileType} file_type Required file type. For Example pem type is for
 * private key and load type is for an unpacked item.
 * @param {Function} callback called with selected item's path.
 */
chrome.developerPrivate.choosePath = function(select_type, file_type, callback) {};

/**
 * Pack an item with given |path| and |private_key_path|
 * @param {string} path
 * @param {string} private_key_path
 * @param {number} flags
 * @param {Function} callback called with the success result string.
 */
chrome.developerPrivate.packDirectory = function(path, private_key_path, flags, callback) {};

/**
 * Returns true if the profile is managed.
 * @param {Function} callback
 */
chrome.developerPrivate.isProfileManaged = function(callback) {};

/** @type {!ChromeEvent} */
chrome.developerPrivate.onItemStateChanged;
