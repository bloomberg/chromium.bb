// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * If there is already a tab with the given URL, switch focus to it. Otherwise,
 * create a new tab and open the URL.
 *
 * @param url The URL to open.
 */
function focusOrCreateTab(url) {
  chrome.windows.getAll({"populate":true}, function(windows) {
      var existing_tab = null;
      for (var i in windows) {
        var tabs = windows[i].tabs;
        for (var j in tabs) {
          var tab = tabs[j];
          if (tab.url == url) {
            existing_tab = tab;
            break;
          }
        }
      }
      if (existing_tab) {
        chrome.tabs.update(existing_tab.id, {"selected": true});
      } else {
        chrome.tabs.create({"url": url, "selected": true});
      }
    });
}

/**
 * In the current tab, navigate to the specified URL.
 *
 * @param url The URL to navigate to.
 */
function navigate(url, callback) {
  chrome.tabs.getSelected(null, function(tab) {
      chrome.tabs.update(tab.id, {url: url}, callback);
  });
}

// Open the Chromoting HostList tab when
chrome.browserAction.onClicked.addListener(function(tab) {
    var hostlist_url = chrome.extension.getURL("hostlist.html");
    focusOrCreateTab(hostlist_url);
  });


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

  console.log("Attempt to connect with" +
              " username='" + request.username + "'" +
              " hostName='" + request.hostName + "'" +
              " hostJid='" + request.hostJid + "'" +
              " auth_token='" + request.xmppAuth + "'");

  var sendRequestFunc = function (tab) {
    console.log("We're trying now to send to " + tab.id);
    chrome.tabs.sendRequest(
        tab.id, request, function() {
          console.log('Tab finished connect.');
        });
  };

  // This function will run when after the url for the tab is updated. If
  // the tab is not yet loaded it will wait for another 500ms to inspect
  // again.
  var checkStatusFunc = function (tab) {
    if (tab.status == "complete") {
      sendRequestFunc(tab);
      return;
    }

    // Wait for 500ms and then get the tab and check its status.
    setTimeout(function() {
        chrome.tabs.get(tab.id, checkStatusFunc);
      }, 500);
  }

  navigate(newTabUrl, checkStatusFunc);
}
