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
 * @typedef {{
 *   loginPair: LoginPair,
 *   plaintextPassword: string
 * }}
 * @see https://developer.chrome.com/extensions/passwordsPrivate#type-PlaintextPasswordEventParameters
 */
var PlaintextPasswordEventParameters;

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
 * Once the password has been fetched, it will be returned via the
 * onPlaintextPasswordRetrieved event.
 * @param {LoginPair} loginPair The LoginPair corresponding to the entry whose
 *     password     is to be returned.
 * @see https://developer.chrome.com/extensions/passwordsPrivate#method-requestPlaintextPassword
 */
chrome.passwordsPrivate.requestPlaintextPassword = function(loginPair) {};

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
 * has been added or removed. Note that this event fires as soon as a listener
 * is added.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/passwordsPrivate#event-onPasswordExceptionsListChanged
 */
chrome.passwordsPrivate.onPasswordExceptionsListChanged;

/**
 * Fired when a plaintext password has been fetched in response to a call to
 * chrome.passwordsPrivate.requestPlaintextPassword().
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/passwordsPrivate#event-onPlaintextPasswordRetrieved
 */
chrome.passwordsPrivate.onPlaintextPasswordRetrieved;
