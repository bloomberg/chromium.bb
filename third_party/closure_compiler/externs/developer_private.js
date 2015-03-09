// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: developerPrivate */

// Note: hand-modified to change Array to !Array in ItemInfo typedef, and add
// enum definitions.

/**
 * @typedef {string}
 */
var ItemType;

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
 *   render_process_id: (string|number),
 *   render_view_id: (string|number),
 *   incognito: boolean
 * }}
 */
var InspectOptions;

/**
 * @typedef {{
 *   failQuietly: (boolean|undefined)
 * }}
 */
var ReloadOptions;

/**
 * @typedef {{
 *   failQuietly: (boolean|undefined)
 * }}
 */
var LoadUnpackedOptions;

/**
 * @enum {string}
 */
chrome.developerPrivate.PackStatus = {
  SUCCESS: 'SUCCESS',
  ERROR: 'ERROR',
  WARNING: 'WARNING',
};

/**
 * @enum {string}
 */
chrome.developerPrivate.FileType = {
  LOAD: 'LOAD',
  PEM: 'PEM',
};

/**
 * @enum {string}
 */
chrome.developerPrivate.SelectType = {
  FILE: 'FILE',
  FOLDER: 'FOLDER',
};

/**
 * @enum {string}
 */
chrome.developerPrivate.EventType = {
  INSTALLED: 'INSTALLED',
  UNINSTALLED: 'UNINSTALLED',
  LOADED: 'LOADED',
  UNLOADED: 'UNLOADED',
  // New window / view opened.
  VIEW_REGISTERED: 'VIEW_REGISTERED',
  // window / view closed.
  VIEW_UNREGISTERED: 'VIEW_UNREGISTERED',
  ERROR_ADDED: 'ERROR_ADDED',
}

/**
 * @typedef {{
 *   message: string,
 *   item_path: string,
 *   pem_path: string,
 *   override_flags: number,
 *   status: chrome.developerPrivate.PackStatus
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
 *   event_type: chrome.developerPrivate.EventType,
 *   item_id: string
 * }}
 */
var EventData;

/**
 * @typedef {{
 *   extensionId: string,
 *   pathSuffix: string,
 *   message: string,
 *   manifestKey: (string|undefined),
 *   manifestSpecific: (string|undefined),
 *   lineNumber: (number|undefined)
 * }}
 */
var RequestFileSourceProperties;

/**
 * @typedef {{
 *   highlight: string,
 *   beforeHighlight: string,
 *   afterHighlight: string,
 *   title: string,
 *   message: string
 * }}
 */
var RequestFileSourceResponse;

/**
 * @typedef {{
 *   renderViewId: number,
 *   renderProcessId: number,
 *   url: (string|undefined),
 *   lineNumber: (number|undefined),
 *   columnNumber: (number|undefined)
 * }}
 */
var OpenDevToolsProperties;

/**
 * @const
 */
chrome.developerPrivate = {};

/**
 * Runs auto update for extensions and apps immediately.
 * @param {Function=} callback Called with the boolean result, true if
 * autoUpdate is successful.
 */
chrome.developerPrivate.autoUpdate = function(callback) {};

/**
 * Returns information of all the extensions and apps installed.
 * @param {boolean} includeDisabled include disabled items.
 * @param {boolean} includeTerminated include terminated items.
 * @param {Function} callback Called with items info.
 */
chrome.developerPrivate.getItemsInfo = function(includeDisabled, includeTerminated, callback) {};

/**
 * Opens a permissions dialog.
 * @param {string} extensionId The id of the extension to show permissions for.
 * @param {Function=} callback
 */
chrome.developerPrivate.showPermissionsDialog = function(extensionId, callback) {};

/**
 * Opens a developer tools inspection window.
 * @param {InspectOptions} options The details about the inspection.
 * @param {Function=} callback
 */
chrome.developerPrivate.inspect = function(options, callback) {};

/**
 * Enables / Disables file access for an extension.
 * @param {string} extensionId The id of the extension to set file access for.
 * @param {boolean} allow Whether or not to allow file access for the
 * extension.
 * @param {Function=} callback
 */
chrome.developerPrivate.allowFileAccess = function(extensionId, allow, callback) {};

/**
 * Reloads a given extension.
 * @param {string} extensionId The id of the extension to reload.
 * @param {ReloadOptions=} options Additional configuration parameters.
 * @param {Function=} callback
 */
chrome.developerPrivate.reload = function(extensionId, options, callback) {};

/**
 * Enables / Disables a given extension.
 * @param {string} extensionId The id of the extension to enable/disable.
 * @param {boolean} enable Whether the extension should be enabled.
 * @param {Function=} callback
 */
chrome.developerPrivate.enable = function(extensionId, enable, callback) {};

/**
 * Allows / Disallows an extension to run in incognito mode.
 * @param {string} extensionId The id of the extension.
 * @param {boolean} allow Whether or not the extension should be allowed
 * incognito.
 * @param {Function=} callback
 */
chrome.developerPrivate.allowIncognito = function(extensionId, allow, callback) {};

/**
 * Loads a user-selected unpacked item.
 * @param {LoadUnpackedOptions=} options Additional configuration parameters.
 * @param {Function=} callback
 */
chrome.developerPrivate.loadUnpacked = function(options, callback) {};

/**
 * Loads an extension / app.
 * @param {Object} directory The directory to load the extension from.
 * @param {Function} callback
 */
chrome.developerPrivate.loadDirectory = function(directory, callback) {};

/**
 * Open Dialog to browse to an entry.
 * @param {chrome.developerPrivate.SelectType} selectType
 *     Select a file or a folder.
 * @param {chrome.developerPrivate.FileType} fileType
 *     Required file type. For example, pem type is for
 * private key and load type is for an unpacked item.
 * @param {Function} callback called with selected item's path.
 */
chrome.developerPrivate.choosePath = function(selectType, fileType, callback) {};

/**
 * Pack an extension.
 * @param {string} path
 * @param {string=} privateKeyPath The path of the private key, if one is
 * given.
 * @param {number=} flags Special flags to apply to the loading process, if
 * any.
 * @param {Function=} callback called with the success result string.
 */
chrome.developerPrivate.packDirectory = function(path, privateKeyPath, flags, callback) {};

/**
 * Returns true if the profile is managed.
 * @param {Function} callback
 */
chrome.developerPrivate.isProfileManaged = function(callback) {};

/**
 * Reads and returns the contents of a file related to an extension which
 * caused an error.
 * @param {RequestFileSourceProperties} properties
 * @param {Function} callback
 */
chrome.developerPrivate.requestFileSource = function(properties, callback) {};

/**
 * Open the developer tools to focus on a particular error.
 * @param {OpenDevToolsProperties} properties
 */
chrome.developerPrivate.openDevTools = function(properties) {};

/** @type {!ChromeEvent} */
chrome.developerPrivate.onItemStateChanged;
