// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * OAuth2 API flow implementations.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor */
remoting.OAuth2Api = function() {
};

/** @private
 *  @return {string} OAuth2 token URL.
 */
remoting.OAuth2Api.getOAuth2TokenEndpoint_ = function() {
  return remoting.settings.OAUTH2_BASE_URL + '/token';
};

/** @private
 *  @return {string} OAuth2 userinfo API URL.
 */
remoting.OAuth2Api.getOAuth2ApiUserInfoEndpoint_ = function() {
  return remoting.settings.OAUTH2_API_BASE_URL + '/v1/userinfo';
};


/**
 * Interprets HTTP error responses in authentication XMLHttpRequests.
 *
 * @private
 * @param {number} xhrStatus Status (HTTP response code) of the XMLHttpRequest.
 * @return {remoting.Error} An error code to be raised.
 */
remoting.OAuth2Api.interpretXhrStatus_ =
    function(xhrStatus) {
  // Return AUTHENTICATION_FAILED by default, so that the user can try to
  // recover from an unexpected failure by signing in again.
  /** @type {remoting.Error} */
  var error = remoting.Error.AUTHENTICATION_FAILED;
  if (xhrStatus == 400 || xhrStatus == 401 || xhrStatus == 403) {
    error = remoting.Error.AUTHENTICATION_FAILED;
  } else if (xhrStatus == 502 || xhrStatus == 503) {
    error = remoting.Error.SERVICE_UNAVAILABLE;
  } else if (xhrStatus == 0) {
    error = remoting.Error.NETWORK_FAILURE;
  } else {
    console.warn('Unexpected authentication response code: ' + xhrStatus);
  }
  return error;
};

/**
 * Asynchronously retrieves a new access token from the server.
 *
 * @param {function(string, number): void} onDone Callback to invoke when
 *     the access token and expiration time are successfully fetched.
 * @param {function(remoting.Error):void} onError Callback invoked if an
 *     error occurs.
 * @param {string} clientId OAuth2 client ID.
 * @param {string} clientSecret OAuth2 client secret.
 * @param {string} refreshToken OAuth2 refresh token to be redeemed.
 * @return {void} Nothing.
 */
remoting.OAuth2Api.refreshAccessToken = function(
    onDone, onError, clientId, clientSecret, refreshToken) {
  /** @param {XMLHttpRequest} xhr */
  var onResponse = function(xhr) {
    if (xhr.status == 200) {
      try {
        // Don't use jsonParseSafe here unless you move the definition out of
        // remoting.js, otherwise this won't work from the OAuth trampoline.
        // TODO(jamiewalch): Fix this once we're no longer using the trampoline.
        var tokens = JSON.parse(xhr.responseText);
        onDone(tokens['access_token'], tokens['expires_in']);
      } catch (err) {
        console.error('Invalid "token" response from server:',
                      /** @type {*} */ (err));
        onError(remoting.Error.UNEXPECTED);
      }
    } else {
      console.error('Failed to refresh token. Status: ' + xhr.status +
                    ' response: ' + xhr.responseText);
      onError(remoting.OAuth2Api.interpretXhrStatus_(xhr.status));
    }
  };

  var parameters = {
    'client_id': clientId,
    'client_secret': clientSecret,
    'refresh_token': refreshToken,
    'grant_type': 'refresh_token'
  };

  remoting.xhr.post(remoting.OAuth2Api.getOAuth2TokenEndpoint_(),
                    onResponse, parameters);
};

/**
 * Asynchronously exchanges an authorization code for access and refresh tokens.
 *
 * @param {function(string, string, number): void} onDone Callback to
 *     invoke when the refresh token, access token and access token expiration
 *     time are successfully fetched.
 * @param {function(remoting.Error):void} onError Callback invoked if an
 *     error occurs.
 * @param {string} clientId OAuth2 client ID.
 * @param {string} clientSecret OAuth2 client secret.
 * @param {string} code OAuth2 authorization code.
 * @param {string} redirectUri Redirect URI used to obtain this code.
 * @return {void} Nothing.
 */
remoting.OAuth2Api.exchangeCodeForTokens = function(
    onDone, onError, clientId, clientSecret, code, redirectUri) {
  /** @param {XMLHttpRequest} xhr */
  var onResponse = function(xhr) {
    if (xhr.status == 200) {
      try {
        // Don't use jsonParseSafe here unless you move the definition out of
        // remoting.js, otherwise this won't work from the OAuth trampoline.
        // TODO(jamiewalch): Fix this once we're no longer using the trampoline.
        var tokens = JSON.parse(xhr.responseText);
        onDone(tokens['refresh_token'],
               tokens['access_token'], tokens['expires_in']);
      } catch (err) {
        console.error('Invalid "token" response from server:',
                      /** @type {*} */ (err));
        onError(remoting.Error.UNEXPECTED);
      }
    } else {
      console.error('Failed to exchange code for token. Status: ' + xhr.status +
                    ' response: ' + xhr.responseText);
      onError(remoting.OAuth2Api.interpretXhrStatus_(xhr.status));
    }
  };

  var parameters = {
    'client_id': clientId,
    'client_secret': clientSecret,
    'redirect_uri': redirectUri,
    'code': code,
    'grant_type': 'authorization_code'
  };
  remoting.xhr.post(remoting.OAuth2Api.getOAuth2TokenEndpoint_(),
                    onResponse, parameters);
};

/**
 * Get the user's email address.
 *
 * @param {function(string):void} onDone Callback invoked when the email
 *     address is available.
 * @param {function(remoting.Error):void} onError Callback invoked if an
 *     error occurs.
 * @param {string} token Access token.
 * @return {void} Nothing.
 */
remoting.OAuth2Api.getEmail = function(onDone, onError, token) {
  /** @param {XMLHttpRequest} xhr */
  var onResponse = function(xhr) {
    if (xhr.status == 200) {
      try {
        var result = JSON.parse(xhr.responseText);
        onDone(result['email']);
      } catch (err) {
        console.error('Invalid "userinfo" response from server:',
                      /** @type {*} */ (err));
        onError(remoting.Error.UNEXPECTED);
      }
    } else {
      console.error('Failed to get email. Status: ' + xhr.status +
                    ' response: ' + xhr.responseText);
      onError(remoting.OAuth2Api.interpretXhrStatus_(xhr.status));
    }
  };
  var headers = { 'Authorization': 'OAuth ' + token };
  remoting.xhr.get(remoting.OAuth2Api.getOAuth2ApiUserInfoEndpoint_(),
                   onResponse, '', headers);
};
