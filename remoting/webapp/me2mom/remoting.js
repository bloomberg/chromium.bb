// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chromoting = {};
XMPP_TOKEN_NAME = 'xmpp_token';
OAUTH2_TOKEN_NAME = 'oauth2_token';

function updateAuthStatus() {
  var oauth1_status = document.getElementById('oauth1_status');
  if (chromoting.oauth.hasToken()) {
    oauth1_status.innerText = 'OK';
    oauth1_status.style.color='green';
  } else {
    oauth1_status.innerText = 'Unauthorized';
    oauth1_status.style.color='red';
  }
}

function authorizeOAuth1() {
  chromoting.oauth.authorize(updateAuthStatus);
}

function clearOAuth1() {
  chromoting.oauth.clearTokens();
  updateAuthStatus();
}

function initAuthPanel_() {
  document.getElementById('xmpp_token').value =
      chromoting.getItem(XMPP_TOKEN_NAME);
  updateAuthStatus();
}

function initBackgroundFuncs_() {
  chromoting.getItem = chrome.extension.getBackgroundPage().getItem;
  chromoting.setItem = chrome.extension.getBackgroundPage().setItem;
  chromoting.oauth = chrome.extension.getBackgroundPage().oauth;
}

function saveCredentials(form) {
  chromoting.setItem(OAUTH2_TOKEN_NAME, form['oauth2_token'].value);
  chromoting.setItem(XMPP_TOKEN_NAME, form['xmpp_token'].value);
}

function init() {
  initBackgroundFuncs_();
  initAuthPanel_();
  setHostMode('unshared');
  setClientMode('unconnected');
  setGlobalMode(chromoting.getItem('startup-mode', 'host'));
}

// Show the div with id |mode| and hide those with other ids in |modes|.
function setMode_(mode, modes) {
  for (var i = 0; i < modes.length; ++i) {
    var div = document.getElementById(modes[i]);
    if (mode == modes[i]) {
      div.style.display = 'block';
    } else {
      div.style.display = 'none';
    }
  }
}

function setGlobalMode(mode) {
  setMode_(mode, ['host', 'client', 'session']);
}

function setGlobalModePersistent(mode) {
  setGlobalMode(mode);
  chromoting.setItem('startup-mode', mode);
}

function setHostMode(mode) {
  setMode_(mode, ['unshared', 'ready_to_share', 'shared']);
}

function setClientMode(mode) {
  setMode_(mode, ['unconnected', 'connecting']);
}

function tryShare() {
  setHostMode('ready_to_share');
  chromoting.hostTimer = setTimeout(
      function() {
        setHostMode('shared');
      },
      3000);
}

function cancelShare() {
  setHostMode('unshared');
  clearTimeout(chromoting.hostTimer);
}

function tryConnect(form) {
  chromoting.accessCode = form['access_code_entry'].value;
  setClientMode('connecting');
  chromoting.clientTimer = setTimeout(
      function() {
        var code = document.getElementById('access_code_proof');
        code.innerHTML = chromoting.accessCode;
        setGlobalMode('session');
      },
      3000);
}

function cancelConnect() {
  chromoting.accessCode = '';
  setClientMode('unconnected');
  clearTimeout(chromoting.clientTimer);
}
