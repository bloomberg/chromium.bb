language_settings_private_externs.js

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: languageSettingsPrivate */

/**
 * @const
 */
chrome.languageSettingsPrivate = {};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#type-DownloadStatus
 */
chrome.languageSettingsPrivate.DownloadStatus = {
  DOWNLOADED: 'DOWNLOADED',
  DOWNLOADING: 'DOWNLOADING',
  FAILURE: 'FAILURE',
};

/**
 * @typedef {{
 *   code: string,
 *   displayName: string,
 *   nativeDisplayName: string,
 *   rtl: (boolean|undefined),
 *   supportsUI: (boolean|undefined),
 *   supportsSpellCheck: (boolean|undefined),
 *   supportsTranslate: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#type-Language
 */
var Language;

/**
 * @typedef {{
 *   languageCode: string,
 *   status: !chrome.languageSettingsPrivate.DownloadStatus
 * }}
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#type-SpellCheckDictionaryStatus
 */
var SpellCheckDictionaryStatus;

/**
 * @typedef {{
 *   id: string,
 *   displayName: string,
 *   languageCodes: !Array<string>,
 *   enabled: boolean
 * }}
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#type-InputMethod
 */
var InputMethod;

/**
 * @typedef {{
 *   inputMethods: !Array<InputMethod>,
 *   componentExtensionIMEs: !Array<InputMethod>,
 *   thirdPartyExtensionIMEs: !Array<InputMethod>
 * }}
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#type-InputMethodLists
 */
var InputMethodLists;

/**
 * Gets languages available for translate, spell checking, input and locale.
 * @param {function(!Array<Language>):void} callback
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#method-getLanguageList
 */
chrome.languageSettingsPrivate.getLanguageList = function(callback) {};

/**
 * Sets the accepted languages, used to decide which languages to translate,
 * generate the Accept-Language header, etc.
 * @param {!Array<string>} languageCodes
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#method-setLanguageList
 */
chrome.languageSettingsPrivate.setLanguageList = function(languageCodes) {};

/**
 * Gets the current status of the spell check dictionary.
 * @param {function(SpellCheckDictionaryStatus):void} callback
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#method-getSpellCheckDictionaryStatus
 */
chrome.languageSettingsPrivate.getSpellCheckDictionaryStatus = function(callback) {};

/**
 * Gets the custom spell check words.
 * @param {function(!Array<string>):void} callback
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#method-getSpellCheckWords
 */
chrome.languageSettingsPrivate.getSpellCheckWords = function(callback) {};

/**
 * Gets the translate target language (in most cases, the display locale).
 * @param {function(string):void} callback
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#method-getTranslateTargetLanguage
 */
chrome.languageSettingsPrivate.getTranslateTargetLanguage = function(callback) {};

/**
 * Gets all supported input methods, including IMEs. Chrome OS only.
 * @param {function(InputMethodLists):void} callback
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#method-getInputMethodLists
 */
chrome.languageSettingsPrivate.getInputMethodLists = function(callback) {};

/**
 * Adds the input method to the current user's list of enabled input methods,
 * enabling the input method for the current user. Chrome OS only.
 * @param {string} inputMethodId
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#method-addInputMethod
 */
chrome.languageSettingsPrivate.addInputMethod = function(inputMethodId) {};

/**
 * Removes the input method from the current user's list of enabled input
 * methods, disabling the input method for the current user. Chrome OS only.
 * @param {string} inputMethodId
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#method-removeInputMethod
 */
chrome.languageSettingsPrivate.removeInputMethod = function(inputMethodId) {};

/**
 * Called when the language used for spell checking changes or the dictionary
 * download status changes.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#event-onSpellCheckDictionaryChanged
 */
chrome.languageSettingsPrivate.onSpellCheckDictionaryChanged;

/**
 * Called when an input method is added.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#event-onInputMethodAdded
 */
chrome.languageSettingsPrivate.onInputMethodAdded;

/**
 * Called when an input method is removed.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/languageSettingsPrivate#event-onInputMethodRemoved
 */
chrome.languageSettingsPrivate.onInputMethodRemoved;


