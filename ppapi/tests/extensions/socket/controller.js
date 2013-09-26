// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var control_message;

function testAll() {
  var nacl_module = document.getElementById('nacl_module');
  // The plugin will start the corresponding test and post a message back when
  // the test is done. If the test has failed, the message is a description of
  // the error; otherwise the message is empty.
  nacl_module.postMessage(control_message);
}

var onControlMessageReceived = function(message) {
  control_message = message;
  chrome.test.runTests([testAll]);
}

var onPluginMessageReceived = function(message) {
  if (message.data == "ready") {
    chrome.test.sendMessage("info_please", onControlMessageReceived);
  } else if (message.data) {
    chrome.test.fail(message.data);
  } else {
    chrome.test.succeed();
  }
};

window.onload = function() {
  var nacl_module = document.getElementById('nacl_module');
  nacl_module.addEventListener("message", onPluginMessageReceived, false);
};

