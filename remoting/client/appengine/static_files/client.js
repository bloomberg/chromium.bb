// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flag to indicate whether or not to show offline hosts in the host list.
chromoting.showOfflineHosts = true;

// String to identify bad auth tokens.
var BAD_AUTH_TOKEN = 'bad_token';

// Number of days before auth token cookies expire.
// TODO(garykac): 21 days is arbitrary. Need to change this to the appropriate
// value from security review.
var AUTH_EXPIRES = 21;

function init() {
  updateLoginStatus();

  // Defer getting the host list for a little bit so that we don't
  // block the display of the extension popup.
  window.setTimeout(populateHostList, 100);

  var showOff = getCookie('offline');
  chromoting.showOfflineHosts = (!showOff || showOff == '1');
  updateShowOfflineCheckbox();
}

function updateShowOfflineHosts(cbox) {
  chromoting.showOfflineHosts = cbox.checked;

  // Save pref in cookie with long expiration.
  setCookie('offline', chromoting.showOfflineHosts ? '1' : '0', 1000);

  populateHostList();
}

function updateShowOfflineCheckbox() {
  var cbox = document.getElementById('show_offline');
  cbox.checked = chromoting.showOfflineHosts;
}

// Update the login status region (at the bottom of the popup) with the
// current account and links to sign in/out.
function updateLoginStatus() {
  var username = getCookie('username');

  var loginDiv = document.getElementById('login_div');
  clear(loginDiv);

  if (!username) {
    var signinLink = document.createElement('a');
    signinLink.setAttribute('href', 'javascript:showDirectoryLogin();');
    signinLink.appendChild(document.createTextNode('Sign In'));
    loginDiv.appendChild(signinLink);
  } else {
    var email = document.createElement('span');
    email.setAttribute('class', 'login_email');
    email.appendChild(document.createTextNode(username));
    loginDiv.appendChild(email);

    loginDiv.appendChild(document.createTextNode(' | '));

    var signoutLink = document.createElement('a');
    signoutLink.setAttribute('href', 'javascript:logoutAndReload(this.form);');
    signoutLink.appendChild(document.createTextNode('Sign Out'));
    loginDiv.appendChild(signoutLink);
  }
}

// Sign out the current user and reload the host list.
function logoutAndReload(form) {
  logout();
  populateHostList();
}

// Sign out the current user by erasing the auth cookies.
function logout() {
  setCookie('username', '', AUTH_EXPIRES);
  setCookie('chromoting_auth', '', AUTH_EXPIRES);
  setCookie('xmpp_auth', '', AUTH_EXPIRES);

  updateLoginStatus();
}

// Sign in to Chromoting Directory services.
function showDirectoryLogin() {
  document.getElementById("login_panel").style.display = "block";
}

function login() {
  var username = document.getElementById("username").value;
  var password = document.getElementById("password").value;

  doLogin(username, password, checkLogin);
}

// Check to see if the login was successful.
function checkLogin() {
  var username = getCookie('username');
  var cauth = getCookie('chromoting_auth');
  var xauth = getCookie('xmpp_auth');

  // Verify login and show login status.
  if (cauth == BAD_AUTH_TOKEN || xauth == BAD_AUTH_TOKEN) {
    // Erase the username cookie.
    setCookie('username', '', AUTH_EXPIRES);
    showLoginError("Sign in failed!");
  } else {
    // Successful login - update status and update host list.
    updateLoginStatus();
    populateHostList();

    // Hide login dialog and clear out values.
    document.getElementById('login_panel').style.display = "none";
    document.getElementById('username').value = "";
    document.getElementById('password').value = "";
  }
}

function doLogin(username, password, done) {
  // Don't call |done| callback until both login requests have completed.
  var count = 2;
  var barrier = function() {
    count--;
    if (done && count == 0) {
      done();
    }
  }
  setCookie('username', username, AUTH_EXPIRES);
  doGaiaLogin(username, password, 'chromoting',
              function(cAuthToken) {
                setCookie('chromoting_auth', cAuthToken, AUTH_EXPIRES);
                barrier();
              });
  doGaiaLogin(username, password, 'chromiumsync',
              function(xAuthToken) {
                setCookie('xmpp_auth', xAuthToken, AUTH_EXPIRES);
                barrier();
              });
}

function doGaiaLogin(username, password, service, done) {
  var xhr = new XMLHttpRequest();
  xhr.open('POST', 'https://www.google.com/accounts/ClientLogin', true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status = 200) {
      done(extractAuthToken(xhr.responseText));
    } else {
      console.log('Bad status on auth: ' + xhr.statusText);
    }
  };

  xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
  xhr.send('accountType=HOSTED_OR_GOOGLE&Email=' +
           encodeURIComponent(username) + '&Passwd=' +
           encodeURIComponent(password) + '&service=' +
           encodeURIComponent(service) + '&source=chromoclient');
}

function showLoginError() {
  var errorDiv = document.getElementById('errormsg_div');
  clear(errorDiv);

  errorDiv.appendChild(document.createTextNode(
    "The username or password you entered is incorrect ["));

  var helpLink = document.createElement('a');
  helpLink.setAttribute('href',
      'http://www.google.com/support/accounts/bin/answer.py?answer=27444');
  helpLink.setAttribute('target', '_blank');
  helpLink.appendChild(document.createTextNode('?'));
  errorDiv.appendChild(helpLink);

  errorDiv.appendChild(document.createTextNode("]"));
}

function extractAuthToken(message) {
  var lines = message.split('\n');
  for (var i = 0; i < lines.length; i++) {
    if (lines[i].match('^Auth=.*')) {
      return lines[i].split('=')[1];
    }
  }

  console.log('Could not parse auth token in : "' + message + '"');
  return BAD_AUTH_TOKEN;
}

