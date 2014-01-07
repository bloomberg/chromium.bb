// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var NaClModulesExpected = 0;
var NaClModulesLoaded = 0;

// Indicate load success.
function moduleDidLoad() {
  NaClModulesLoaded++;
  if (NaClModulesLoaded == NaClModulesExpected)
    chrome.test.sendMessage("nacl_modules_loaded", handleChromeTestMessage);
}

var handleChromeTestMessage = function (message) {
  NaClModules = document.querySelectorAll('embed');
  for (var i = 0; i < NaClModules.length; i++) {
    NaClModules[i].postMessage(message);
  }
}

function handleNaclMessage(message_event) {
  console.log("handleNaclMessage: " + message_event.data);
}

function createNaClEmbed() {
  NaClModulesExpected++;

  var listener = document.createElement("div");
  listener.addEventListener("load", moduleDidLoad, true);
  listener.addEventListener("message", handleNaclMessage, true);
  listener.innerHTML = '<embed' +
    ' src="ppapi_tests_extensions_background_keepalive.nmf"' +
    ' type="application/x-nacl" />';
  document.body.appendChild(listener);
}

// Create 2 embeds to verify that we can handle more than one.
createNaClEmbed();
createNaClEmbed();

