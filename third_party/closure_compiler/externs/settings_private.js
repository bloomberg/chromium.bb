// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs generated from namespace: settingsPrivate
 * @externs
 */

/**
 * @const
 */
chrome.settingsPrivate = {};

/**
 * @typedef {{
 *   key: string,
 *   type: chrome.settingsPrivate.PrefType,
 *   value: *,
 *   source: chrome.settingsPrivate.PrefSource
 * }}
 */
chrome.settingsPrivate.PrefObject;

/**
 * @typedef {function(success)}
 */
chrome.settingsPrivate.OnPrefSetCallback;

/**
 * @typedef {function(!Array<chrome.settingsPrivate.PrefObject>)}
 */
chrome.settingsPrivate.GetAllPrefsCallback;

/**
 * @typedef {function(chrome.settingsPrivate.PrefObject)}
 */
chrome.settingsPrivate.GetPrefCallback;

/**
 * Sets a boolean settings value.
 * @param {string} name
 * @param {boolean} value
 * @param {chrome.settingsPrivate.OnPrefSetCallback} callback
 */
chrome.settingsPrivate.setBooleanPref = function(name, value, callback) {};

/**
 * Sets a number settings value.
 * @param {string} name
 * @param {number} value
 * @param {chrome.settingsPrivate.OnPrefSetCallback} callback
 */
chrome.settingsPrivate.setNumericPref = function(name, value, callback) {};

/**
 * Sets a string settings value.
 * @param {string} name
 * @param {string} value
 * @param {chrome.settingsPrivate.OnPrefSetCallback} callback
 */
chrome.settingsPrivate.setStringPref = function(name, value, callback) {};

/**
 * Sets a URL settings value.
 * @param {string} name
 * @param {string} value
 * @param {chrome.settingsPrivate.OnPrefSetCallback} callback
 */
chrome.settingsPrivate.setURLPref = function(name, value, callback) {};

/**
 * Gets all the prefs.
 * @param {chrome.settingsPrivate.GetAllPrefsCallback} callback
 */
chrome.settingsPrivate.getAllPrefs = function(callback) {};

/**
 * Gets the value of a specific pref.
 * @param {string} name
 * @param {chrome.settingsPrivate.GetPrefCallback} callback
 */
chrome.settingsPrivate.getPref = function(name, callback) {};

/** @type {!ChromeEvent} */
chrome.settingsPrivate.onPrefsChanged;
