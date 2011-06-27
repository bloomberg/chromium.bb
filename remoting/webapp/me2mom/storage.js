// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var remoting = remoting || {};

(function() {
"use strict";

/**
 * Creates a storage object abstracting our storage layer.
 *
 * This class provide accessors/mutators for all stateful items that are
 * persisted by the webapp.  This ensures that there are no collisions in
 * the naming between different modules.
 *
 * Note that not all data items are stored with the same lifetime.
 *
 * @constructor
 */
remoting.Storage = function() {
  /**
   * Key name for storing the OAuth2 refresh token.
   *
   * @const
   * @type {string}
   */
  this.KEY_OAUTH2_REFRESH_TOKEN_ = 'storage-oauth2-refresh-token';

  /**
   * Key name for storing the OAuth2 access token.
   *
   * @const
   * @type {string}
   */
  this.KEY_OAUTH2_ACCESS_TOKEN_ = 'storage-oauth2-access-token';

  /**
   * Key name for storing the user's e-mail address.
   *
   * @const
   * @type {string}
   */
  this.KEY_EMAIL_ = 'storage-email';
}

/**
 * @return {string} The refresh token.
 */
remoting.Storage.prototype.getOAuth2RefreshToken = function() {
  return unescape(window.localStorage.getItem(this.KEY_OAUTH2_REFRESH_TOKEN_));
}

/**
 * @param token {string} The new refresh token.
 * @return {void}
 */
remoting.Storage.prototype.setOAuth2RefreshToken = function(token) {
  window.localStorage.setItem(this.KEY_OAUTH2_REFRESH_TOKEN_, escape(token));
}

/**
 * @return {{access_token: string, expires_in: number}} The access token.
 */
remoting.Storage.prototype.getOAuth2AccessToken = function() {
  return JSON.parse(
      window.localStorage.getItem(this.KEY_OAUTH2_ACCESS_TOKEN_));
}

/**
 * @param token {string} The new access token.
 * @param expires {number} When token expires in seconds since epoch.
 * @return {void}
 */
remoting.Storage.prototype.setOAuth2AccessToken = function(token, expires) {
  var access_token = {'access_token': token, 'expires_in': expires}
  window.localStorage.setItem(this.KEY_OAUTH2_ACCESS_TOKEN_,
                              JSON.stringify(access_token));
}

/**
 * @return {string} The user's e-mail.
 */
remoting.Storage.prototype.getEmail = function() {
  return unescape(window.localStorage.getItem(this.KEY_EMAIL_));
}

/**
 * @param email {string} The user's email.
 * @return {void}
 */
remoting.Storage.prototype.setEmail = function(email) {
  window.localStorage.setItem(this.KEY_EMAIL_, escape(email));
}

}());
