// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jamiewalch): strict mode causes the page to crash so it's disabled for
// now. Reinstate this when the associated bug is fixed.
// http://code.google.com/p/v8/issues/detail?id=1423
//"use strict";

// TODO(ajwong): This seems like a bad idea to share the exact same object
// with the background page.  Why are we doing it like this?
var remoting = chrome.extension.getBackgroundPage().remoting;
remoting.CLIENT_MODE='client';
remoting.HOST_MODE='host';
remoting.PLUGIN_MIMETYPE='HOST_PLUGIN_MIMETYPE';
remoting.XMPP_LOGIN_NAME = 'xmpp_login';
remoting.HOST_PLUGIN_ID = 'host-plugin-id';

window.addEventListener("load", init_, false);

function hasClass(element, cls) {
  return element.className.match(new RegExp('\\b' + cls + '\\b'));
}

function addClass(element, cls) {
  if (!hasClass(element, cls))
    element.className = element.className + ' ' + cls;
}

function removeClass(element, cls) {
  element.className =
      element.className.replace(new RegExp('\\b' + cls + '\\b', 'g'), '');
}

function showElement(element, visible) {
  if (visible) {
    if (hasClass(element, 'display-inline')) {
      element.style.display = 'inline';
    } else {
      element.style.display = 'block';
    }
  } else {
    element.style.display = 'none';
  }
}

function showElementById(id, visible) {
  showElement(document.getElementById(id), visible);
}

// This code moved into this subroutine (instead of being inlined in
// updateAuthStatus_() because of bug in V8.
// http://code.google.com/p/v8/issues/detail?id=1423
function updateControls_(disable) {
  var authStatusControls =
      document.getElementsByClassName('auth-status-control');
  for (var i = 0; i < authStatusControls.length; ++i) {
    authStatusControls[i].disabled = disable;
  }
}

function updateAuthStatus_() {
  var oauthValid = remoting.oauth2.isAuthenticated();
  if (!oauthValid) {
    document.getElementById('oauth2-code').value = "";
  }
  showElementById('oauth2-submit-button', false);
  showElementById('oauth2-code', !oauthValid);
  showElementById('oauth2-code-button', !oauthValid);
  showElementById('oauth2-clear-button', oauthValid);

  var loginName = remoting.getItem(remoting.XMPP_LOGIN_NAME);
  if (loginName) {
    document.getElementById('current-email').innerText = loginName;
  }
  showElementById('current-email', loginName);
  showElementById('change-email-button', loginName);
  showElementById('new-email', !loginName);
  showElementById('email-submit-button', !loginName);

  var disableControls = !(loginName && oauthValid);
  var authPanel = document.getElementById('auth-panel');
  if (disableControls) {
    authPanel.style.backgroundColor = 'rgba(204, 0, 0, 0.15)';
  } else {
    authPanel.style.backgroundColor = 'rgba(0, 204, 102, 0.15)';
  }
  updateControls_(disableControls);
}

function initBackgroundFuncs_() {
  remoting.getItem = chrome.extension.getBackgroundPage().getItem;
  remoting.setItem = chrome.extension.getBackgroundPage().setItem;
  remoting.removeItem = chrome.extension.getBackgroundPage().removeItem;
  remoting.oauth2 = new OAuth2();
}

function setEmail(value) {
  remoting.setItem(remoting.XMPP_LOGIN_NAME, value);
  updateAuthStatus_();
}

function exchangedCodeForToken_() {
  if (!remoting.oauth2.isAuthenticated()) {
    alert("Your OAuth2 token was invalid. Please try again.");
  }
  updateAuthStatus_();
}

function authorizeOAuth2() {
  remoting.oauth2.exchangeCodeForToken(
      document.getElementById('oauth2-code').value, exchangedCodeForToken_);
}

function clearOAuth2() {
  remoting.oauth2.clear();
  updateAuthStatus_();
}

function handleOAuth2CodeChange() {
  var hasCode = document.getElementById('oauth2-code').value.length > 0;

  showElementById('oauth2-submit-button', hasCode);
  showElementById('oauth2-code-button', !hasCode);
}

// Show the div with id |mode| and hide those with other ids in |modes|.
function setMode_(mode, modes) {
  for (var i = 0; i < modes.length; ++i) {
    showElement(modes[i], mode == modes[i].id);
  }
}

function init_() {
  initBackgroundFuncs_();
  updateAuthStatus_();
  setHostMode('unshared');
  setClientMode('unconnected');
  setGlobalMode(remoting.getItem('startup-mode', remoting.HOST_MODE));
  addClass(document.getElementById('loading-panel'), 'hidden');
  removeClass(document.getElementById('main-panel'), 'hidden');
}

function setGlobalMode(mode) {
  var elementsToShow = [];
  var elementsToHide = [];
  var hostElements = document.getElementsByClassName('host-element');
  var clientElements = document.getElementsByClassName('client-element');
  if (mode == remoting.HOST_MODE) {
    elementsToShow = hostElements;
    elementsToHide = clientElements;
  } else {
    elementsToShow = clientElements;
    elementsToHide = hostElements;
  }
  for (var i = 0; i < elementsToShow.length; ++i) {
    showElement(elementsToShow[i], true);
  }
  for (var i = 0; i < elementsToHide.length; ++i) {
    showElement(elementsToHide[i], false);
  }
}

