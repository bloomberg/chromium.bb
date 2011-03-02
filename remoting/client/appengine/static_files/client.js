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
  var showOff = getCookie('offline');
  chromoting.showOfflineHosts = (!showOff || showOff == '1');

  var cbox = document.getElementById('show_offline');
  cbox.checked = chromoting.showOfflineHosts;

  populateHostList();
}

function updateShowOfflineHosts(cbox) {
  chromoting.showOfflineHosts = cbox.checked;

  // Save pref in cookie with long expiration.
  setCookie('offline', chromoting.showOfflineHosts ? '1' : '0', 1000);

  populateHostList();
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
  var hostlistDiv = document.getElementById('hostlist-div');
  displayMessage(hostlistDiv, 'message',
      'Hosts will appear if Chromoting Token is "OK".');

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

    try {
      if (xhr.status == 200) {
        var parsed_response = JSON.parse(xhr.responseText);
        appendHostLinks(parsed_response.data.items);
      } else if (xhr.status == 400) {
        clear(hostlistDiv);
        appendMessage(hostlistDiv, 'message',
                      'Unable to authenticate. Please sign in again.');
      } else if (xhr.status == 401) {
        clear(hostlistDiv);
        appendMessage(hostlistDiv, 'message',
                      'Please use your google.com account for your ' +
                      'chromoting token.');
      } else {
        var errorResponse = JSON.parse(xhr.responseText);

        console.log('Error: Bad status on host list query: ' +
                    xhr.status + ' ' + xhr.statusText);
        console.log('Error code ' + errorResponse.error.code);
        console.log('Error message ' + errorResponse.error.message);

        clear(hostlistDiv);
        if (errorResponse.error.message == "Token expired") {
          appendMessage(hostlistDiv, 'message',
                        'Authentication token expired. Please sign in again.');
        } else if (errorResponse.error.message == "Token invalid") {
          appendMessage(hostlistDiv, 'message',
                        'Invalid authentication token. Please sign in again.');
        } else {
          appendMessage(hostlistDiv, 'message',
                        'Unable to load host list. Please try again later.');
          appendMessage(hostlistDiv, 'message',
                        'Error: ' + errorResponse.error.code);
          appendMessage(hostlistDiv, 'message',
                        'Message: ' + errorResponse.error.message);
        }
      }
    } catch(er) {
      // Here because the reponse could not be parsed.
      console.log('Error: Bad status on host list query: "' +
                  xhr.status + ' ' + xhr.statusText);
      console.log(xhr.responseText);
      clear(hostlistDiv);
      appendMessage(hostlistDiv, 'message',
                    'Unable to load host list. Please try again later.');
      appendMessage(hostlistDiv, 'message',
                    'Error: ' + xhr.statusText + ' (' + xhr.status + ')');
    }
  };

  xhr.open('GET', 'api/get_host_list', true);
  xhr.setRequestHeader('Content-Type', 'text/plain;charset=UTF-8');
  xhr.send(null);
}

// Populate the 'hostlist-div' element with the list of hosts for this user.
function appendHostLinks(hostlist) {
  var hostlistDiv = document.getElementById('hostlist-div');

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
      message = 'No hosts available.' +
                ' See LINK for info on how to set up a new host.';
    } else {
      message = 'No online hosts available (' +
          numOfflineHosts + ' offline hosts).';
    }
    displayMessage(hostlistDiv, 'message', message);
  }
}

// Create a single host description element.
function addHostInfo(host) {
  var hostEntry = document.createElement('div');
  hostEntry.setAttribute('class', 'hostentry');

  var hostIcon = document.createElement('img');
  hostIcon.className = "hosticon";
  hostIcon.style.height = 64;
  hostIcon.style.width = 64;
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
    hostIcon.setAttribute('src', 'static_files/online.png');
  } else {
    hostIcon.setAttribute('src', 'static_files/offline.png');
  }

  var hostName = document.createElement('p');
  hostName.setAttribute('class', 'hostindent hostname');
  hostName.appendChild(document.createTextNode(host.hostName));
  hostEntry.appendChild(hostName);

  var hostStatus = document.createElement('p');
  hostStatus.setAttribute('class', 'hostindent hostinfo hoststatus-' +
                          ((host.status == 'ONLINE') ? 'good' : 'bad'));
  hostStatus.appendChild(document.createTextNode(host.status));
  hostEntry.appendChild(hostStatus);

  var hostInfo = document.createElement('p');
  hostInfo.setAttribute('class', 'hostindent hostinfo');
  hostInfo.appendChild(document.createTextNode(host.jabberId));
  hostEntry.appendChild(hostInfo);

  return hostEntry;
}
