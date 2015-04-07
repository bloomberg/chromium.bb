// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: developerPrivate */

/**
 * @const
 */
chrome.developerPrivate = {};

/**
 * @enum {string}
 */
chrome.developerPrivate.ItemType = {
  hosted_app: 'hosted_app',
  packaged_app: 'packaged_app',
  legacy_packaged_app: 'legacy_packaged_app',
  extension: 'extension',
  theme: 'theme',
};

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
 *   extension_id: string,
 *   render_process_id: (string|number),
 *   render_view_id: (string|number),
 *   incognito: boolean
 * }}
 */
var InspectOptions;

/**
 * @typedef {{
 *   message: string
 * }}
 */
var InstallWarning;

/**
 * @enum {string}
 */
chrome.developerPrivate.ExtensionType = {
  HOSTED_APP: 'HOSTED_APP',
  PLATFORM_APP: 'PLATFORM_APP',
  LEGACY_PACKAGED_APP: 'LEGACY_PACKAGED_APP',
  EXTENSION: 'EXTENSION',
  THEME: 'THEME',
  SHARED_MODULE: 'SHARED_MODULE',
};

/**
 * @enum {string}
 */
chrome.developerPrivate.Location = {
  FROM_STORE: 'FROM_STORE',
  UNPACKED: 'UNPACKED',
  THIRD_PARTY: 'THIRD_PARTY',
  UNKNOWN: 'UNKNOWN',
};

/**
 * @enum {string}
 */
chrome.developerPrivate.ViewType = {
  APP_WINDOW: 'APP_WINDOW',
  BACKGROUND_CONTENTS: 'BACKGROUND_CONTENTS',
  EXTENSION_BACKGROUND_PAGE: 'EXTENSION_BACKGROUND_PAGE',
  EXTENSION_DIALOG: 'EXTENSION_DIALOG',
  EXTENSION_POPUP: 'EXTENSION_POPUP',
  LAUNCHER_PAGE: 'LAUNCHER_PAGE',
  PANEL: 'PANEL',
  TAB_CONTENTS: 'TAB_CONTENTS',
  VIRTUAL_KEYBOARD: 'VIRTUAL_KEYBOARD',
};

/**
 * @enum {string}
 */
chrome.developerPrivate.ErrorType = {
  MANIFEST: 'MANIFEST',
  RUNTIME: 'RUNTIME',
};

/**
 * @enum {string}
 */
chrome.developerPrivate.ErrorLevel = {
  LOG: 'LOG',
  WARN: 'WARN',
  ERROR: 'ERROR',
};

/**
 * @enum {string}
 */
chrome.developerPrivate.ExtensionState = {
  ENABLED: 'ENABLED',
  DISABLED: 'DISABLED',
  TERMINATED: 'TERMINATED',
};

/**
 * @typedef {{
 *   isEnabled: boolean,
 *   isActive: boolean
 * }}
 */
var AccessModifier;

/**
 * @typedef {{
 *   lineNumber: number,
 *   columnNumber: number,
 *   url: string,
 *   functionName: string
 * }}
 */
var StackFrame;

/**
 * @typedef {{
 *   type: !chrome.developerPrivate.ErrorType,
 *   extensionId: string,
 *   fromIncognito: boolean,
 *   source: string,
 *   message: string,
 *   manifestKey: string,
 *   manifestSpecific: (string|undefined)
 * }}
 */
var ManifestError;

/**
 * @typedef {{
 *   type: !chrome.developerPrivate.ErrorType,
 *   extensionId: string,
 *   fromIncognito: boolean,
 *   source: string,
 *   message: string,
 *   severity: !chrome.developerPrivate.ErrorLevel,
 *   contextUrl: string,
 *   occurrences: number,
 *   renderViewId: number,
 *   renderProcessId: number,
 *   canInspect: boolean,
 *   stackTrace: !Array<StackFrame>
 * }}
 */
var RuntimeError;

/**
 * @typedef {{
 *   suspiciousInstall: boolean,
 *   corruptInstall: boolean,
 *   updateRequired: boolean
 * }}
 */
var DisableReasons;

/**
 * @typedef {{
 *   openInTab: boolean,
 *   url: string
 * }}
 */
var OptionsPage;

/**
 * @typedef {{
 *   url: string,
 *   specified: boolean
 * }}
 */
var HomePage;

/**
 * @typedef {{
 *   url: string,
 *   renderProcessId: number,
 *   renderViewId: number,
 *   incognito: boolean,
 *   type: !chrome.developerPrivate.ViewType
 * }}
 */
