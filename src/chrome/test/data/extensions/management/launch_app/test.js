// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  chrome.management.getAll(function(items) {
    for (var i in items) {
      var item = items[i];
      if (item.name == "packaged_app") {
        chrome.management.launchApp(item.id);
      }
      if (item.name == "simple_extension") {
        // Try launching a non-app extension, which should fail.
        var expected_error = "Extension " + item.id + " is not an App.";
        chrome.management.launchApp(item.id, function() {
          if (chrome.runtime.lastError &&
              chrome.runtime.lastError.message == expected_error) {
            chrome.test.sendMessage("got_expected_error");
          }
        });
      }
    }
  });
};

