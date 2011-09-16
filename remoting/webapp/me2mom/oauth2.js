// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * OAuth2 class that handles retrieval/storage of an OAuth2 token.
 *
 * Uses a content script to trampoline the OAuth redirect page back into the
 * extension context.  This works around the lack of native support for
 * chrome-extensions in OAuth2.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {
/** @constructor */
remoting.OAuth2 = function() {
}

// Constants representing keys used for storing persistent state.
remoting.OAuth2.prototype.KEY_REFRESH_TOKEN_ = 'oauth2-refresh-token';
remoting.OAuth2.prototype.KEY_ACCESS_TOKEN_ = 'oauth2-access-token';

// Constants for parameters used in retrieving the OAuth2 credentials.
remoting.OAuth2.prototype.CLIENT_ID_ =
      '440925447803-2pi3v45bff6tp1rde2f7q6lgbor3o5uj.' +
      'apps.googleusercontent.com';
remoting.OAuth2.prototype.CLIENT_SECRET_ = 'W2ieEsG-R1gIA4MMurGrgMc_';
remoting.OAuth2.prototype.SCOPE_ =
      'https://www.googleapis.com/auth/chromoting ' +
      'https://www.googleapis.com/auth/googletalk ' +
      'https://www.googleapis.com/auth/userinfo#email';
remoting.OAuth2.prototype.REDIRECT_URI_ =
      'https://talkgadget.google.com/talkgadget/blank';
remoting.OAuth2.prototype.OAUTH2_TOKEN_ENDPOINT_ =
    'https://accounts.google.com/o/oauth2/token';

/** @return {boolean} True if the app is already authenticated. */
remoting.OAuth2.prototype.isAuthenticated = function() {
  if (this.getRefreshToken()) {
    return true;
  }
  return false;
}

/**
 * Removes all storage, and effectively unauthenticates the user.
 *
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.clear = function() {
  window.localStorage.removeItem(this.KEY_REFRESH_TOKEN_);
  this.clearAccessToken();
}

/**
 * @param {string} token The new refresh token.
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.setRefreshToken = function(token) {
  window.localStorage.setItem(this.KEY_REFRESH_TOKEN_, escape(token));
  this.clearAccessToken();
}

/** @return {?string} The refresh token, if authenticated, or NULL. */
remoting.OAuth2.prototype.getRefreshToken = function() {
  var value = window.localStorage.getItem(this.KEY_REFRESH_TOKEN_);
  if (typeof value == 'string') {
    return unescape(value);
  }
  return null;
}

/**
 * @param {string} token The new access token.
 * @param {number} expiration Expiration time in milliseconds since epoch.
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.setAccessToken = function(token, expiration) {
  var access_token = {'token': token, 'expiration': expiration};
  window.localStorage.setItem(this.KEY_ACCESS_TOKEN_,
                              JSON.stringify(access_token));
}

/**
 * Returns the current access token, setting it to a invalid value if none
 * existed before.
 *
 * @return {{token: string, expiration: number}} The current access token, or
 * an invalid token if not authenticated.
 */
remoting.OAuth2.prototype.getAccessTokenInternal_ = function() {
  if (!window.localStorage.getItem(this.KEY_ACCESS_TOKEN_)) {
    // Always be able to return structured data.
    this.setAccessToken('', 0);
  }
  var accessToken = window.localStorage.getItem(this.KEY_ACCESS_TOKEN_);
  if (typeof accessToken == 'string') {
    var result = JSON.parse(accessToken);
    if ('token' in result && 'expiration' in result) {
      return /** @type {{token: string, expiration: number}} */ result;
    }
  }
  console.log('Invalid access token stored.');
  return {'token': '', 'expiration': 0};
}

/**
 * Returns true if the access token is expired, or otherwise invalid.
 *
 * Will throw if !isAuthenticated().
 *
 * @return {boolean} True if a new access token is needed.
 */
remoting.OAuth2.prototype.needsNewAccessToken = function() {
  if (!this.isAuthenticated()) {
    throw 'Not Authenticated.';
  }
  var access_token = this.getAccessTokenInternal_();
  if (!access_token['token']) {
    return true;
  }
  if (Date.now() > access_token['expiration']) {
    return true;
  }
  return false;
}

/**
 * Returns the current access token.
 *
 * Will throw if !isAuthenticated() or needsNewAccessToken().
 *
 * @return {{token: string, expiration: number}}
 */
remoting.OAuth2.prototype.getAccessToken = function() {
  if (this.needsNewAccessToken()) {
    throw 'Access Token expired.';
  }
  return this.getAccessTokenInternal_()['token'];
}

/** @return {void} Nothing. */
remoting.OAuth2.prototype.clearAccessToken = function() {
  window.localStorage.removeItem(this.KEY_ACCESS_TOKEN_);
}

/**
 * Update state based on token response from the OAuth2 /token endpoint.
 *
 * @param {XMLHttpRequest} xhr The XHR object for this request.
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.processTokenResponse_ = function(xhr) {
  if (xhr.status == 200) {
    var tokens = JSON.parse(xhr.responseText);
    if ('refresh_token' in tokens) {
      this.setRefreshToken(tokens['refresh_token']);
    }

    // Offset by 120 seconds so that we can guarantee that the token
    // we return will be valid for at least 2 minutes.
    // If the access token is to be useful, this object must make some
    // guarantee as to how long the token will be valid for.
    // The choice of 2 minutes is arbitrary, but that length of time
    // is part of the contract satisfied by callWithToken().
    // Offset by a further 30 seconds to account for RTT issues.
    this.setAccessToken(tokens['access_token'],
        (tokens['expires_in'] - (120 + 30)) * 1000 + Date.now());
  } else {
    console.log('Failed to get tokens. Status: ' + xhr.status +
                ' response: ' + xhr.responseText);
  }
}

/**
 * Asynchronously retrieves a new access token from the server.
 *
 * Will throw if !isAuthenticated().
 *
 * @param {function(XMLHttpRequest): void} onDone Callback to invoke on
 *     completion.
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.refreshAccessToken = function(onDone) {
  if (!this.isAuthenticated()) {
    throw 'Not Authenticated.';
  }

  var parameters = {
    'client_id': this.CLIENT_ID_,
    'client_secret': this.CLIENT_SECRET_,
    'refresh_token': this.getRefreshToken(),
    'grant_type': 'refresh_token'
  };

  var that = this;
  remoting.xhr.post(this.OAUTH2_TOKEN_ENDPOINT_,
                    function(xhr) {
                      that.processTokenResponse_(xhr);
                      onDone(xhr);
                    },
                    parameters);
}

/**
 * Redirect page to get a new OAuth2 Refresh Token.
 *
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.doAuthRedirect = function() {
  var GET_CODE_URL = 'https://accounts.google.com/o/oauth2/auth?' +
    remoting.xhr.urlencodeParamHash({
          'client_id': this.CLIENT_ID_,
          'redirect_uri': this.REDIRECT_URI_,
          'scope': this.SCOPE_,
          'response_type': 'code'
        });
  window.location.replace(GET_CODE_URL);
}

/**
 * Asynchronously exchanges an authorization code for a refresh token.
 *
 * @param {string} code The new refresh token.
 * @param {function(XMLHttpRequest):void} onDone Callback to invoke on
 *     completion.
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.exchangeCodeForToken = function(code, onDone) {
  var parameters = {
    'client_id': this.CLIENT_ID_,
    'client_secret': this.CLIENT_SECRET_,
    'redirect_uri': this.REDIRECT_URI_,
    'code': code,
    'grant_type': 'authorization_code'
  };

  var that = this;
  remoting.xhr.post(this.OAUTH2_TOKEN_ENDPOINT_,
                    function(xhr) {
                      that.processTokenResponse_(xhr);
                      onDone(xhr);
                    },
                    parameters);
}

/**
 * Call myfunc with an access token as the only parameter.
 *
 * This will refresh the access token if necessary.  If the access token
 * cannot be refreshed, an error is thrown.
 *
 * The access token will remain valid for at least 2 minutes.
 *
 * @param {function({token: string, expiration: number}):void} myfunc
 *        Function to invoke with access token.
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.callWithToken = function(myfunc) {
  var that = this;
  if (remoting.oauth2.needsNewAccessToken()) {
    remoting.oauth2.refreshAccessToken(function() {
      if (remoting.oauth2.needsNewAccessToken()) {
        // If we still need it, we're going to infinite loop.
        throw 'Unable to get access token.';
      }
      myfunc(that.getAccessToken());
    });
    return;
  }

  myfunc(this.getAccessToken());
}
}());
