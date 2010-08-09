// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function init_params() {
  var hash;
  var hashes = window.location.href.slice(
      window.location.href.indexOf('?') + 1).split('&');

  // Prepopulate via cookies first.
  document.getElementById('xmpp_auth').value = get_cookie('xmpp_auth');
  document.getElementById('chromoting_auth').value = get_cookie('chromoting_auth');
  document.getElementById('username').value = get_cookie('username');

  for(var i = 0; i < hashes.length; i++)
  {
    hash = hashes[i].split('=');
    if (hash[0] == 'xmpp_auth') {
      document.getElementById('xmpp_auth').value = hash[1];
      set_cookie('xmpp_auth', hash[1]);

    } else if (hash[0] == "chromoting_auth") {
      document.getElementById('chromoting_auth').value = hash[1];
      set_cookie('chromoting_auth', hash[1]);

    } else if (hash[0] == 'username') {
      document.getElementById('username').value = hash[1];
      set_cookie('username', hash[1]);

    } else if (hash[0] == 'password') {
      document.getElementById('password').value = hash[1];

    } else if (hash[0] == 'host_jid') {
      document.getElementById('host_jid').value = hash[1];

    }
  }
}

function find_hosts(form) {
  // If either cookie is missing, login first.
  if (get_cookie('chromoting_auth') == null || get_cookie('xmpp_auth') == null) {
    do_login(form.username.value, form.username.password, do_list_hosts);
  } else {
    do_list_hosts();
  }
}

function login(form) {
  do_login(form.username.value, form.password.value);
}

function extract_auth_token(message) {
  var lines = message.split('\n');
  for (var i = 0; i < lines.length; i++) {
    if (lines[i].match('^Auth=.*')) {
      return lines[i].split('=')[1];
    }
  }

  console.log('Could not parse auth token in : "' + message + '"');
  return 'bad_token';
}

function do_gaia_login(username, password, service, done) {
  var xhr = new XMLHttpRequest();
  xhr.open('POST', 'https://www.google.com/accounts/ClientLogin', true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status = 200) {
      done(extract_auth_token(xhr.responseText));
    } else {
      console.log('Bad status on auth: ' + xhr.statusText);
    }
  };

  xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
  xhr.send('accountType=HOSTED_OR_GOOGLE&Email=' + username + '&Passwd=' + password + '&service=' + service + '&source=chromoclient');
}

function do_login(username, password, done) {
  var count = 2;
  var barrier = function() {
    count--;
    if (done && count == 0) {
      done();
    }
  }
  set_cookie('username', username, 100);
  do_gaia_login(username, password, 'chromoting',
                function(token1) {
                  set_cookie('chromoting_auth', token1, 100);
                  document.getElementById('chromoting_auth').value = token1;
                  barrier();
                });
  do_gaia_login(username, password, 'chromiumsync',
                function(token) {
                  set_cookie('xmpp_auth', token, 100);
                  document.getElementById('xmpp_auth').value = token;
                  barrier();
                });
}

function do_list_hosts() {
  var xhr = new XMLHttpRequest();
  var token = get_cookie('chromoting_auth');

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
      append_host_links(parsed_response.data.items);
    } else {
      console.log('bad status on host list query: "' + xhr.status + ' ' + xhr.statusText);
      hostlist_div.appendChild(document.createTextNode('!! Failed !!.  :\'('));
    }
  };

  xhr.open('GET', 'http://www-googleapis-test.sandbox.google.com/chromoting/v1/@me/hosts');
  xhr.setRequestHeader('Content-Type', 'text/plain;charset=UTF-8');
  xhr.setRequestHeader('Authorization', 'GoogleLogin auth=' + token);
  xhr.send(null);
}

function append_host_links(hostlist) {
// A host link entry should look like:
// - Host: <a onclick="open_chromoting_tab(host_jid); return false;">NAME (JID)</a> <br />
  var host;
  var host_link;
  var hostlist_div = document.getElementById('hostlist_div');

  // Add the hosts.
  for(var i = 0; i < hostlist.length; ++i) {
    hostlist_div.appendChild(document.createTextNode('-*- Host: '));
    host = hostlist[i];
    host_link = document.createElement('a');
    // TODO(ajwong): Reenable once we figure out how to control a new tab.
    host_link.setAttribute('onclick', 'open_chromoting_tab(\'' + host.jabberId + '\'); return false;');
    host_link.setAttribute('href', 'javascript:void(0)');
    host_link.appendChild(document.createTextNode(host.hostName + ' (' + host.hostId  + ', ' + host.jabberId + ')'));
    hostlist_div.appendChild(host_link);
    hostlist_div.appendChild(document.createElement('br'));
  }
}

// Cookie reading code taken from quirksmode with modification for escaping.
//   http://www.quirksmode.org/js/cookies.html
function set_cookie(name,value,days) {
  if (days) {
    var date = new Date();
    date.setTime(date.getTime()+(days*24*60*60*1000));
    var expires = "; expires="+date.toGMTString();
  }
  else var expires = "";
  document.cookie = name+"="+escape(value)+expires+"; path=/";
}

function get_cookie(name) {
  var nameEQ = name + "=";
  var ca = document.cookie.split(';');
  for(var i=0;i < ca.length;i++) {
    var c = ca[i];
    while (c.charAt(0)==' ') c = c.substring(1,c.length);
    if (c.indexOf(nameEQ) == 0) return unescape(c.substring(nameEQ.length,c.length));
  }
  return null;
}

function set_auth_cookies(form) {
  var now = new Date();
  now.setTime(now.getTime() + 1000 * 60 * 60 * 24 * 365)

  create_cookie('xmpp_auth', form.xmpp_auth.value, 100);
  create_cookie('chromoting_auth', form.chromoting_auth.value, 100);
}

function connect(form) {
  open_chromoting_tab(form.host_jid.value);
}

function debug_output(message) {
  var debug_div = document.getElementById('debug_div');
  debug_div.appendChild(document.createTextNode(message));
  debug_div.appendChild(document.createElement('br'));
}

function open_chromoting_tab(host_jid) {
  var username = get_cookie('username');
  var xmpp_auth = get_cookie('xmpp_auth');
  var new_tab_url = chrome.extension.getURL("chromoting_tab.html");
  var request = {
    username: get_cookie('username'),
    xmpp_auth: get_cookie('xmpp_auth'),
    host_jid: host_jid,
  };
  var tab_args = {
    url: new_tab_url,
    selected: false,
  };

  console.log("Attempt to connect with " +
              "username='" + request.username + "'" +
              " host_jid='" + request.host_jid + "'" +
              " auth_token='" + request.xmpp_auth + "'");

  // TODO(sergeyu): Currently we open a new tab, send a message and only after
  // that select the new tab. This is neccessary because chrome closes the
  // popup the moment the new tab is selected, and so we fail to send the
  // message if the tab is selected when it is created. There is visible delay
  // when opening a new tab, so it is necessary to pass the message somehow
  // differently. Figure out how.
  chrome.tabs.create(tab_args, function(tab) {
    console.log("We're trying now to send to " + tab.id);
    chrome.tabs.sendRequest(
      tab.id, request, function() {
        console.log('Tab finished connect.');
        chrome.tabs.update(tab.id, {selected: true});
      });
  });
}
