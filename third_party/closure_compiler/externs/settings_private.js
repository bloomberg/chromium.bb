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
 *   policySource: (chrome.settingsPrivate.PolicySource|undefined),
 *   policyEnforcement: (chrome.settingsPrivate.PolicyEnforcement|undefined),
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
 * Sets a settings value.
 * @param {string} name
 * @param {*} value
 * @param {string} pageId
 * @param {chrome.settingsPrivate.OnPrefSetCallback} callback
 */
chrome.settingsPrivate.setPref =
    function(name, value, pageId, callback) {};

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