function setGlobalModePersistent(mode) {
  setGlobalMode(mode);
  remoting.setItem('startup-mode', mode);
}

function setHostMode(mode) {
  var section = document.getElementById('host-section');
  var modes = section.getElementsByClassName('mode');
  setMode_(mode, modes);
}

function setClientMode(mode) {
  var section = document.getElementById('client-section');
  var modes = section.getElementsByClassName('mode');
  setMode_(mode, modes);
}

function tryShare() {
  if (remoting.oauth2.needsNewAccessToken()) {
    remoting.oauth2.refreshAccessToken(function() {
      if (remoting.oauth2.needsNewAccessToken()) {
        // If we still need it, we're going to infinite loop.
        throw "Unable to get access token";
      }
      tryShare();
    });
    return;
  }

  var div = document.getElementById('plugin-wrapper');
  var plugin = document.createElement('embed');
  plugin.setAttribute('type', remoting.PLUGIN_MIMETYPE);
  plugin.setAttribute('hidden', 'true');
  plugin.setAttribute('id', remoting.HOST_PLUGIN_ID);
  div.appendChild(plugin);
  plugin.onStateChanged = onStateChanged_;
  plugin.connect(remoting.getItem(remoting.XMPP_LOGIN_NAME),
                 'oauth2:' + remoting.oauth2.getAccessToken());
}

function onStateChanged_() {
  var plugin = document.getElementById(remoting.HOST_PLUGIN_ID);
  var state = plugin.state;
  if (state == plugin.REQUESTED_ACCESS_CODE) {
    setHostMode('preparing-to-share');
  } else if (state == plugin.RECEIVED_ACCESS_CODE) {
    var accessCode = plugin.accessCode;
    var accessCodeDisplay = document.getElementById('access-code-display');
    accessCodeDisplay.innerText = accessCode;
    setHostMode('ready-to-share');
  } else if (state == plugin.CONNECTED) {
    setHostMode('shared');
  } else if (state == plugin.DISCONNECTED) {
    setHostMode('unshared');
    plugin.parentNode.removeChild(plugin);
  } else {
    window.alert('Unknown state -> ' + state);
  }
}

function cancelShare() {
  var plugin = document.getElementById(remoting.HOST_PLUGIN_ID);
  plugin.disconnect();
}

function startSession_() {
  remoting.username = remoting.getItem(remoting.XMPP_LOGIN_NAME);
  document.location = 'remoting_session.html';
}

function showConnectError_(responseCode, responseString) {
  var invalid = document.getElementById('invalid-access-code');
  var other = document.getElementById('other-connect-error');
  if (responseCode == 404) {
    invalid.style.display = 'block';
    other.style.display = 'none';
  } else {
    invalid.style.display = 'none';
    other.style.display = 'block';
    var responseNode = document.getElementById('server-response');
    responseNode.innerText = responseString + ' (' + responseCode + ')';
  }
  remoting.accessCode = '';
  setClientMode('connect-failed');
}

function parseServerResponse_(xhr) {
  if (xhr.status == 200) {
    var host = JSON.parse(xhr.responseText);
    if (host.data && host.data.jabberId) {
      remoting.hostJid = host.data.jabberId;
      remoting.hostPublicKey = host.data.publicKey;
      startSession_();
      return;
    }
  }
  showConnectError_(xhr.status, xhr.responseText);
}

function normalizeAccessCode(accessCode) {
  // Trim whitespace from beginning and the end.
  // TODO(sergeyu): Do we need to do any other normalization here?
  return accessCode.replace(/^\s+|\s+$/, '');
}

function resolveSupportId(supportId) {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      if (xhr.readyState != 4) {
        return;
      }
      parseServerResponse_(xhr);
    };

    xhr.open('GET',
             'https://www.googleapis.com/chromoting/v1/support-hosts/' +
                 encodeURIComponent(supportId),
             true);
    xhr.setRequestHeader('Authorization',
                         'OAuth ' + remoting.oauth2.getAccessToken());
    xhr.send(null);
}

function tryConnect() {
  if (remoting.oauth2.needsNewAccessToken()) {
    remoting.oauth2.refreshAccessToken(function() {
      if (remoting.oauth2.needsNewAccessToken()) {
        // If we still need it, we're going to infinite loop.
        throw "Unable to get access token";
      }
      tryConnect();
    });
    return;
  }
  var accessCode = document.getElementById('access-code-entry').value;
  remoting.accessCode = accessCode;
  // TODO(jamiewalch): Since the mapping from (SupportId, HostSecret) to
  // AccessCode is not yet defined, assume it's hyphen-separated for now.
  var parts = remoting.accessCode.split('-');
  if (parts.length != 2) {
    showConnectError_(404);
  } else {
    setClientMode('connecting');
    resolveSupportId(parts[0]);
  }
}

function cancelConnect() {
  remoting.accessCode = '';
  setClientMode('unconnected');
}
