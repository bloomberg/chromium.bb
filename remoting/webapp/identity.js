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
  /** @type {Array.<remoting.Identity.Callbacks>} */
  this.pendingCallbacks_ = [];
};

/**
 * Call a function with an access token.
 *
 * TODO(jamiewalch): Currently, this results in a new GAIA token being minted
 * each time the function is called. Implement caching functionality unless
 * getAuthToken starts doing so itself.
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
    chrome.experimental.identity.getAuthToken(
        { 'interactive': false },
        this.onAuthComplete_.bind(this, false));
  }
};

/**
 * Get the user's email address.
 *
 * @param {function(string):void} onOk Callback invoked when the email
 *     address is available.
 * @param {function(remoting.Error):void} onError Callback invoked if an
 *     error occurs.
 * @return {void} Nothing.
 */
remoting.Identity.prototype.getEmail = function(onOk, onError) {
  /** @type {remoting.Identity} */
  var that = this;
  /** @param {XMLHttpRequest} xhr The XHR response. */
  var onResponse = function(xhr) {
    var email = null;
    if (xhr.status == 200) {
      email = xhr.responseText.split('&')[0].split('=')[1];
      that.email_ = email;
      onOk(email);
      return;
    }
    console.error('Unable to get email address:', xhr.status, xhr);
    if (xhr.status == 401) {
      onError(remoting.Error.AUTHENTICATION_FAILED);
    } else {
      onError(that.interpretUnexpectedXhrStatus_(xhr.status));
    }
  };

  /** @param {string} token The access token. */
  var getEmailFromToken = function(token) {
    var headers = { 'Authorization': 'OAuth ' + token };
    // TODO(ajwong): Update to new v2 API.
    remoting.xhr.get('https://www.googleapis.com/userinfo/email',
                     onResponse, '', headers);
  };

  this.callWithToken(getEmailFromToken, onError);
};

/**
 * Get the user's email address, or null if no successful call to getEmail
 * has been made.
 *
 * @return {?string} The cached email address, if available.
 */
remoting.Identity.prototype.getCachedEmail = function() {
  return this.email_;
};

/**
 * Interprets unexpected HTTP response codes to authentication XMLHttpRequests.
 * The caller should handle the usual expected responses (200, 400) separately.
 *
 * @param {number} xhrStatus Status (HTTP response code) of the XMLHttpRequest.
 * @return {remoting.Error} An error code to be raised.
 * @private
 */
remoting.Identity.prototype.interpretUnexpectedXhrStatus_ = function(
    xhrStatus) {
  // Return AUTHENTICATION_FAILED by default, so that the user can try to
  // recover from an unexpected failure by signing in again.
  /** @type {remoting.Error} */
  var error = remoting.Error.AUTHENTICATION_FAILED;
  if (xhrStatus == 502 || xhrStatus == 503) {
    error = remoting.Error.SERVICE_UNAVAILABLE;
  } else if (xhrStatus == 0) {
    error = remoting.Error.NETWORK_FAILURE;
  } else {
    console.warn('Unexpected authentication response code: ' + xhrStatus);
  }
  return error;
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
  // TODO(jamiewalch): Figure out what to do with the error in this case.
  if (interactive) {
    console.error(chrome.runtime.lastError);
    while (this.pendingCallbacks_.length > 0) {
      var callback = /** @type {remoting.Identity.Callbacks} */
          this.pendingCallbacks_.shift();
      callback.onError(remoting.Error.UNEXPECTED);
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
  chrome.experimental.identity.getAuthToken(
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
