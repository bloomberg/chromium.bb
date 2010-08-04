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

  debug_output('Could not parse auth token in : "' + message + '"');
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
      debug_output('Bad status on auth: ' + xhr.statusText);
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
  xhr.onreadystatechange = function() {
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status == 200) {
      parsed_response = JSON.parse(xhr.responseText);
      create_host_links(parsed_response.data.items);
    } else {
      debug_output('bad status on host list query: "' + xhr.status + ' ' + xhr.statusText);
    }
  };

  xhr.open('GET', 'http://www-googleapis-test.sandbox.google.com/chromoting/v1/@me/hosts');
  xhr.setRequestHeader('Content-Type', 'text/plain;charset=UTF-8');
  xhr.setRequestHeader('Authorization', 'GoogleLogin auth=' + token);
  xhr.send(null);
}

function create_host_links(hostlist) {
// A host link entry should look like:
// - Host: <a onclick="open_chromoting_tab(host_jid); return false;">NAME (JID)</a> <br />
  var host;
  var host_link;
  var hostlist_div = document.getElementById('hostlist_div');
  for(var i = 0; i < hostlist.length; ++i) {
    hostlist_div.appendChild(document.createTextNode('-*- Host: '));
    host = hostlist[i];
    host_link = document.createElement('a');
    // TODO(ajwong): Reenable once we figure out how to control a new tab.
    //host_link.setAttribute('onclick', 'open_chromoting_tab(\'' + host.jabberId + '\'); return false;');
    host_link.setAttribute('onclick', 'connect_in_popup(\'' + host.jabberId + '\'); return false;');
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
  // TODO(ajwong): reenable once we figure out how to command the DOM in
  // the opened tab.
  //
  // open_chromoting_tab(form.host_jid.value);
  connect_in_popup(form.host_jid.value);
}

function debug_output(message) {
  var debug_div = document.getElementById('debug_div');
  debug_div.appendChild(document.createTextNode(message));
  debug_div.appendChild(document.createElement('br'));
}

function connect_in_popup(host_jid) {
  var username = get_cookie('username');
  var xmpp_auth = get_cookie('xmpp_auth');
  debug_output("Attempt to connect with " +
               "username='" + username + "'" +
               " host_jid='" + host_jid + "'" +
               " auth_token='" + xmpp_auth + "'");

  document.getElementById('chromoting').connect(username, host_jid, xmpp_auth);
}

function open_chromoting_tab(host_jid) {
  var username = get_cookie('username');
  var xmpp_auth = get_cookie('xmpp_auth');
  debug_output("Attempt to connect with " +
               "username='" + username + "'" +
               " host_jid='" + host_jid + "'" +
               " auth_token='" + xmpp_auth + "'");

  var tab_args = {
    url: "chrome://remoting",
  };

  chrome.tabs.create(tab_args, function(tab) {
    var details = {};
    details.code = function() {
      // TODO(ajwong): We need to punch a hole into the content script to
      // make this work. See
      //   http://code.google.com/chrome/extensions/content_scripts.html
      var an_event = document.createEvent('Event');
      an_event.initEvent('startPlugin', true, true);

      alert('hi');
    }
    chrome.tabs.executeScript(tab.id, details, function() { alert('done');});
  });
}