// Open a chromoting connection in a new tab.
function openChromotingTab(hostName, hostJid) {
  var background = chrome.extension.getBackgroundPage();
  background.openChromotingTab(hostName, hostJid);
}

// Erase the content of the specified element.
function clear(e) {
  e.innerHTML = '';
}

// Clear out the specified element and show the message to the user.
function displayMessage(e, classname, message) {
  clear(e);
  appendMessage(e, classname, message);
}

// Append the message text to the specified element.
function appendMessage(e, classname, message) {
  var p = document.createElement('p');
  if (classname.length != 0) {
    p.setAttribute('class', classname);
  }

  p.appendChild(document.createTextNode(message));

  e.appendChild(p);
}

function populateHostList() {
  var hostlistDiv = document.getElementById('hostlist_div');
  displayMessage(hostlistDiv, 'message',
      'Hosts will appaer if Chromoting Token is "OK".');

  var xhr = new XMLHttpRequest();
  // Unhide host list.
  hostlistDiv.style.display = "block";

  xhr.onreadystatechange = function() {
    if (xhr.readyState == 1) {
      displayMessage(hostlistDiv, 'message', 'Loading host list');
    }
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status == 200) {
      var parsed_response = JSON.parse(xhr.responseText);
      appendHostLinks(parsed_response.data.items);
    } else {
      var errorResponse = JSON.parse(xhr.responseText);

      console.log('Error: Bad status on host list query: "' +
                  xhr.status + ' ' + xhr.statusText);
      console.log('Error code ' + errorResponse.error.code);
      console.log('Error message ' + errorResponse.error.message);

      clear(hostlistDiv);
      if (errorResponse.error.message == "Token expired") {
        appendMessage(hostlistDiv, 'message',
                      'Authentication token expired. Please sign in again.');
        logout();
      } else if (errorResponse.error.message == "Token invalid") {
        appendMessage(hostlistDiv, 'message',
                      'Invalid authentication token. Please sign in again.');
        logout();
      } else {
        appendMessage(hostlistDiv, 'message',
                      'Unable to load host list. Please try again later.');
        appendMessage(hostlistDiv, 'message',
                      'Error code: ' + errorResponse.error.code);
        appendMessage(hostlistDiv, 'message',
                      'Message: ' + errorResponse.error.message);
      }
    }
  };

  xhr.open('GET', 'api/get_host_list', true);
  xhr.setRequestHeader('Content-Type', 'text/plain;charset=UTF-8');
  xhr.send(null);
}

// Populate the 'hostlist_div' element with the list of hosts for this user.
function appendHostLinks(hostlist) {
  var hostlistDiv = document.getElementById('hostlist_div');

  // Clear the div before adding the host info.
  clear(hostlistDiv);

  var numHosts = 0;
  var numOfflineHosts = 0;

  // Add the hosts.
  // TODO(garykac): We should have some sort of MRU list here or have
  // the Chromoting Directory provide a MRU list.
  // First, add all of the connected hosts.
  for (var i = 0; i < hostlist.length; ++i) {
    if (hostlist[i].status == "ONLINE") {
      hostlistDiv.appendChild(addHostInfo(hostlist[i]));
      numHosts++;
    }
  }
  // Add non-online hosts at the end.
  for (var i = 0; i < hostlist.length; ++i) {
    if (hostlist[i].status != "ONLINE") {
      if (chromoting.showOfflineHosts == 1) {
        hostlistDiv.appendChild(addHostInfo(hostlist[i]));
        numHosts++;
      }
      numOfflineHosts++;
    }
  }

  if (numHosts == 0) {
    var message;
    if (numOfflineHosts == 0) {
      message = 'No hosts available.';
    } else {
      message = 'No online hosts available (' +
          numOfflineHosts + ' offline hosts).';
    }
    message += ' See LINK for info on how to set up a new host.';
    displayMessage(hostlistDiv, 'message', message);
  }
}

// Create a single host description element.
function addHostInfo(host) {
  var hostEntry = document.createElement('div');
  hostEntry.setAttribute('class', 'hostentry');

  var hostIcon = document.createElement('img');
  hostIcon.setAttribute('src', 'static_files/machine.png');
  hostIcon.setAttribute('class', 'hosticon');
  hostEntry.appendChild(hostIcon);

  if (host.status == 'ONLINE') {
    var span = document.createElement('span');
    span.setAttribute('class', 'connect');
    var connect = document.createElement('input');
    connect.setAttribute('type', 'button');
    connect.setAttribute('value', 'Connect');
    connect.setAttribute('onclick', "window.open('session?hostname=" +
        encodeURIComponent(host.hostName) + "&hostjid=" +
        encodeURIComponent(host.jabberId) + "');");
    span.appendChild(connect);
    hostEntry.appendChild(span);
  }

  var hostName = document.createElement('p');
  hostName.setAttribute('class', 'hostindent hostname');
  hostName.appendChild(document.createTextNode(host.hostName));
  hostEntry.appendChild(hostName);

  var hostStatus = document.createElement('p');
  hostStatus.setAttribute('class', 'hostindent hostinfo hoststatus_' +
                          ((host.status == 'ONLINE') ? 'good' : 'bad'));
  hostStatus.appendChild(document.createTextNode(host.status));
  hostEntry.appendChild(hostStatus);

  var hostInfo = document.createElement('p');
  hostInfo.setAttribute('class', 'hostindent hostinfo');
  hostInfo.appendChild(document.createTextNode(host.jabberId));
  hostEntry.appendChild(hostInfo);

  return hostEntry;
}
