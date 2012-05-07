// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

/** @type {remoting.OAuth2} */
remoting.oauth2 = null;


/** @constructor */
remoting.OAuth2 = function() {
};

// Constants representing keys used for storing persistent state.
/** @private */
remoting.OAuth2.prototype.KEY_REFRESH_TOKEN_ = 'oauth2-refresh-token';
/** @private */
remoting.OAuth2.prototype.KEY_REFRESH_TOKEN_REVOKABLE_ =
    'oauth2-refresh-token-revokable';
/** @private */
remoting.OAuth2.prototype.KEY_ACCESS_TOKEN_ = 'oauth2-access-token';
/** @private */
remoting.OAuth2.prototype.KEY_EMAIL_ = 'remoting-email';

// Constants for parameters used in retrieving the OAuth2 credentials.
/** @private */
remoting.OAuth2.prototype.SCOPE_ =
      'https://www.googleapis.com/auth/chromoting ' +
      'https://www.googleapis.com/auth/googletalk ' +
      'https://www.googleapis.com/auth/userinfo#email';
/** @private */
remoting.OAuth2.prototype.OAUTH2_TOKEN_ENDPOINT_ =
    'https://accounts.google.com/o/oauth2/token';
/** @private */
remoting.OAuth2.prototype.OAUTH2_REVOKE_TOKEN_ENDPOINT_ =
    'https://accounts.google.com/o/oauth2/revoke';
/** @return {boolean} True if the app is already authenticated. */
remoting.OAuth2.prototype.isAuthenticated = function() {
  if (this.getRefreshToken_()) {
    return true;
  }
  return false;
};

/**
 * Removes all storage, and effectively unauthenticates the user.
 *
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.clear = function() {
  window.localStorage.removeItem(this.KEY_EMAIL_);
  this.clearAccessToken();
  this.clearRefreshToken_();
};

/**
 * Sets the refresh token.
 *
 * This method also marks the token as revokable, so that this object will
 * revoke the token when it no longer needs it.
 *
 * @param {string} token The new refresh token.
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.setRefreshToken = function(token) {
  window.localStorage.setItem(this.KEY_REFRESH_TOKEN_, escape(token));
  window.localStorage.setItem(this.KEY_REFRESH_TOKEN_REVOKABLE_, true);
  this.clearAccessToken();
};

/**
 * Gets the refresh token.
 *
 * This method also marks the refresh token as not revokable, so that this
 * object will not revoke the token when it no longer needs it. After this
 * object has exported the token, it cannot know whether it is still in use
 * when this object no longer needs it.
 *
 * @return {?string} The refresh token, if authenticated, or NULL.
 */
remoting.OAuth2.prototype.exportRefreshToken = function() {
  window.localStorage.removeItem(this.KEY_REFRESH_TOKEN_REVOKABLE_);
  return this.getRefreshToken_();
};

/**
 * @return {?string} The refresh token, if authenticated, or NULL.
 * @private
 */
remoting.OAuth2.prototype.getRefreshToken_ = function() {
  var value = window.localStorage.getItem(this.KEY_REFRESH_TOKEN_);
  if (typeof value == 'string') {
    return unescape(value);
  }
  return null;
};

/**
 * Clears the refresh token.
 *
 * @return {void} Nothing.
 * @private
 */
remoting.OAuth2.prototype.clearRefreshToken_ = function() {
  if (window.localStorage.getItem(this.KEY_REFRESH_TOKEN_REVOKABLE_)) {
    this.revokeToken_(this.getRefreshToken_());
  }
  window.localStorage.removeItem(this.KEY_REFRESH_TOKEN_);
  window.localStorage.removeItem(this.KEY_REFRESH_TOKEN_REVOKABLE_);
};

/**
 * @param {string} token The new access token.
 * @param {number} expiration Expiration time in milliseconds since epoch.
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.setAccessToken = function(token, expiration) {
  var access_token = {'token': token, 'expiration': expiration};
  window.localStorage.setItem(this.KEY_ACCESS_TOKEN_,
                              JSON.stringify(access_token));
};

/**
 * Returns the current access token, setting it to a invalid value if none
 * existed before.
 *
 * @private
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
    var result = jsonParseSafe(accessToken);
    if (result && 'token' in result && 'expiration' in result) {
      return /** @type {{token: string, expiration: number}} */ result;
    }
  }
  console.log('Invalid access token stored.');
  return {'token': '', 'expiration': 0};
};

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
};

/**
 * Returns the current access token.
 *
 * Will throw if !isAuthenticated() or needsNewAccessToken().
 *
 * @return {string} The access token.
 */
remoting.OAuth2.prototype.getAccessToken = function() {
  if (this.needsNewAccessToken()) {
    throw 'Access Token expired.';
  }
  return this.getAccessTokenInternal_()['token'];
};

/** @return {void} Nothing. */
remoting.OAuth2.prototype.clearAccessToken = function() {
  window.localStorage.removeItem(this.KEY_ACCESS_TOKEN_);
};

