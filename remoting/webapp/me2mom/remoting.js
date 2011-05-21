// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(ajwong): This seems like a bad idea to share the exact same object
// with the background page.  Why are we doing it like this?
var remoting = chrome.extension.getBackgroundPage().remoting;

XMPP_LOGIN_NAME = 'xmpp_login';
XMPP_TOKEN_NAME = 'xmpp_token';
HOST_PLUGIN_ID = 'host_plugin_id';

function updateAuthStatus_() {
  var oauth2_status = document.getElementById('oauth2_status');
  if (remoting.oauth2.isAuthenticated()) {
    oauth2_status.innerText = 'OK';
    oauth2_status.style.color = 'green';
    document.getElementById('oauth2_code_button').style.display = 'none';
    document.getElementById('oauth2_clear_button').style.display = 'inline';
    document.getElementById('oauth2_form').style.display = 'none';
  } else {
    oauth2_status.innerText = 'Unauthorized';
    oauth2_status.style.color = 'red';
    document.getElementById('oauth2_code_button').style.display = 'inline';
    document.getElementById('oauth2_clear_button').style.display = 'none';
    document.getElementById('oauth2_form').style.display = 'inline';
  }
  var xmpp_status = document.getElementById('xmpp_status');
  if (remoting.getItem(XMPP_TOKEN_NAME) && remoting.getItem(XMPP_LOGIN_NAME)) {
    document.getElementById('xmpp_clear').style.display = 'inline';
    document.getElementById('xmpp_form').style.display = 'none';
    xmpp_status.innerText = 'OK';
    xmpp_status.style.color = 'green';
    remoting.xmppAuthToken = remoting.getItem(XMPP_TOKEN_NAME);
  } else {
    document.getElementById('xmpp_clear').style.display = 'none';
    document.getElementById('xmpp_form').style.display = 'inline';
    xmpp_status.innerText = 'Unauthorized';
    xmpp_status.style.color = 'red';
  }
}

function clientLoginError_(xhr) {
  // If there's an error URL, load it into an iframe.
  var url_line = xhr.responseText.match('Url=.*');
  if (url_line) {
    url = url_line[0].substr(4);
    var error_frame = document.getElementById('xmpp_error');
    error_frame.src = url;
    error_frame.style.display = 'block';
  }

  var log_msg = 'Client Login failed. Status: ' + xhr.status +
      ' body: ' + xhr.responseText;
  console.log(log_msg);
  var last_error = document.getElementById('xmpp_last_error');
  last_error.innerText = log_msg;
  last_error.style.display = 'inline';
}

function resetXmppErrors_() {
  document.getElementById('xmpp_captcha').style.display = 'none';
  document.getElementById('xmpp_error').style.display = 'none';
  document.getElementById('xmpp_last_error').style.display = 'none';
}

function displayCaptcha_(form, url, token) {
  form['xmpp_captcha_token'].value = token;
  document.getElementById('xmpp_captcha_img').src = url;
  document.getElementById('xmpp_captcha').style.display = 'inline';
}

function readAndClearCaptcha_(form) {
  var captcha_token = form['xmpp_captcha_token'].value;
  form['xmpp_captcha_token'].value = '';
  resetXmppErrors_();
  return [captcha_token, form['xmpp_captcha_result'].value];
}

function initAuthPanel_() {
  updateAuthStatus_();
  resetXmppErrors_();
}

function initBackgroundFuncs_() {
  remoting.getItem = chrome.extension.getBackgroundPage().getItem;
  remoting.setItem = chrome.extension.getBackgroundPage().setItem;
  remoting.removeItem = chrome.extension.getBackgroundPage().removeItem;
  remoting.oauth2 = new OAuth2();
}

function authorizeXmpp(form) {
  var xhr = new XMLHttpRequest();
  var captcha_result = readAndClearCaptcha_(form);

  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }

    if (xhr.status == 200) {
      var auth_line = xhr.responseText.match('Auth=.*');
      if (!auth_line) {
        clientLoginError_(xhr);
        return;
      }
      remoting.setItem(XMPP_TOKEN_NAME, auth_line[0].substr(5));
      remoting.setItem(XMPP_LOGIN_NAME, form['xmpp_username'].value);
      updateAuthStatus_();
    } else if (xhr.status == 403) {
      var error_line = xhr.responseText.match('Error=.*');
      if (error_line && error_line == 'Error=CaptchaRequired') {
        var captcha_token = xhr.responseText.match('CaptchaToken=.*');
        var captcha_url = xhr.responseText.match('CaptchaUrl=.*');
        displayCaptcha_(
            form,
            'https://www.google.com/accounts/' + captcha_url[0].substr(11),
            captcha_token[0].substr(13));
        return;
      }
      clientLoginError_(xhr);
    } else {
      clientLoginError_(xhr);
    }
  };
  xhr.open('POST', 'https://www.google.com/accounts/ClientLogin', true);
  xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
  var post_data =
      'accountType=HOSTED_OR_GOOGLE' +
      '&service=chromiumsync' +
      '&source=remoting-webapp' +
      '&Email=' + encodeURIComponent(form['xmpp_username'].value) +
      '&Passwd=' + encodeURIComponent(form['xmpp_password'].value);

  if (captcha_result[0]) {
    post_data += '&logintoken=' + encodeURIComponent(captcha_result[0]) +
        '&logincaptcha=' + encodeURIComponent(captcha_result[1]);
  }
  xhr.send(post_data);
}

