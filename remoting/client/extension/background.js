// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function openChromotingTab(host_jid) {
  var username = getCookie('username');
  var xmpp_auth = getCookie('xmpp_auth');
  var new_tab_url = chrome.extension.getURL("chromoting_tab.html");
  var request = {
    username: getCookie('username'),
    xmpp_auth: getCookie('xmpp_auth'),
    host_jid: host_jid,
  };
  var tab_args = {
    url: new_tab_url,
  };

  console.log("Attempt to connect with" +
              " username='" + request.username + "'" +
              " host_jid='" + request.host_jid + "'" +
              " auth_token='" + request.xmpp_auth + "'");
  chrome.tabs.create(tab_args, function(tab) {
      console.log("We're trying now to send to " + tab.id);
      chrome.tabs.sendRequest(
          tab.id, request, function() {
            console.log('Tab finished connect.');
          });
    });
}