var ExtensionView;

/**
 * @typedef {{
 *   actionButtonHidden: boolean,
 *   blacklistText: (string|undefined),
 *   dependentExtensions: !Array<string>,
 *   description: string,
 *   disableReasons: DisableReasons,
 *   errorCollection: AccessModifier,
 *   fileAccess: AccessModifier,
 *   homePage: HomePage,
 *   iconUrl: string,
 *   id: string,
 *   incognitoAccess: AccessModifier,
 *   installedByCustodian: boolean,
 *   installWarnings: !Array<string>,
 *   launchUrl: (string|undefined),
 *   location: !chrome.developerPrivate.Location,
 *   locationText: (string|undefined),
 *   manifestErrors: !Array<ManifestError>,
 *   mustRemainInstalled: boolean,
 *   name: string,
 *   offlineEnabled: boolean,
 *   optionsPage: (OptionsPage|undefined),
 *   path: (string|undefined),
 *   policyText: (string|undefined),
 *   prettifiedPath: (string|undefined),
 *   runOnAllUrls: AccessModifier,
 *   runtimeErrors: !Array<RuntimeError>,
 *   runtimeWarnings: !Array<string>,
 *   state: !chrome.developerPrivate.ExtensionState,
 *   type: !chrome.developerPrivate.ExtensionType,
 *   updateUrl: string,
 *   userMayModify: boolean,
 *   version: string,
 *   views: !Array<ExtensionView>
 * }}
 */
var ExtensionInfo;

/**
 * @typedef {{
 *   id: string,
 *   name: string,
 *   version: string,
 *   description: string,
 *   may_disable: boolean,
 *   enabled: boolean,
 *   isApp: boolean,
 *   type: !chrome.developerPrivate.ItemType,
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
 *   install_warnings: !Array<InstallWarning>,
 *   manifest_errors: !Array<*>,
 *   runtime_errors: !Array<*>,
 *   offline_enabled: boolean,
 *   views: !Array<ItemInspectView>
 * }}
 */
var ItemInfo;

/**
 * @typedef {{
 *   includeDisabled: (boolean|undefined),
 *   includeTerminated: (boolean|undefined)
 * }}
 */
var GetExtensionsInfoOptions;

/**
 * @typedef {{
 *   extensionId: string,
 *   fileAccess: (boolean|undefined),
 *   incognitoAccess: (boolean|undefined),
 *   errorCollection: (boolean|undefined),
 *   runOnAllUrls: (boolean|undefined),
 *   showActionButton: (boolean|undefined)
 * }}
 */
var ExtensionConfigurationUpdate;

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
  VIEW_REGISTERED: 'VIEW_REGISTERED',
  VIEW_UNREGISTERED: 'VIEW_UNREGISTERED',
  ERROR_ADDED: 'ERROR_ADDED',
};

/**
 * @typedef {{
 *   message: string,
 *   item_path: string,
 *   pem_path: string,
 *   override_flags: number,
 *   status: !chrome.developerPrivate.PackStatus
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
 *   event_type: !chrome.developerPrivate.EventType,
 *   item_id: string,
 *   extensionInfo: (ExtensionInfo|undefined)
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
 *   extensionId: (string|undefined),
 *   renderViewId: number,
 *   renderProcessId: number,
 *   incognito: (boolean|undefined),
 *   url: (string|undefined),
 *   lineNumber: (number|undefined),
 *   columnNumber: (number|undefined)
 * }}
 */
var OpenDevToolsProperties;

/**
 * @typedef {{
 *   extensionId: string,
 *   errorIds: (!Array<number>|undefined),
 *   type: (!chrome.developerPrivate.ErrorType|undefined)
 * }}
 */
var DeleteExtensionErrorsProperties;

/**
 * Runs auto update for extensions and apps immediately.
 * @param {function(boolean):void=} callback Called with the boolean result,
 *     true if autoUpdate is successful.
 */
chrome.developerPrivate.autoUpdate = function(callback) {};

/**
 * Returns information of all the extensions and apps installed.
 * @param {GetExtensionsInfoOptions=} options Options to restrict the items
 *     returned.
 * @param {function(!Array<ExtensionInfo>):void=} callback Called with
 *     extensions info.
 */
chrome.developerPrivate.getExtensionsInfo = function(options, callback) {};

/**
 * Returns information of a particular extension.
 * @param {string} id The id of the extension.
 * @param {function(ExtensionInfo):void=} callback Called with the result.
 */
