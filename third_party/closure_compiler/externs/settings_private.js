/** @fileoverview Externs generated from namespace: settingsPrivate */

/**
 * @const
 */
chrome.settingsPrivate = {};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/settingsPrivate#type-PrefType
 */
chrome.settingsPrivate.PrefType = {
  BOOLEAN: 'BOOLEAN',
  NUMBER: 'NUMBER',
  STRING: 'STRING',
  URL: 'URL',
  LIST: 'LIST',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/settingsPrivate#type-PolicySource
 */
chrome.settingsPrivate.PolicySource = {
  DEVICE: 'DEVICE',
  USER: 'USER',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/settingsPrivate#type-PolicyEnforcement
 */
chrome.settingsPrivate.PolicyEnforcement = {
  ENFORCED: 'ENFORCED',
  RECOMMENDED: 'RECOMMENDED',
};

/**
 * @typedef {{
 *   key: string,
 *   type: !chrome.settingsPrivate.PrefType,
 *   value: *,
 *   policySource: (!chrome.settingsPrivate.PolicySource|undefined),
 *   policyEnforcement: (!chrome.settingsPrivate.PolicyEnforcement|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/settingsPrivate#type-PrefObject
 */
var PrefObject;

/**
 * Sets a settings value.
 * @param {string} name The name of the pref.
 * @param {*} value The new value of the pref.
 * @param {string} pageId The user metrics identifier or null.
 * @param {function(boolean):void} callback The callback for whether the pref
 *     was set or not.
 * @see https://developer.chrome.com/extensions/settingsPrivate#method-setPref
 */
chrome.settingsPrivate.setPref = function(name, value, pageId, callback) {};

/**
 * Gets an array of all the prefs.
 * @param {function(!Array<PrefObject>):void} callback
 * @see https://developer.chrome.com/extensions/settingsPrivate#method-getAllPrefs
 */
chrome.settingsPrivate.getAllPrefs = function(callback) {};

/**
 * Gets the value of a specific pref.
 * @param {string} name
 * @param {function(PrefObject):void} callback
 * @see https://developer.chrome.com/extensions/settingsPrivate#method-getPref
 */
chrome.settingsPrivate.getPref = function(name, callback) {};

/**
 * <p>Fired when a set of prefs has changed.</p><p>|prefs| The prefs that
 * changed.</p>
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/settingsPrivate#event-onPrefsChanged
 */
chrome.settingsPrivate.onPrefsChanged;
