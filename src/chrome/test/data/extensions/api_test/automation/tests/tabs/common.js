// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;

var rootNode = null;
var url = '';

function createTab(url, callback) {
  chrome.tabs.create({"url": url}, function(tab) {
    callback(tab);
  });
}

function listenOnce(node, eventType, callback, capture) {
  var innerCallback = function(evt) {
    node.removeEventListener(eventType, innerCallback, capture);
    callback(evt);
  };
  node.addEventListener(eventType, innerCallback, capture);
}

function setUpAndRunTests(allTests, opt_path) {
  var path = opt_path || 'index.html';
  getUrlFromConfig(path, function(url) {
    createTab(url, function(unused_tab) {
      chrome.automation.getTree(function (returnedRootNode) {
        rootNode = returnedRootNode;
        if (rootNode.docLoaded) {
          chrome.test.runTests(allTests);
          return;
        }
        rootNode.addEventListener('loadComplete', function() {
          chrome.test.runTests(allTests);
        });
      });
    });
  });
}

function getUrlFromConfig(path, callback) {
  chrome.test.getConfig(function(config) {
    assertTrue('testServer' in config, 'Expected testServer in config');
    url = ('http://a.com:PORT/' + path)
        .replace(/PORT/, config.testServer.port);
    callback(url)
  });
}
