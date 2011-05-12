// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var remoting = {};
XMPP_TOKEN_NAME = 'xmpp_token';
OAUTH2_TOKEN_NAME = 'oauth2_token';
HOST_PLUGIN_ID = 'host_plugin_id';

function updateAuthStatus() {
  var oauth1_status = document.getElementById('oauth1_status');
  if (remoting.oauth.hasToken()) {
    oauth1_status.innerText = 'OK';
    oauth1_status.style.color='green';
  } else {
    oauth1_status.innerText = 'Unauthorized';
    oauth1_status.style.color='red';
  }
}

function authorizeOAuth1() {
  remoting.oauth.authorize(updateAuthStatus);
}

function clearOAuth1() {
  remoting.oauth.clearTokens();
  updateAuthStatus();
}

function initAuthPanel_() {
  document.getElementById('xmpp_token').value =
      remoting.getItem(XMPP_TOKEN_NAME);
  updateAuthStatus();
}

function initBackgroundFuncs_() {
  remoting.getItem = chrome.extension.getBackgroundPage().getItem;
  remoting.setItem = chrome.extension.getBackgroundPage().setItem;
  remoting.oauth = chrome.extension.getBackgroundPage().oauth;
}

function saveCredentials(form) {
  remoting.setItem(OAUTH2_TOKEN_NAME, form['oauth2_token'].value);
  remoting.setItem(XMPP_TOKEN_NAME, form['xmpp_token'].value);
}

function init() {
  initBackgroundFuncs_();
  initAuthPanel_();
  setHostMode('unshared');
  setClientMode('unconnected');
  setGlobalMode(remoting.getItem('startup-mode', 'host'));
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
  remoting.setItem('startup-mode', mode);
}

function setHostMode(mode) {
  setMode_(mode, ['unshared',
                  'preparing_to_share',
                  'ready_to_share',
                  'shared']);
}

function setClientMode(mode) {
  setMode_(mode, ['unconnected', 'connecting']);
}

function tryShare() {
  var div = document.getElementById('plugin_wrapper');
  var plugin = document.createElement('embed');
  plugin.setAttribute('type', 'HOST_PLUGIN_MIMETYPE');
  plugin.setAttribute('hidden', 'true');
  plugin.setAttribute('id', HOST_PLUGIN_ID);
  div.appendChild(plugin);
  plugin.onStateChanged = onStateChanged;
  plugin.connect('uid', 'authtoken');
}

function onStateChanged() {
  var plugin = document.getElementById(HOST_PLUGIN_ID);
  var state = plugin.state;
  if (state == plugin.REQUESTED_SUPPORT_ID) {
    setHostMode('preparing_to_share');
  } else if (state == plugin.RECEIVED_SUPPORT_ID) {
    var support_id = plugin.supportID;
    var access_code = document.getElementById('access_code_display');
    access_code.innerHTML = support_id;
    setHostMode('ready_to_share');
  } else if (state == plugin.CONNECTED) {
    setHostMode('shared');
  } else if (state == plugin.DISCONNECTED) {
    setHostMode('unshared');
    plugin.parentNode.removeChild(plugin);
  } else {
    window.alert("Unknown state -> " + state);
  }
}

function cancelShare() {
  var plugin = document.getElementById(HOST_PLUGIN_ID);
  plugin.disconnect();
}

function tryConnect(form) {
  remoting.accessCode = form['access_code_entry'].value;
  setClientMode('connecting');
  remoting.clientTimer = setTimeout(
      function() {
        var code = document.getElementById('access_code_proof');
        code.innerHTML = remoting.accessCode;
        setGlobalMode('session');
      },
      3000);
}

function cancelConnect() {
  remoting.accessCode = '';
  setClientMode('unconnected');
  clearTimeout(remoting.clientTimer);
}