function authorizeOAuth2(code) {
  remoting.oauth2.exchangeCodeForToken(code, updateAuthStatus_);
}

function clearOAuth2() {
  remoting.oauth2.clear();
  updateAuthStatus_();
}

function clearXmpp() {
  remoting.removeItem(XMPP_TOKEN_NAME);
  updateAuthStatus_();
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

function init() {
  initBackgroundFuncs_();
  initAuthPanel_();
  setHostMode('unshared');
  setClientMode('unconnected');
  setGlobalMode(remoting.getItem('startup-mode', 'host'));
}

function setGlobalMode(mode) {
  setMode_(mode, ['host', 'client']);
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
  setMode_(mode, ['unconnected', 'connecting', 'connect_failed']);
}

function tryShare() {
  var div = document.getElementById('plugin_wrapper');
  var plugin = document.createElement('embed');
  plugin.setAttribute('type', 'HOST_PLUGIN_MIMETYPE');
  plugin.setAttribute('hidden', 'true');
  plugin.setAttribute('id', HOST_PLUGIN_ID);
  div.appendChild(plugin);
  plugin.onStateChanged = onStateChanged_;
  plugin.connect(remoting.getItem(XMPP_LOGIN_NAME),
                 remoting.getItem(XMPP_TOKEN_NAME));
}

function onStateChanged_() {
  var plugin = document.getElementById(HOST_PLUGIN_ID);
  var state = plugin.state;
  if (state == plugin.REQUESTED_ACCESS_CODE) {
    setHostMode('preparing_to_share');
  } else if (state == plugin.RECEIVED_ACCESS_CODE) {
    var access_code = plugin.accessCode;
    var access_code_display = document.getElementById('access_code_display');
    access_code_display.innerText = access_code;
    setHostMode('ready_to_share');
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
  var plugin = document.getElementById(HOST_PLUGIN_ID);
  plugin.disconnect();
}

function startSession_() {
  remoting.username = remoting.getItem(XMPP_LOGIN_NAME);
  document.location = 'remoting_session.html';
}

function showConnectError_(responseCode, responseString) {
  var invalid = document.getElementById('invalid_access_code');
  var other = document.getElementById('other_connect_error');
  if (responseCode == 404) {
    invalid.style.display = 'block';
    other.style.display = 'none';
  } else {
    invalid.style.display = 'none';
    other.style.display = 'block';
    var responseNode = document.getElementById('server_response');
    responseNode.innerText = responseString + ' (' + responseCode + ')';
  }
  remoting.accessCode = '';
  setClientMode('connect_failed');
}

function parseServerResponse_(xhr) {
  if (xhr.status == 200) {
    var host = JSON.parse(xhr.responseText);
    if (host.data && host.data.jabberId) {
      remoting.hostjid = host.data.jabberId;
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

function resolveSupportId(support_id) {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      if (xhr.readyState != 4) {
        return;
      }
      parseServerResponse_(xhr);
    };

    xhr.open('GET',
             'https://www.googleapis.com/chromoting/v1/support-hosts/' +
                 encodeURIComponent(support_id),
             true);
    xhr.setRequestHeader('Authorization',
                         'OAuth ' + remoting.oauth2.getAccessToken());
    xhr.send(null);
}

function tryConnect(form) {
  remoting.accessCode = normalizeAccessCode(form['access_code_entry'].value);
  // TODO(jamiewalch): Since the mapping from (SupportId, HostSecret) to
  // AccessCode is not yet defined, assume it's hyphen-separated for now.
  var parts = remoting.accessCode.split('-');
  if (parts.length != 2) {
    showConnectError_(404);
  } else {
    setClientMode('connecting');
    if (remoting.oauth2.needsNewAccessToken()) {
      remoting.oauth2.refreshAccessToken(function() {
        resolveSupportId(parts[0]);
      });
      return;
    } else {
      resolveSupportId(parts[0]);
    }
  }
}

function cancelConnect() {
  remoting.accessCode = '';
  setClientMode('unconnected');
}
