// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * This file contains typedefs for chromeOS OOBE properties.
 */

/* #export */ var OobeTypes = {};

/**
 * ChromeOS OOBE language descriptor.
 * @typedef {{
 *   code: (string|undefined),
 *   displayName: (string|undefined),
 *   nativeDisplayName: (string|undefined),
 *   optionGroupName: (string|undefined),
 *   selected: boolean,
 *   textDirection: (string|undefined),
 *   title: string,
 *   value: string,
 * }}
 */
OobeTypes.LanguageDsc;

/**
 * ChromeOS OOBE input method descriptor.
 * @typedef {{
 *   optionGroupName: (string|undefined),
 *   selected: boolean,
 *   title: string,
 *   value: string,
 * }}
 */
OobeTypes.IMEDsc;

/**
 * ChromeOS OOBE demo country descriptor.
 * @typedef {{
 *   value: !string,
 *   title: (string|undefined),
 *   selected: (boolean|undefined),
 * }}
 */
OobeTypes.DemoCountryDsc;

/**
 * A set of flags of accessibility options for ChromeOS OOBE.
 * @typedef {{
 *   highContrastEnabled: boolean,
 *   spokenFeedbackEnabled: boolean,
 *   screenMagnifierEnabled: boolean,
 *   largeCursorEnabled: boolean,
 *   virtualKeyboardEnabled: boolean,
 * }}
 */
OobeTypes.A11yStatuses;

/**
 * Timezone ID.
 * @typedef {!string}
 */
OobeTypes.Timezone;

/**
 * ChromeOS timezone descriptor.
 * @typedef {{
 *   value: (OobeTypes.Timezone|undefined),
 *   title: (string|undefined),
 *   selected: (boolean|undefined),
 * }}
 */
OobeTypes.TimezoneDsc;

/**
 * OOBE configuration, allows automation during OOBE.
 * Keys are also listed in chrome/browser/ash/login/configuration_keys.h
 * @typedef {{
 *   language: (string|undefined),
 *   inputMethod: (string|undefined),
 *   welcomeNext: (boolean|undefined),
 *   enableDemoMode: (boolean|undefined),
 *   demoPreferencesNext: (boolean|undefined),
 *   networkSelectGuid: (string|undefined),
 *   networkOfflineDemo: (boolean|undefined),
 *   eulaAutoAccept: (boolean|undefined),
 *   eulaSendStatistics: (boolean|undefined),
 *   networkUseConnected: (boolean|undefined),
 *   arcTosAutoAccept: (boolean|undefined),
 *   networkConfig: (string|undefined),
 * }}
 */
OobeTypes.OobeConfiguration;

/**
 * Parameters passed to show PIN setup screen
 * @typedef {{
 *   auth_token: string,
 *   is_child_account: boolean,
 * }}
 */
OobeTypes.PinSetupScreenParameters;

/**
 * Configuration of the security token PIN dialog.
 * @typedef {{
 *   enableUserInput: boolean,
 *   attemptsLeft: number,
 *   hasError: boolean,
 *   formattedError: string,
 *   formattedAttemptsLeft: string,
 * }}
 */
OobeTypes.SecurityTokenPinDialogParameters;

/**
 * Data type that is expected for each app that is shown on the RecommendApps screen.
 * @typedef {{
 *   icon: string,
 *   name: string,
 *   package_name: string,
 * }}
 */
OobeTypes.RecommendedAppsExpectedAppData;

/**
 * Event sent from inner webview to enclosing Recommended apps screen.
 * @typedef {{
 *   type: (string|undefined),
 *   numOfSelected: number,
 * }}
 */
OobeTypes.RecommendedAppsSelectionEventData;

/**
 * Fatal Error Codes from SignInFatalErrorScreen
 * @enum {number}
 */
OobeTypes.FatalErrorCode = {
  UNKNOWN: 0,
  SCRAPED_PASSWORD_VERIFICATION_FAILURE: 1,
  INSECURE_CONTENT_BLOCKED: 2,
  MISSING_GAIA_INFO: 3,
  CUSTOM: 4,
};

/**
 * Screen steps used by EnterpriseEnrollmentElement. Defined here to
 * avoid circular dependencies since it is needed by cr_ui.js
 * @enum {string}
 */
OobeTypes.EnrollmentStep = {
  SIGNIN: 'signin',
  AD_JOIN: 'ad-join',
  WORKING: 'working',
  ATTRIBUTE_PROMPT: 'attribute-prompt',
  ERROR: 'error',
  SUCCESS: 'success',
  CHECKING: 'checking',
  TPM_CHECKING: 'tpm-checking',

  /* TODO(dzhioev): define this step on C++ side.
   */
  ATTRIBUTE_PROMPT_ERROR: 'attribute-prompt-error',
  ACTIVE_DIRECTORY_JOIN_ERROR: 'active-directory-join-error',
};

/**
 * Verification figure for the Quick Start screen.
 * @typedef {{
 *   shape: number,
 *   color: number,
 *   digit: number,
 * }}
 */
OobeTypes.QuickStartScreenFigureData;
