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
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ItemType
 */
chrome.developerPrivate.ItemType = {
  HOSTED_APP: 'hosted_app',
  PACKAGED_APP: 'packaged_app',
  LEGACY_PACKAGED_APP: 'legacy_packaged_app',
  EXTENSION: 'extension',
  THEME: 'theme',
};

/**
 * @typedef {{
 *   path: string,
 *   render_process_id: number,
 *   render_view_id: number,
 *   incognito: boolean,
 *   generatedBackgroundPage: boolean
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ItemInspectView
 */
var ItemInspectView;

/**
 * @typedef {{
 *   extension_id: string,
 *   render_process_id: (string|number),
 *   render_view_id: (string|number),
 *   incognito: boolean
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-InspectOptions
 */
var InspectOptions;

/**
 * @typedef {{
 *   message: string
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-InstallWarning
 */
var InstallWarning;

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ExtensionType
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
 * @see https://developer.chrome.com/extensions/developerPrivate#type-Location
 */
chrome.developerPrivate.Location = {
  FROM_STORE: 'FROM_STORE',
  UNPACKED: 'UNPACKED',
  THIRD_PARTY: 'THIRD_PARTY',
  UNKNOWN: 'UNKNOWN',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ViewType
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
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ErrorType
 */
chrome.developerPrivate.ErrorType = {
  MANIFEST: 'MANIFEST',
  RUNTIME: 'RUNTIME',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ErrorLevel
 */
chrome.developerPrivate.ErrorLevel = {
  LOG: 'LOG',
  WARN: 'WARN',
  ERROR: 'ERROR',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ExtensionState
 */
chrome.developerPrivate.ExtensionState = {
  ENABLED: 'ENABLED',
  DISABLED: 'DISABLED',
  TERMINATED: 'TERMINATED',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-CommandScope
 */
chrome.developerPrivate.CommandScope = {
  GLOBAL: 'GLOBAL',
  CHROME: 'CHROME',
};

/**
 * @typedef {{
 *   isEnabled: boolean,
 *   isActive: boolean
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-AccessModifier
 */
var AccessModifier;

/**
 * @typedef {{
 *   lineNumber: number,
 *   columnNumber: number,
 *   url: string,
 *   functionName: string
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-StackFrame
 */
var StackFrame;

/**
 * @typedef {{
 *   type: !chrome.developerPrivate.ErrorType,
 *   extensionId: string,
 *   fromIncognito: boolean,
 *   source: string,
 *   message: string,
 *   id: number,
 *   manifestKey: string,
 *   manifestSpecific: (string|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ManifestError
 */
var ManifestError;

/**
 * @typedef {{
 *   type: !chrome.developerPrivate.ErrorType,
 *   extensionId: string,
 *   fromIncognito: boolean,
 *   source: string,
 *   message: string,
 *   id: number,
 *   severity: !chrome.developerPrivate.ErrorLevel,
 *   contextUrl: string,
 *   occurrences: number,
 *   renderViewId: number,
 *   renderProcessId: number,
 *   canInspect: boolean,
 *   stackTrace: !Array<StackFrame>
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-RuntimeError
 */
var RuntimeError;

/**
 * @typedef {{
 *   suspiciousInstall: boolean,
 *   corruptInstall: boolean,
 *   updateRequired: boolean
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-DisableReasons
 */
var DisableReasons;

/**
 * @typedef {{
 *   openInTab: boolean,
 *   url: string
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-OptionsPage
 */
var OptionsPage;

/**
 * @typedef {{
 *   url: string,
 *   specified: boolean
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-HomePage
 */
var HomePage;

/**
 * @typedef {{
 *   url: string,
 *   renderProcessId: number,
 *   renderViewId: number,
 *   incognito: boolean,
 *   isIframe: boolean,
 *   type: !chrome.developerPrivate.ViewType
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ExtensionView
 */
var ExtensionView;

/**
 * @enum {string}
 */
chrome.developerPrivate.ControllerType = {
  POLICY: 'POLICY',
  CHILD_CUSTODIAN: 'CHILD_CUSTODIAN',
  SUPERVISED_USER_CUSTODIAN: 'SUPERVISED_USER_CUSTODIAN',
};

/**
 * @typedef {{
 *   type: !chrome.developerPrivate.ControllerType,
 *   text: string
 * }}
 */
var ControlledInfo;

/**
 * @typedef {{
 *   description: string,
 *   keybinding: string,
 *   name: string,
 *   isActive: boolean,
 *   scope: !chrome.developerPrivate.CommandScope,
 *   isExtensionAction: boolean
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-Command
 */
var Command;

/**
 * @typedef {{
 *   actionButtonHidden: boolean,
 *   blacklistText: (string|undefined),
 *   commands: !Array<Command>,
 *   controlledInfo: (ControlledInfo|undefined),
 *   dependentExtensions: !Array<string>,
 *   description: string,
 *   disableReasons: DisableReasons,
 *   errorCollection: AccessModifier,
 *   fileAccess: AccessModifier,
 *   homePage: HomePage,
 *   iconUrl: string,
 *   id: string,
 *   incognitoAccess: AccessModifier,
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
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ExtensionInfo
 */
var ExtensionInfo;

/**
 * @typedef {{
 *   appInfoDialogEnabled: boolean,
 *   canLoadUnpacked: boolean,
 *   inDeveloperMode: boolean,
 *   isIncognitoAvailable: boolean,
 *   isSupervised: boolean
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ProfileInfo
 */
var ProfileInfo;

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
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ItemInfo
 */
var ItemInfo;

/**
 * @typedef {{
 *   includeDisabled: (boolean|undefined),
 *   includeTerminated: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-GetExtensionsInfoOptions
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
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ExtensionConfigurationUpdate
 */
var ExtensionConfigurationUpdate;

/**
 * @typedef {{
 *   inDeveloperMode: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ProfileConfigurationUpdate
 */
var ProfileConfigurationUpdate;

/**
 * @typedef {{
 *   extensionId: string,
 *   commandName: string,
 *   scope: (!chrome.developerPrivate.CommandScope|undefined),
 *   keybinding: (string|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ExtensionCommandUpdate
 */
var ExtensionCommandUpdate;

/**
 * @typedef {{
 *   failQuietly: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ReloadOptions
 */
var ReloadOptions;

/**
 * @typedef {{
 *   failQuietly: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-LoadUnpackedOptions
 */
var LoadUnpackedOptions;

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-PackStatus
 */
chrome.developerPrivate.PackStatus = {
  SUCCESS: 'SUCCESS',
  ERROR: 'ERROR',
  WARNING: 'WARNING',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-FileType
 */
chrome.developerPrivate.FileType = {
  LOAD: 'LOAD',
  PEM: 'PEM',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-SelectType
 */
chrome.developerPrivate.SelectType = {
  FILE: 'FILE',
  FOLDER: 'FOLDER',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-EventType
 */
chrome.developerPrivate.EventType = {
  INSTALLED: 'INSTALLED',
  UNINSTALLED: 'UNINSTALLED',
  LOADED: 'LOADED',
  UNLOADED: 'UNLOADED',
  VIEW_REGISTERED: 'VIEW_REGISTERED',
  VIEW_UNREGISTERED: 'VIEW_UNREGISTERED',
  ERROR_ADDED: 'ERROR_ADDED',
  ERRORS_REMOVED: 'ERRORS_REMOVED',
  PREFS_CHANGED: 'PREFS_CHANGED',
  WARNINGS_CHANGED: 'WARNINGS_CHANGED',
};

/**
 * @typedef {{
 *   message: string,
 *   item_path: string,
 *   pem_path: string,
 *   override_flags: number,
 *   status: !chrome.developerPrivate.PackStatus
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-PackDirectoryResponse
 */
var PackDirectoryResponse;

/**
 * @typedef {{
 *   name: string
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-ProjectInfo
 */
var ProjectInfo;

/**
 * @typedef {{
 *   event_type: !chrome.developerPrivate.EventType,
 *   item_id: string,
 *   extensionInfo: (ExtensionInfo|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-EventData
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
 * @see https://developer.chrome.com/extensions/developerPrivate#type-RequestFileSourceProperties
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
 * @see https://developer.chrome.com/extensions/developerPrivate#type-RequestFileSourceResponse
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
 * @see https://developer.chrome.com/extensions/developerPrivate#type-OpenDevToolsProperties
 */
var OpenDevToolsProperties;

/**
 * @typedef {{
 *   extensionId: string,
 *   errorIds: (!Array<number>|undefined),
 *   type: (!chrome.developerPrivate.ErrorType|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/developerPrivate#type-DeleteExtensionErrorsProperties
 */
var DeleteExtensionErrorsProperties;

/**
 * Runs auto update for extensions and apps immediately.
 * @param {function(boolean):void=} callback Called with the boolean result,
 *     true if autoUpdate is successful.
 * @see https://developer.chrome.com/extensions/developerPrivate#method-autoUpdate
 */
chrome.developerPrivate.autoUpdate = function(callback) {};

/**
 * Returns information of all the extensions and apps installed.
 * @param {GetExtensionsInfoOptions=} options Options to restrict the items
 *     returned.
 * @param {function(!Array<ExtensionInfo>):void=} callback Called with
 *     extensions info.
 * @see https://developer.chrome.com/extensions/developerPrivate#method-getExtensionsInfo
 */
chrome.developerPrivate.getExtensionsInfo = function(options, callback) {};

/**
 * Returns information of a particular extension.
 * @param {string} id The id of the extension.
 * @param {function(ExtensionInfo):void=} callback Called with the result.
 * @see https://developer.chrome.com/extensions/developerPrivate#method-getExtensionInfo
 */
chrome.developerPrivate.getExtensionInfo = function(id, callback) {};

/**
 * Returns information of all the extensions and apps installed.
 * @param {boolean} includeDisabled include disabled items.
 * @param {boolean} includeTerminated include terminated items.
 * @param {function(!Array<ItemInfo>):void} callback Called with items info.
 * @deprecated Use getExtensionsInfo
 * @see https://developer.chrome.com/extensions/developerPrivate#method-getItemsInfo
 */
chrome.developerPrivate.getItemsInfo = function(includeDisabled, includeTerminated, callback) {};

/**
 * Returns the current profile's configuration.
 * @param {function(ProfileInfo):void} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-getProfileConfiguration
 */
chrome.developerPrivate.getProfileConfiguration = function(callback) {};

/**
 * Updates the active profile.
 * @param {ProfileConfigurationUpdate} update The parameters for updating the
 *     profile's configuration.  Any     properties omitted from |update| will
 *     not be changed.
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-updateProfileConfiguration
 */
chrome.developerPrivate.updateProfileConfiguration = function(update, callback) {};

/**
 * Opens a permissions dialog.
 * @param {string} extensionId The id of the extension to show permissions for.
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-showPermissionsDialog
 */
chrome.developerPrivate.showPermissionsDialog = function(extensionId, callback) {};

/**
 * Reloads a given extension.
 * @param {string} extensionId The id of the extension to reload.
 * @param {ReloadOptions=} options Additional configuration parameters.
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-reload
 */
chrome.developerPrivate.reload = function(extensionId, options, callback) {};

/**
 * Modifies an extension's current configuration.
 * @param {ExtensionConfigurationUpdate} update The parameters for updating the
 *     extension's configuration.     Any properties omitted from |update| will
 *     not be changed.
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-updateExtensionConfiguration
 */
chrome.developerPrivate.updateExtensionConfiguration = function(update, callback) {};

/**
 * Loads a user-selected unpacked item.
 * @param {LoadUnpackedOptions=} options Additional configuration parameters.
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-loadUnpacked
 */
chrome.developerPrivate.loadUnpacked = function(options, callback) {};

/**
 * Loads an extension / app.
 * @param {Object} directory The directory to load the extension from.
 * @param {function(string):void} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-loadDirectory
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
 * @see https://developer.chrome.com/extensions/developerPrivate#method-choosePath
 */
chrome.developerPrivate.choosePath = function(selectType, fileType, callback) {};

/**
 * Pack an extension.
 * @param {string} path
 * @param {string=} privateKeyPath The path of the private key, if one is given.
 * @param {number=} flags Special flags to apply to the loading process, if any.
 * @param {function(PackDirectoryResponse):void=} callback called with the
 *     success result string.
 * @see https://developer.chrome.com/extensions/developerPrivate#method-packDirectory
 */
chrome.developerPrivate.packDirectory = function(path, privateKeyPath, flags, callback) {};

/**
 * Returns true if the profile is managed.
 * @param {function(boolean):void} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-isProfileManaged
 */
chrome.developerPrivate.isProfileManaged = function(callback) {};

/**
 * Reads and returns the contents of a file related to an extension which caused
 * an error.
 * @param {RequestFileSourceProperties} properties
 * @param {function(RequestFileSourceResponse):void} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-requestFileSource
 */
chrome.developerPrivate.requestFileSource = function(properties, callback) {};

/**
 * Open the developer tools to focus on a particular error.
 * @param {OpenDevToolsProperties} properties
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-openDevTools
 */
chrome.developerPrivate.openDevTools = function(properties, callback) {};

/**
 * Delete reported extension erors.
 * @param {DeleteExtensionErrorsProperties} properties The properties specifying
 *     the errors to remove.
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-deleteExtensionErrors
 */
chrome.developerPrivate.deleteExtensionErrors = function(properties, callback) {};

/**
 * Repairs the extension specified.
 * @param {string} extensionId The id of the extension to repair.
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-repairExtension
 */
chrome.developerPrivate.repairExtension = function(extensionId, callback) {};

/**
 * Shows the options page for the extension specified.
 * @param {string} extensionId The id of the extension to show the options page
 *     for.
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-showOptions
 */
chrome.developerPrivate.showOptions = function(extensionId, callback) {};

/**
 * Shows the path of the extension specified.
 * @param {string} extensionId The id of the extension to show the path for.
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-showPath
 */
chrome.developerPrivate.showPath = function(extensionId, callback) {};

/**
 * (Un)suspends global shortcut handling.
 * @param {boolean} isSuspended Whether or not shortcut handling should be
 *     suspended.
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-setShortcutHandlingSuspended
 */
chrome.developerPrivate.setShortcutHandlingSuspended = function(isSuspended, callback) {};

/**
 * Updates an extension command.
 * @param {ExtensionCommandUpdate} update The parameters for updating the
 *     extension command.
 * @param {function():void=} callback
 * @see https://developer.chrome.com/extensions/developerPrivate#method-updateExtensionCommand
 */
chrome.developerPrivate.updateExtensionCommand = function(update, callback) {};

/**
 * @param {string} id
 * @param {boolean} enabled
 * @param {function():void=} callback
 * @deprecated Use management.setEnabled
 * @see https://developer.chrome.com/extensions/developerPrivate#method-enable
 */
chrome.developerPrivate.enable = function(id, enabled, callback) {};

/**
 * @param {string} extensionId
 * @param {boolean} allow
 * @param {function():void=} callback
 * @deprecated Use updateExtensionConfiguration
 * @see https://developer.chrome.com/extensions/developerPrivate#method-allowIncognito
 */
chrome.developerPrivate.allowIncognito = function(extensionId, allow, callback) {};

/**
 * @param {string} extensionId
 * @param {boolean} allow
 * @param {function():void=} callback
 * @deprecated Use updateExtensionConfiguration
 * @see https://developer.chrome.com/extensions/developerPrivate#method-allowFileAccess
 */
chrome.developerPrivate.allowFileAccess = function(extensionId, allow, callback) {};

/**
 * @param {InspectOptions} options
 * @param {function():void=} callback
 * @deprecated Use openDevTools
 * @see https://developer.chrome.com/extensions/developerPrivate#method-inspect
 */
chrome.developerPrivate.inspect = function(options, callback) {};

/**
 * Fired when a item state is changed.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/developerPrivate#event-onItemStateChanged
 */
chrome.developerPrivate.onItemStateChanged;

/**
 * Fired when the profile's state has changed.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/developerPrivate#event-onProfileStateChanged
 */
chrome.developerPrivate.onProfileStateChanged;
