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
remoting.CLIENT_MODE = 'client';
remoting.HOST_MODE = 'host';
remoting.EMAIL = 'email';
remoting.HOST_PLUGIN_ID = 'host-plugin-id';

window.addEventListener('load', init_, false);

// Some constants for pretty-printing the access code.
var kSupportIdLen = 7;
var kHostSecretLen = 5;
var kAccessCodeLen = kSupportIdLen + kHostSecretLen;
var kDigitsPerGroup = 4;

function hasClass(element, cls) {
  return element.className.match(new RegExp('\\b' + cls + '\\b'));
}

function retrieveEmail_(access_token) {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status != 200) {
      // TODO(ajwong): Have a better way of showing an error.
      window.alert("Unable to get e-mail");
      return;
    }

    // TODO(ajwong): See if we can't find a JSON endpoint.
    setEmail(xhr.responseText.split('&')[0].split('=')[1]);
  };

  xhr.open('GET', 'https://www.googleapis.com/userinfo/email', true);
  xhr.setRequestHeader('Authorization', 'OAuth ' + access_token);
  xhr.send(null);
}

function refreshEmail_() {
  if (!remoting.getItem(remoting.EMAIL) && remoting.oauth2.isAuthenticated()) {
    remoting.oauth2.callWithToken(retrieveEmail_);
  }
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
      element.style.display = 'inline-block';
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
    document.getElementById('oauth2-code').value = '';
  }
  showElementById('oauth2-submit-button', false);
  showElementById('oauth2-code', !oauthValid);
  showElementById('oauth2-code-button', !oauthValid);
  showElementById('oauth2-clear-button', oauthValid);

  var loginName = remoting.getItem(remoting.EMAIL);
  if (loginName) {
    document.getElementById('current-email').innerText = loginName;
  }

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
  remoting.setItem(remoting.EMAIL, value);
  updateAuthStatus_();
}

function exchangedCodeForToken_() {
  if (!remoting.oauth2.isAuthenticated()) {
    alert('Your OAuth2 token was invalid. Please try again.');
  }
  remoting.oauth2.callWithToken(function(token) {
      retrieveEmail_(token);
      updateAuthStatus_();
  });
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
  refreshEmail_();
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
  showElement(document.getElementById('waiting-footer', false));
  remoting.currentMode = mode;
}

function setGlobalModePersistent(mode) {
  setGlobalMode(mode);
  remoting.setItem('startup-mode', mode);
}

function setHostMode(mode) {
  var section = document.getElementById('host-section');
  var modes = section.getElementsByClassName('mode');
  addToDebugLog('Host mode: ' + mode);
  setMode_(mode, modes);
}

function setClientMode(mode) {
  var section = document.getElementById('client-section');
  var modes = section.getElementsByClassName('mode');
  addToDebugLog('Client mode: ' + mode);
  setMode_(mode, modes);
}

function showWaiting_() {
  showElement(document.getElementById('client-footer'), false);
  showElement(document.getElementById('host-footer'), false);
  showElement(document.getElementById('waiting-footer'), true);
  document.getElementById('cancel-button').disabled = false;
}

function tryShare() {
  addToDebugLog('Attempting to share...');
  if (remoting.oauth2.needsNewAccessToken()) {
    addToDebugLog('Refreshing token...');
    remoting.oauth2.refreshAccessToken(function() {
      if (remoting.oauth2.needsNewAccessToken()) {
        // If we still need it, we're going to infinite loop.
        showShareError_('unable-to-get-token');
        throw 'Unable to get access token';
      }
      tryShare();
    });
    return;
  }

  showWaiting_();

  var div = document.getElementById('plugin-wrapper');
  var plugin = document.createElement('embed');
  plugin.setAttribute('type', remoting.PLUGIN_MIMETYPE);
  plugin.setAttribute('hidden', 'true');
  plugin.setAttribute('id', remoting.HOST_PLUGIN_ID);
  div.appendChild(plugin);
  plugin.onStateChanged = onStateChanged_;
  plugin.logDebugInfoCallback = debugInfoCallback_;
  plugin.connect(remoting.getItem(remoting.EMAIL),
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
    accessCodeDisplay.innerText = '';
    // Display the access code in groups of four digits for readability.
    for (var i = 0; i < accessCode.length; i += kDigitsPerGroup) {
      var nextFourDigits = document.createElement('span');
      nextFourDigits.className = 'access-code-digit-group';
      nextFourDigits.innerText = accessCode.substring(i, i + kDigitsPerGroup);
      accessCodeDisplay.appendChild(nextFourDigits);
    }
    setHostMode('ready-to-share');
  } else if (state == plugin.CONNECTED) {
    setHostMode('shared');
  } else if (state == plugin.DISCONNECTED) {
    setGlobalMode(remoting.HOST_MODE);
    setHostMode('unshared');
    plugin.parentNode.removeChild(plugin);
  } else {
    addToDebugLog('Unknown state -> ' + state);
  }
}

/**
* This is that callback that the host plugin invokes to indicate that there
* is additional debug log info to display.
*/
function debugInfoCallback_(msg) {
  addToDebugLog('plugin: ' + msg);
}

function showShareError_(errorCode) {
  var errorDiv = document.getElementById(errorCode);
  errorDiv.style.display = 'block';
  addToDebugLog("Sharing error: " + errorCode);
  setHostMode('share-failed');
}

function cancelShare() {
  addToDebugLog('Canceling share...');
  var plugin = document.getElementById(remoting.HOST_PLUGIN_ID);
  plugin.disconnect();
}

function startSession_() {
  addToDebugLog('Starting session...');
  remoting.username = remoting.getItem(remoting.EMAIL);
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

function normalizeAccessCode_(accessCode) {
  // Trim whitespace.
  // TODO(sergeyu): Do we need to do any other normalization here?
  return accessCode.replace(/\s/g, '');
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
        throw 'Unable to get access token.';
      }
      tryConnect();
    });
    return;
  }
  var accessCode = document.getElementById('access-code-entry').value;
  remoting.accessCode = normalizeAccessCode_(accessCode);
  // At present, only 12-digit access codes are supported, of which the first
  // 7 characters are the supportId.
  if (remoting.accessCode.length != kAccessCodeLen) {
    showConnectError_(404);
  } else {
    var supportId = remoting.accessCode.substring(0, kSupportIdLen);
    setClientMode('connecting');
    resolveSupportId(supportId);
  }
}

function cancelPendingOperation() {
  document.getElementById('cancel-button').disabled = true;
  if (remoting.currentMode == remoting.HOST_MODE) {
    cancelShare();
  }
}
