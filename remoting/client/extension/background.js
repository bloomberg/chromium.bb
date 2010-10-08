// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function openChromotingTab(hostName, hostJid) {
  var username = getCookie('username');
  var xmppAuth = getCookie('xmpp_auth');
  var newTabUrl = chrome.extension.getURL("chromoting_tab.html");
  var request = {
    username: getCookie('username'),
    xmppAuth: getCookie('xmpp_auth'),
    hostName: hostName,
    hostJid: hostJid,
  };
  var tabArgs = {
    url: newTabUrl,
  };

  console.log("Attempt to connect with" +
              " username='" + request.username + "'" +
              " hostName='" + request.hostName + "'" +
              " hostJid='" + request.hostJid + "'" +
              " auth_token='" + request.xmppAuth + "'");
  chrome.tabs.create(tabArgs, function(tab) {
      console.log("We're trying now to send to " + tab.id);
      chrome.tabs.sendRequest(
          tab.id, request, function() {
            console.log('Tab finished connect.');
          });
    });
}