chrome.developerPrivate.getExtensionInfo = function(id, callback) {};

/**
 * Returns information of all the extensions and apps installed.
 * @param {boolean} includeDisabled include disabled items.
 * @param {boolean} includeTerminated include terminated items.
 * @param {function(!Array<ItemInfo>):void} callback Called with items info.
 * @deprecated Use getExtensionsInfo
 */
chrome.developerPrivate.getItemsInfo = function(includeDisabled, includeTerminated, callback) {};

/**
 * Opens a permissions dialog.
 * @param {string} extensionId The id of the extension to show permissions for.
 * @param {function():void=} callback
 */
chrome.developerPrivate.showPermissionsDialog = function(extensionId, callback) {};

/**
 * Reloads a given extension.
 * @param {string} extensionId The id of the extension to reload.
 * @param {ReloadOptions=} options Additional configuration parameters.
 * @param {function():void=} callback
 */
chrome.developerPrivate.reload = function(extensionId, options, callback) {};

/**
 * Modifies an extension's current configuration.
 * @param {ExtensionConfigurationUpdate} update The parameters for updating the
 *     extension's configuration.     Any properties omitted from |update| will
 *     not be changed.
 * @param {function():void=} callback
 */
chrome.developerPrivate.updateExtensionConfiguration = function(update, callback) {};

/**
 * Loads a user-selected unpacked item.
 * @param {LoadUnpackedOptions=} options Additional configuration parameters.
 * @param {function():void=} callback
 */
chrome.developerPrivate.loadUnpacked = function(options, callback) {};

/**
 * Loads an extension / app.
 * @param {Object} directory The directory to load the extension from.
 * @param {function(string):void} callback
 */
chrome.developerPrivate.loadDirectory = function(directory, callback) {};

/**
 * Open Dialog to browse to an entry.
 * @param {!chrome.developerPrivate.SelectType} selectType Select a file or a
 *     folder.
 * @param {!chrome.developerPrivate.FileType} fileType Required file type. For
 *     example, pem type is for private key and load type is for an unpacked
 *     item.
 * @param {function(string):void} callback called with selected item's path.
 */
chrome.developerPrivate.choosePath = function(selectType, fileType, callback) {};

/**
 * Pack an extension.
 * @param {string} path
 * @param {string=} privateKeyPath The path of the private key, if one is given.
 * @param {number=} flags Special flags to apply to the loading process, if any.
 * @param {function(PackDirectoryResponse):void=} callback called with the
 *     success result string.
 */
chrome.developerPrivate.packDirectory = function(path, privateKeyPath, flags, callback) {};

/**
 * Returns true if the profile is managed.
 * @param {function(boolean):void} callback
 */
chrome.developerPrivate.isProfileManaged = function(callback) {};

/**
 * Reads and returns the contents of a file related to an extension which caused
 * an error.
 * @param {RequestFileSourceProperties} properties
 * @param {function(RequestFileSourceResponse):void} callback
 */
chrome.developerPrivate.requestFileSource = function(properties, callback) {};

/**
 * Open the developer tools to focus on a particular error.
 * @param {OpenDevToolsProperties} properties
 * @param {function():void=} callback
 */
chrome.developerPrivate.openDevTools = function(properties, callback) {};

/**
 * Delete reported extension erors.
 * @param {DeleteExtensionErrorsProperties} properties
 * @param {function():void=} callback
 */
chrome.developerPrivate.deleteExtensionErrors = function(properties, callback) {};

/**
 * @param {string} id
 * @param {boolean} enabled
 * @param {function():void=} callback
 * @deprecated Use management.setEnabled
 */
chrome.developerPrivate.enable = function(id, enabled, callback) {};

/**
 * @param {string} extensionId
 * @param {boolean} allow
 * @param {function():void=} callback
 * @deprecated Use updateExtensionConfiguration
 */
chrome.developerPrivate.allowIncognito = function(extensionId, allow, callback) {};

/**
 * @param {string} extensionId
 * @param {boolean} allow
 * @param {function():void=} callback
 * @deprecated Use updateExtensionConfiguration
 */
chrome.developerPrivate.allowFileAccess = function(extensionId, allow, callback) {};

/**
 * @param {InspectOptions} options
 * @param {function():void=} callback
 * @deprecated Use openDevTools
 */
chrome.developerPrivate.inspect = function(options, callback) {};

/** @type {!ChromeEvent} */
chrome.developerPrivate.onItemStateChanged;
