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
 * @type {remoting.Identity}
 */
remoting.identity = null;

/**
 * @param {remoting.Identity.ConsentDialog=} opt_consentDialog
 * @constructor
 */
remoting.Identity = function(opt_consentDialog) {
  /** @private */
  this.consentDialog_ = opt_consentDialog;
  /** @type {string} @private */
  this.email_ = '';
  /** @type {string} @private */
  this.fullName_ = '';
  /** @type {base.Deferred<string>} */
  this.authTokenDeferred_ = null;
};

/**
 * chrome.identity.getAuthToken should be initiated from user interactions if
 * called with interactive equals true.  This interface prompts a dialog for
 * the user's consent.
 *
 * @interface
 */
remoting.Identity.ConsentDialog = function() {};

/**
 * @return {Promise} A Promise that resolves when permission to start an
 *   interactive flow is granted.
 */
remoting.Identity.ConsentDialog.prototype.show = function() {};

/**
 * Gets an access token.
 *
 * @return {!Promise<string>} A promise resolved with an access token
 *     or rejected with a remoting.Error.
 */
remoting.Identity.prototype.getToken = function() {
  /** @const */
  var that = this;

  if (this.authTokenDeferred_ == null) {
    this.authTokenDeferred_ = new base.Deferred();
    chrome.identity.getAuthToken(
        { 'interactive': false },
        that.onAuthComplete_.bind(that, false));
  }
  return this.authTokenDeferred_.promise();
};

/**
 * Gets a fresh access token.
 *
 * @return {!Promise<string>} A promise resolved with an access token
 *     or rejected with a remoting.Error.
 */
remoting.Identity.prototype.getNewToken = function() {
  /** @type {remoting.Identity} */
  var that = this;

  return this.getToken().then(function(/** string */ token) {
    return new Promise(function(resolve, reject) {
      chrome.identity.removeCachedAuthToken({'token': token }, function() {
        resolve(that.getToken());
      });
    });
  });
};

/**
 * Removes the cached auth token, if any.
 *
 * @return {!Promise<null>} A promise resolved with the operation completes.
 */
remoting.Identity.prototype.removeCachedAuthToken = function() {
  return new Promise(function(resolve, reject) {
    /** @param {string} token */
    var onToken = function(token) {
      if (token) {
        chrome.identity.removeCachedAuthToken(
            {'token': token}, resolve.bind(null, null));
      } else {
        resolve(null);
      }
    };
    chrome.identity.getAuthToken({'interactive': false}, onToken);
  });
};

/**
 * Gets the user's email address and full name.  The full name will be
 * null unless the webapp has requested and been granted the
 * userinfo.profile permission.
 *
 * TODO(jrw): Type declarations say the name can't be null.  Are the
 * types wrong, or is the documentation wrong?
 *
 * @return {!Promise<{email:string, name:string}>} Promise
 *     resolved with the user's email address and full name, or rejected
 *     with a remoting.Error.
 */
remoting.Identity.prototype.getUserInfo = function() {
  if (this.isAuthenticated()) {
    /**
     * The temp variable is needed to work around a compiler bug.
     * @type {{email: string, name: string}}
     */
    var result = {email: this.email_, name: this.fullName_};
    return Promise.resolve(result);
  }

  /** @type {remoting.Identity} */
  var that = this;

  return this.getToken().then(function(token) {
    return new Promise(function(resolve, reject) {
      /**
       * @param {string} email
       * @param {string} name
       */
      var onResponse = function(email, name) {
        that.email_ = email;
        that.fullName_ = name;
        resolve({email: email, name: name});
      };

      remoting.oauth2Api.getUserInfo(onResponse, reject, token);
    });
  });
};

/**
 * Gets the user's email address.
 *
 * @return {!Promise<string>} Promise resolved with the user's email
 *     address or rejected with a remoting.Error.
 */
remoting.Identity.prototype.getEmail = function() {
  return this.getUserInfo().then(function(userInfo) {
    return userInfo.email;
  });
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
  var authTokenDeferred = this.authTokenDeferred_;

  // Pass the token to the callback(s) if it was retrieved successfully.
  if (token) {
    authTokenDeferred.resolve(token);
    this.authTokenDeferred_ = null;
    return;
  }

  // If not, pass an error back to the callback(s) if we've already prompted the
  // user for permission.
  if (interactive) {
    var error_message =
        chrome.runtime.lastError ? chrome.runtime.lastError.message
                                 : 'Unknown error.';
    console.error(error_message);
    authTokenDeferred.reject(remoting.Error.NOT_AUTHENTICATED);
    this.authTokenDeferred_ = null;
    return;
  }

  // If there's no token, but we haven't yet prompted for permission, do so
  // now.
  var that = this;
  var showConsentDialog =
      (this.consentDialog_) ? this.consentDialog_.show() : Promise.resolve();
  showConsentDialog.then(function() {
    chrome.identity.getAuthToken(
        {'interactive': true}, that.onAuthComplete_.bind(that, true));
  });
};

/**
 * Returns whether the web app has authenticated with the Google services.
 *
 * @return {boolean}
 */
remoting.Identity.prototype.isAuthenticated = function() {
  return remoting.identity.email_ !== '';
};
