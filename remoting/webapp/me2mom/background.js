// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var remoting = {};

function setItem(key, value) {
  window.localStorage.setItem(key, value);
}

function getItem(key, defaultValue) {
  var result = window.localStorage.getItem(key);
  return (result != null) ? result : defaultValue;
}

function removeItem(key) {
  window.localStorage.removeItem(key);
}

function clearAll() {
  window.localStorage.clear();
}

var oauth = ChromeExOAuth.initBackgroundPage({
    'request_url': 'https://www.google.com/accounts/OAuthGetRequestToken',
    'authorize_url': 'https://www.google.com/accounts/OAuthAuthorizeToken',
    'access_url': 'https://www.google.com/accounts/OAuthGetAccessToken',
    'consumer_key': 'anonymous',
    'consumer_secret': 'anonymous',
    'scope': 'https://www.googleapis.com/auth/chromoting',
    'app_name': 'Remoting WebApp'
});
