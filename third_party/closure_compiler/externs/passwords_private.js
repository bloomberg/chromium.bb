// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: passwordsPrivate */

/**
 * @const
 */
chrome.passwordsPrivate = {};

/**
 * @typedef {{
 *   originUrl: string,
 *   username: string
 * }}
 * @see https://developer.chrome.com/extensions/passwordsPrivate#type-LoginPair
 */
var LoginPair;

/**
 * @typedef {{
 *   loginPair: LoginPair,
 *   numCharactersInPassword: number,
 *   federationText: (string|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/passwordsPrivate#type-PasswordUiEntry
 */
var PasswordUiEntry;

/**
 * Determines whether account's passwords can be managed via
 * https://passwords.google.com/settings/passwords.
 * @param {function(boolean):void} callback Callback which will be passed the
 *     boolean of whether the     account can be managed.
 * @see https://developer.chrome.com/extensions/passwordsPrivate#method-canPasswordAccountBeManaged
 */
chrome.passwordsPrivate.canPasswordAccountBeManaged = function(callback) {};

/**
 * Removes the saved password corresponding to |loginPair|. If no saved password
 * for this pair exists, this function is a no-op.
 * @param {LoginPair} loginPair The LoginPair corresponding to the entry to
 *     remove.
 * @see https://developer.chrome.com/extensions/passwordsPrivate#method-removeSavedPassword
 */
chrome.passwordsPrivate.removeSavedPassword = function(loginPair) {};

/**
 * Removes the saved password exception corresponding to |exceptionUrl|. If no
 * exception with this URL exists, this function is a no-op.
 * @param {string} exceptionUrl The URL corresponding to the exception to
 *     remove.
 * @see https://developer.chrome.com/extensions/passwordsPrivate#method-removePasswordException
 */
chrome.passwordsPrivate.removePasswordException = function(exceptionUrl) {};

/**
 * Returns the plaintext password corresponding to |loginPair|. Note that on
 * some operating systems, this call may result in an OS-level reauthentication.
 * @param {LoginPair} loginPair The LoginPair corresponding to the entry whose
 *     password     is to be returned.
 * @param {function(string):void} callback Callback which will be passed the
 *     plaintext password.
 * @see https://developer.chrome.com/extensions/passwordsPrivate#method-getPlaintextPassword
 */
chrome.passwordsPrivate.getPlaintextPassword = function(loginPair, callback) {};

/**
 * Fired when the saved passwords list has changed, meaning that an entry has
 * been added or removed. Note that this event fires as soon as a  listener is
 * added.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/passwordsPrivate#event-onSavedPasswordsListChanged
 */
chrome.passwordsPrivate.onSavedPasswordsListChanged;

/**
 * Fired when the password exceptions list has changed, meaning that an entry
 * has been added or removed. Note that this event fires as soon as a  listener
 * is added.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/passwordsPrivate#event-onPasswordExceptionsListChanged
 */
chrome.passwordsPrivate.onPasswordExceptionsListChanged;


