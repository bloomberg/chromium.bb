// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function initParams() {
  var hash;
  var hashes = window.location.href.slice(
      window.location.href.indexOf('?') + 1).split('&');

  // Prepopulate via cookies first.
  document.getElementById('xmpp_auth').value = getCookie('xmpp_auth');
  document.getElementById('chromoting_auth').value =
      getCookie('chromoting_auth');
  document.getElementById('username').value = getCookie('username');

  for(var i = 0; i < hashes.length; i++)
  {
    hash = hashes[i].split('=');
    if (hash[0] == 'xmpp_auth') {
      document.getElementById('xmpp_auth').value = hash[1];
      setCookie('xmpp_auth', hash[1]);

    } else if (hash[0] == "chromoting_auth") {
      document.getElementById('chromoting_auth').value = hash[1];
      setCookie('chromoting_auth', hash[1]);

    } else if (hash[0] == 'username') {
      document.getElementById('username').value = hash[1];
      setCookie('username', hash[1]);

    } else if (hash[0] == 'password') {
      document.getElementById('password').value = hash[1];

    } else if (hash[0] == 'host_jid') {
      document.getElementById('host_jid').value = hash[1];
    }
  }
}

function findHosts(form) {
  // If either cookie is missing, login first.
  if (getCookie('chromoting_auth') == null ||
      getCookie('xmpp_auth') == null) {
    doLogin(form.username.value, form.username.password, doListHosts);
  } else {
    doListHosts();
  }
}

function login(form) {
  doLogin(form.username.value, form.password.value);
}

function extractAuthToken(message) {
  var lines = message.split('\n');
  for (var i = 0; i < lines.length; i++) {
    if (lines[i].match('^Auth=.*')) {
      return lines[i].split('=')[1];
    }
  }

  console.log('Could not parse auth token in : "' + message + '"');
  return 'bad_token';
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
  xhr.send('accountType=HOSTED_OR_GOOGLE&Email=' + username + '&Passwd=' +
           password + '&service=' + service + '&source=chromoclient');
}

function doLogin(username, password, done) {
  var count = 2;
  var barrier = function() {
    count--;
    if (done && count == 0) {
      done();
    }
  }
  setCookie('username', username, 100);
  doGaiaLogin(username, password, 'chromoting',
              function(token1) {
                setCookie('chromoting_auth', token1, 100);
                document.getElementById('chromoting_auth').value = token1;
                barrier();
              });
  doGaiaLogin(username, password, 'chromiumsync',
              function(token) {
                setCookie('xmpp_auth', token, 100);
                document.getElementById('xmpp_auth').value = token;
                barrier();
              });
}

function doListHosts() {
  var xhr = new XMLHttpRequest();
  var token = getCookie('chromoting_auth');

  // Unhide host list.
  var hostlist_div = document.getElementById('hostlist_div');
  hostlist_div.style.display = "block";

  xhr.onreadystatechange = function() {
    if (xhr.readyState == 1) {
      hostlist_div.appendChild(document.createTextNode('Finding..'));
      hostlist_div.appendChild(document.createElement('br'));
    }
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status == 200) {
      parsed_response = JSON.parse(xhr.responseText);
      hostlist_div.appendChild(document.createTextNode('--Found Hosts--'));
      hostlist_div.appendChild(document.createElement('br'));
      appendHostLinks(parsed_response.data.items);
    } else {
      console.log('bad status on host list query: "' + xhr.status + ' ' +
                  xhr.statusText);
      hostlist_div.appendChild(document.createTextNode('!! Failed !!.  :\'('));
    }
  };

  xhr.open('GET', 'https://www.googleapis.com/chromoting/v1/@me/hosts');
  xhr.setRequestHeader('Content-Type', 'text/plain;charset=UTF-8');
  xhr.setRequestHeader('Authorization', 'GoogleLogin auth=' + token);
  xhr.send(null);
}

function appendHostLinks(hostlist) {
  // A host link entry should look like:
  //   - Host: <a onclick="openChromotingTab(host_jid); return false;">
  //   NAME (JID) </a> <br />
  var host;
  var host_link;
  var hostlist_div = document.getElementById('hostlist_div');

  // Cleanup the div
  hostlist_div.innerHTML = "";

  // Add the hosts.
  for(var i = 0; i < hostlist.length; ++i) {
    hostlist_div.appendChild(document.createTextNode('-*- Host: '));
    host = hostlist[i];
    host_link = document.createElement('a');
    // TODO(ajwong): Reenable once we figure out how to control a new tab.
    host_link.setAttribute('onclick', 'openChromotingTab(\'' + host.jabberId +
                           '\'); return false;');
    host_link.setAttribute('href', 'javascript:void(0)');
    host_link.appendChild(
        document.createTextNode(host.hostName + ' (' + host.hostId  + ', ' +
                                host.jabberId + ')'));
    hostlist_div.appendChild(host_link);
    hostlist_div.appendChild(document.createElement('br'));
  }
}

function connect(form) {
  openChromotingTab(form.host_jid);
}

function openChromotingTab(host_jid) {
  var background = chrome.extension.getBackgroundPage();
  background.openChromotingTab(host_jid);
}

function setAuthCookies(form) {
  var now = new Date();
  now.setTime(now.getTime() + 1000 * 60 * 60 * 24 * 365)

  setCookie('xmpp_auth', form.xmpp_auth.value, 100);
  setCookie('chromoting_auth', form.chromoting_auth.value, 100);
}
