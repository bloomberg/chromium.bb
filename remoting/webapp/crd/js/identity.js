// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Wrapper class for Chrome's identity API.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * TODO(jamiewalch): Remove remoting.OAuth2 from this type annotation when
 * the Apps v2 work is complete.
 *
 * @type {remoting.Identity|remoting.OAuth2}
 */
remoting.identity = null;

/**
 * @param {function(function():void):void} consentCallback Callback invoked if
 *     user consent is required. The callback is passed a continuation function
 *     which must be called from an interactive event handler (e.g. "click").
 * @constructor
 */
remoting.Identity = function(consentCallback) {
  /** @private */
  this.consentCallback_ = consentCallback;
  /** @type {?string} @private */
  this.email_ = null;
  /** @type {?string} @private */
  this.fullName_ = null;
  /** @type {Array.<remoting.Identity.Callbacks>} */
  this.pendingCallbacks_ = [];
};

/**
 * Call a function with an access token.
 *
 * @param {function(string):void} onOk Function to invoke with access token if
 *     an access token was successfully retrieved.
 * @param {function(remoting.Error):void} onError Function to invoke with an
 *     error code on failure.
 * @return {void} Nothing.
 */
remoting.Identity.prototype.callWithToken = function(onOk, onError) {
  this.pendingCallbacks_.push(new remoting.Identity.Callbacks(onOk, onError));
  if (this.pendingCallbacks_.length == 1) {
    chrome.identity.getAuthToken(
        { 'interactive': false },
        this.onAuthComplete_.bind(this, false));
  }
};

/**
 * Call a function with a fresh access token.
 *
 * @param {function(string):void} onOk Function to invoke with access token if
 *     an access token was successfully retrieved.
 * @param {function(remoting.Error):void} onError Function to invoke with an
 *     error code on failure.
 * @return {void} Nothing.
 */
remoting.Identity.prototype.callWithNewToken = function(onOk, onError) {
  /** @type {remoting.Identity} */
  var that = this;

  /**
   * @param {string} token
   */
  function revokeToken(token) {
    chrome.identity.removeCachedAuthToken(
        {'token': token },
        that.callWithToken.bind(that, onOk, onError));
  };

  this.callWithToken(revokeToken, onError);
};

/**
 * Remove the cached auth token, if any.
 *
 * @param {function():void} onDone Completion callback.
 * @return {void} Nothing.
 */
remoting.Identity.prototype.removeCachedAuthToken = function(onDone) {
  /** @param {string} token */
  var onToken = function(token) {
    if (token) {
      chrome.identity.removeCachedAuthToken({ 'token': token }, onDone);
    } else {
      onDone();
    }
  };
  chrome.identity.getAuthToken({ 'interactive': false }, onToken);
};

/**
 * Get the user's email address and full name.
 * The full name will be null unless the webapp has requested and been
 * granted the userinfo.profile permission.
 *
 * @param {function(string,string):void} onOk Callback invoked when the user's
 *     email address and full name are available.
 * @param {function(remoting.Error):void} onError Callback invoked if an
 *     error occurs.
 * @return {void} Nothing.
 */
remoting.Identity.prototype.getUserInfo = function(onOk, onError) {
  /** @type {remoting.Identity} */
  var that = this;
  /**
   * @param {string} email
   * @param {string} name
   */
  var onResponse = function(email, name) {
    that.email_ = email;
    that.fullName_ = name;
    onOk(email, name);
  };

  this.callWithToken(
      remoting.OAuth2Api.getUserInfo.bind(null, onResponse, onError), onError);
};

/**
 * Get the user's email address, or null if no successful call to getUserInfo
 * has been made.
 *
 * @return {?string} The cached email address, if available.
 */
remoting.Identity.prototype.getCachedEmail = function() {
  return this.email_;
};

/**
 * Get the user's full name.
 * This will return null if either:
 *   No successful call to getUserInfo has been made, or
 *   The webapp doesn't have permission to access this value.
 *
 * @return {?string} The cached user's full name, if available.
 */
remoting.Identity.prototype.getCachedUserFullName = function() {
  return this.fullName_;
};

/**
 * Callback for the getAuthToken API.
 *
 * @param {boolean} interactive The value of the "interactive" parameter to
 *     getAuthToken.
 * @param {?string} token The auth token, or null if the request failed.
 * @private
 */
remoting.Identity.prototype.onAuthComplete_ = function(interactive, token) {
  // Pass the token to the callback(s) if it was retrieved successfully.
  if (token) {
    while (this.pendingCallbacks_.length > 0) {
      var callback = /** @type {remoting.Identity.Callbacks} */
          this.pendingCallbacks_.shift();
      callback.onOk(token);
    }
    return;
  }

  // If not, pass an error back to the callback(s) if we've already prompted the
  // user for permission.
  if (interactive) {
    var error_message =
        chrome.runtime.lastError ? chrome.runtime.lastError.message
                                 : 'Unknown error.';
    console.error(error_message);
    while (this.pendingCallbacks_.length > 0) {
      var callback = /** @type {remoting.Identity.Callbacks} */
          this.pendingCallbacks_.shift();
      callback.onError(remoting.Error.NOT_AUTHENTICATED);
    }
    return;
  }

  // If there's no token, but we haven't yet prompted for permission, do so
  // now. The consent callback is responsible for continuing the auth flow.
  this.consentCallback_(this.onAuthContinue_.bind(this));
};

/**
 * Called in response to the user signing in to the web-app.
 *
 * @private
 */
remoting.Identity.prototype.onAuthContinue_ = function() {
  chrome.identity.getAuthToken(
      { 'interactive': true },
      this.onAuthComplete_.bind(this, true));
};

/**
 * Internal representation for pair of callWithToken callbacks.
 *
 * @param {function(string):void} onOk
 * @param {function(remoting.Error):void} onError
 * @constructor
 * @private
 */
remoting.Identity.Callbacks = function(onOk, onError) {
  /** @type {function(string):void} */
  this.onOk = onOk;
  /** @type {function(remoting.Error):void} */
  this.onError = onError;
};

/**
 * Returns whether the web app has authenticated with the Google services.
 *
 * @return {boolean}
 */
remoting.Identity.prototype.isAuthenticated = function() {
  return remoting.identity.email_ != null;
};
