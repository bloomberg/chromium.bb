// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declare an OAuth2 class to handle retrieval/storage of an OAuth2 token.
//
// Ideally, this should implement the OAuth2 PostMessage flow to avoid needing
// to copy and paste a code, but that does not support extension URL schemes
// quite yet. Instead, we currently use the native app flow with an
// authorization code that the user must cut/paste.
function OAuth2() {
  this.OAUTH2_REFRESH_TOKEN_NAME = 'oauth2_refresh_token';

  this.client_id = encodeURIComponent(
      '440925447803-m890isgsr23kdkcu2erd4mirnrjalf98.' +
      'apps.googleusercontent.com');
  this.client_secret = encodeURIComponent('TgKrL73H2kJe6Ir0ufp7bf6e');
  this.scope = encodeURIComponent(
      'https://www.googleapis.com/auth/chromoting ' +
      'https://www.googleapis.com/auth/googletalk');
  this.redirect_uri = encodeURIComponent('urn:ietf:wg:oauth:2.0:oob');
}

OAuth2.prototype.isAuthenticated = function() {
  if(this.getRefreshToken()) {
    return true;
  }
  return false;
}

OAuth2.prototype.clear = function() {
  remoting.removeItem(this.OAUTH2_REFRESH_TOKEN_NAME);
  delete this.access_token;
  delete this.access_token_expiration;
}

OAuth2.prototype.setRefreshToken = function(token) {
  remoting.setItem(this.OAUTH2_REFRESH_TOKEN_NAME, token);
}

OAuth2.prototype.getRefreshToken = function(token) {
  return remoting.getItem(this.OAUTH2_REFRESH_TOKEN_NAME);
}

OAuth2.prototype.setAccessToken = function(token, expiration) {
  this.access_token = token;
  // Offset by 30 seconds to account for RTT issues.
  // TODO(ajwong): See if this is necessary, or of the protocol already
  // accounts for RTT.
  this.access_token_expiration = expiration - 30000;
}

OAuth2.prototype.needsNewAccessToken = function() {
  if (!this.isAuthenticated()) {
    throw "Not Authenticated.";
  }
  if (!this.access_token) {
    return true;
  }
  if (Date.now() > this.access_token_expiration) {
    return true;
  }
  return false;
}

OAuth2.prototype.getAccessToken = function() {
  if (this.needsNewAccessToken()) {
    throw "Access Token expired.";
  }
  return this.access_token;
}

OAuth2.prototype.refreshAccessToken = function(on_done) {
  if (!this.isAuthenticated()) {
    throw "Not Authenticated.";
  }
  var xhr = new XMLHttpRequest();
  var that = this;
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status == 200) {
      tokens = JSON.parse(xhr.responseText);
      that.setAccessToken(tokens['access_token'],
                          tokens['expires_in'] * 1000 + Date.now());
    } else {
      console.log("Refresh access token failed. Status: " + xhr.status +
                  " response: " + xhr.responseText);
    }
    on_done();
  };
  xhr.open('POST', 'https://accounts.google.com/o/oauth2/token', true);
  xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
  var post_data = 'client_id=' + this.client_id
      + '&client_secret=' + this.client_secret
      + '&refresh_token=' + encodeURIComponent(this.getRefreshToken())
      + '&grant_type=refresh_token';
  xhr.send(post_data);
}

OAuth2.prototype.openOAuth2Window = function() {
  var GET_CODE_URL = 'https://accounts.google.com/o/oauth2/auth?'
      + 'client_id=' + this.client_id
      + '&redirect_uri=' + this.redirect_uri
      + '&scope=' + this.scope
      + '&response_type=code';
  window.open(GET_CODE_URL);
}

OAuth2.prototype.exchangeCodeForToken = function(code, on_done) {
  var xhr = new XMLHttpRequest();
  var that = this;
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status == 200) {
      tokens = JSON.parse(xhr.responseText);
      that.setRefreshToken(tokens['refresh_token']);
      that.setAccessToken(tokens['access_token'],
                          tokens['expires_in'] + Date.now());
    } else {
      console.log("Code exchnage failed. Status: " + xhr.status +
                  " response: " + xhr.responseText);
    }
    on_done();
  };
  xhr.open('POST', 'https://accounts.google.com/o/oauth2/token', true);
  xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
  var post_data = 'client_id=' + this.client_id
      + '&client_secret=' + this.client_secret
      + '&redirect_uri=' + this.redirect_uri
      + '&code=' + encodeURIComponent(code)
      + '&grant_type=authorization_code';
  xhr.send(post_data);
}