/**
 * Update state based on token response from the OAuth2 /token endpoint.
 *
 * @private
 * @param {XMLHttpRequest} xhr The XHR object for this request.
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.processTokenResponse_ = function(xhr) {
  if (xhr.status == 200) {
    try {
      // Don't use jsonParseSafe here unless you move the definition out of
      // remoting.js, otherwise this won't work from the OAuth trampoline.
      // TODO(jamiewalch): Fix this once we're no longer using the trampoline.
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
    } catch (err) {
      console.error('Invalid "token" response from server:',
                    /** @type {*} */ (err));
    }
  } else {
    console.error('Failed to get tokens. Status: ' + xhr.status +
                  ' response: ' + xhr.responseText);
  }
};

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
    'refresh_token': this.getRefreshToken_(),
    'grant_type': 'refresh_token'
  };

  /** @type {remoting.OAuth2} */
  var that = this;
  /** @param {XMLHttpRequest} xhr The XHR reply. */
  var processTokenResponse = function(xhr) {
    that.processTokenResponse_(xhr);
    onDone(xhr);
  };
  remoting.xhr.post(this.OAUTH2_TOKEN_ENDPOINT_,
                    processTokenResponse,
                    parameters);
};

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
          'response_type': 'code',
          'access_type': 'offline',
          'approval_prompt': 'force'
        });
  window.location.replace(GET_CODE_URL);
};

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

  /** @type {remoting.OAuth2} */
  var that = this;
  /** @param {XMLHttpRequest} xhr The XHR reply. */
  var processTokenResponse = function(xhr) {
    that.processTokenResponse_(xhr);
    onDone(xhr);
  };
  remoting.xhr.post(this.OAUTH2_TOKEN_ENDPOINT_,
                    processTokenResponse,
                    parameters);
};

/**
 * Revokes a refresh or an access token.
 *
 * @param {string?} token An access or refresh token.
 * @return {void} Nothing.
 * @private
 */
remoting.OAuth2.prototype.revokeToken_ = function(token) {
  if (!token || (token.length == 0)) {
    return;
  }
  var parameters = { 'token': token };

  /** @param {XMLHttpRequest} xhr The XHR reply. */
  var processResponse = function(xhr) {
    if (xhr.status != 200) {
      console.log('Failed to revoke token. Status: ' + xhr.status +
                  ' ; response: ' + xhr.responseText + ' ; xhr: ', xhr);
    }
  };
  remoting.xhr.post(this.OAUTH2_REVOKE_TOKEN_ENDPOINT_,
                    processResponse,
                    parameters);
};

/**
 * Call myfunc with an access token as the only parameter.
 *
 * This will refresh the access token if necessary.  If the access token
 * cannot be refreshed, an error is thrown.
 *
 * The access token will remain valid for at least 2 minutes.
 *
 * @param {function(string?):void} myfunc Function to invoke with access token.
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.callWithToken = function(myfunc) {
  /** @type {remoting.OAuth2} */
  var that = this;
  if (this.needsNewAccessToken()) {
    var onRefresh = function() {
      if (that.needsNewAccessToken()) {
        myfunc(null);
      } else {
        myfunc(that.getAccessToken());
      }
    };
    this.refreshAccessToken(onRefresh);
  } else {
    myfunc(this.getAccessToken());
  }
};

/**
 * Get the user's email address.
 *
 * @param {function(?string):void} setEmail Callback invoked when the email
 *     address is available, or on error.
 * @return {void} Nothing.
 */
remoting.OAuth2.prototype.getEmail = function(setEmail) {
  /** @type {remoting.OAuth2} */
  var that = this;
  /** @param {XMLHttpRequest} xhr The XHR response. */
  var onResponse = function(xhr) {
    that.email = null;
    if (xhr.status == 200) {
      // TODO(ajwong): See if we can't find a JSON endpoint.
      that.email = xhr.responseText.split('&')[0].split('=')[1];
      window.localStorage.setItem(that.KEY_EMAIL_, that.email);
    } else {
      console.error('Unable to get email address:', xhr.status, xhr);
    }
    setEmail(that.email);
  };

  /** @param {string?} token The access token. */
  var getEmailFromToken = function(token) {
    if (token) {
      var headers = { 'Authorization': 'OAuth ' + token };
      // TODO(ajwong): Update to new v2 API.
      remoting.xhr.get('https://www.googleapis.com/userinfo/email',
                       onResponse, '', headers);
    } else {
      console.error('Unable to get email address: no access token');
      setEmail(null);
    }
  };

  this.callWithToken(getEmailFromToken);
};

/**
 * If the user's email address is cached, return it, otherwise return null.
 *
 * @return {?string} The email address, if it has been cached by a previous call
 *     to getEmail, otherwise null.
 */
remoting.OAuth2.prototype.getCachedEmail = function() {
  var value = window.localStorage.getItem(this.KEY_EMAIL_);
  if (typeof value == 'string') {
    return value;
  }
  return null;
};
