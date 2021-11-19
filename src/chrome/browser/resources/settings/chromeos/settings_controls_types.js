// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Externs for files shared with Browser Settings and have been migrated to
 * TypeScript. Remove if ever CrOS Settings is migrated to TS.
 * @externs
 */

/** @interface */
function PrefControlMixinInterface() {}

/** @type {!chrome.settingsPrivate.PrefObject|undefined} */
PrefControlMixinInterface.prototype.pref;

/**
 * @interface
 * @extends {PrefControlMixinInterface}
 */
function SettingsBooleanControlMixinInterface() {}

/** @type {boolean} */
SettingsBooleanControlMixinInterface.prototype.checked;

/** @type {string} */
SettingsBooleanControlMixinInterface.prototype.label;

/** @return {boolean} */
SettingsBooleanControlMixinInterface.prototype.controlDisabled = function() {};

SettingsBooleanControlMixinInterface.prototype.notifyChangedByUserInteraction =
    function() {};
SettingsBooleanControlMixinInterface.prototype.resetToPrefValue = function() {};
SettingsBooleanControlMixinInterface.prototype.sendPrefChange = function() {};

/**
 * @constructor
 * @implements {SettingsBooleanControlMixinInterface}
 * @extends {HTMLElement}
 */
function SettingsToggleButtonElement() {}


/**
 * @typedef {{
 *   name: string,
 *   value: (number|string)
 * }}
 */
let DropdownMenuOption;

/**
 * @typedef {!Array<!DropdownMenuOption>}
 */
let DropdownMenuOptionList;

/** @interface */
function SettingsPrefsElement() {}

/** @param {string} key */
SettingsPrefsElement.prototype.refresh = function(key) {};

/**
 * @constructor
 * @extends {HTMLElement}
 */
function SettingsPersonalizationOptionsElement() {}

/** @return {?HTMLElement} */
SettingsPersonalizationOptionsElement.prototype.getDriveSuggestToggle =
    function() {};

/** @return {?HTMLElement} */
SettingsPersonalizationOptionsElement.prototype.getUrlCollectionToggle =
    function() {};

/** @return {?HTMLElement} */
SettingsPersonalizationOptionsElement.prototype.getSearchSuggestToggle =
    function() {};

/**
 * @constructor
 * @extends {HTMLElement}
 */
function SettingsSyncEncryptionOptionsElement() {}

/** @return {?HTMLElement} */
SettingsSyncEncryptionOptionsElement.prototype.getEncryptionsRadioButtons =
    function() {};


/**
 * @constructor
 * @extends {HTMLElement}
 */
function SettingsSyncPageElement() {}

/** @return {?SettingsPersonalizationOptionsElement} */
SettingsSyncPageElement.prototype.getPersonalizationOptions = function() {};

/** @return {?SettingsSyncEncryptionOptionsElement} */
SettingsSyncPageElement.prototype.getEncryptionOptions = function() {};

/**
 * Must be kept in sync with the return values of getSyncErrorAction in
 * chrome/browser/ui/webui/settings/people_handler.cc
 * @enum {string}
 */
const StatusAction = {
  NO_ACTION: 'noAction',             // No action to take.
  REAUTHENTICATE: 'reauthenticate',  // User needs to reauthenticate.
  SIGNOUT_AND_SIGNIN:
      'signOutAndSignIn',               // User needs to sign out and sign in.
  UPGRADE_CLIENT: 'upgradeClient',      // User needs to upgrade the client.
  ENTER_PASSPHRASE: 'enterPassphrase',  // User needs to enter passphrase.
  // User needs to go through key retrieval.
  RETRIEVE_TRUSTED_VAULT_KEYS: 'retrieveTrustedVaultKeys',
  CONFIRM_SYNC_SETTINGS:
      'confirmSyncSettings',  // User needs to confirm sync settings.
};

/**
 * @typedef {{
 *   appsRegistered: boolean,
 *   appsSynced: boolean,
 *   autofillRegistered: boolean,
 *   autofillSynced: boolean,
 *   bookmarksRegistered: boolean,
 *   bookmarksSynced: boolean,
 *   customPassphraseAllowed: boolean,
 *   encryptAllData: boolean,
 *   explicitPassphraseTime: (string|undefined),
 *   extensionsRegistered: boolean,
 *   extensionsSynced: boolean,
 *   passphraseRequired: boolean,
 *   passwordsRegistered: boolean,
 *   passwordsSynced: boolean,
 *   paymentsIntegrationEnabled: boolean,
 *   preferencesRegistered: boolean,
 *   preferencesSynced: boolean,
 *   readingListRegistered: boolean,
 *   readingListSynced: boolean,
 *   syncAllDataTypes: boolean,
 *   tabsRegistered: boolean,
 *   tabsSynced: boolean,
 *   themesRegistered: boolean,
 *   themesSynced: boolean,
 *   trustedVaultKeysRequired: boolean,
 *   typedUrlsRegistered: boolean,
 *   typedUrlsSynced: boolean,
 *   wifiConfigurationsRegistered: boolean,
 *   wifiConfigurationsSynced: boolean,
 * }}
 */
let SyncPrefs;

/**
 * @typedef {{childUser: (boolean|undefined),
 *            disabled: (boolean|undefined),
 *            domain: (string|undefined),
 *            hasError: (boolean|undefined),
 *            hasPasswordsOnlyError: (boolean|undefined),
 *            hasUnrecoverableError: (boolean|undefined),
 *            managed: (boolean|undefined),
 *            firstSetupInProgress: (boolean|undefined),
 *            signedIn: (boolean|undefined),
 *            signedInUsername: (string|undefined),
 *            statusAction: (!StatusAction),
 *            statusActionText: (string|undefined),
 *            statusText: (string|undefined),
 *            supervisedUser: (boolean|undefined),
 *            syncSystemEnabled: (boolean|undefined)}}
 * @see chrome/browser/ui/webui/settings/people_handler.cc
 */
let SyncStatus;

/** @interface */
function SyncBrowserProxy() {}

/** @return {!Promise<!SyncStatus>} */
SyncBrowserProxy.prototype.getSyncStatus = function() {};

SyncBrowserProxy.prototype.sendSyncPrefsChanged = function() {};

/**
 * @typedef {{
 *   name: string,
 *   iconUrl: string
 * }}
 */
let ProfileInfo;

/** @interface */
function ProfileInfoBrowserProxy() {}

/** @return {!Promise<!ProfileInfo>} */
ProfileInfoBrowserProxy.prototype.getProfileInfo = function() {};
