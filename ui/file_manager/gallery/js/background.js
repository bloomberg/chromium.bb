// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var stringData = new Promise(function(fulfill) {
  chrome.fileBrowserPrivate.getStrings(function(stringData) {
    loadTimeData.data = stringData;
    fulfill(stringData);
  });
});

// VolumeManager should be obtained after stringData initialized.
var volumeManager = stringData.then(function() {
  return new Promise(function(fulfill) {
    VolumeManager.getInstance(fulfill);
  });
});

var backgroundComponent = Promise.all([stringData, volumeManager]).
    then(function(args) {
      return {
        stringData: args[0],
        volumeManager: args[1]
      };
    });

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  // Skip if files are not selected.
  if (!launchData || !launchData.items || launchData.items.length == 0)
    return;

  // Open application window.
  chrome.app.window.create(
      'gallery.html',
      {
        id: 'video',
        minWidth: 160,
        minHeight: 100,
        frame: 'none'
      },
      function(appWindow) {
        appWindow.contentWindow.launchData = launchData;
        appWindow.contentWindow.backgroundComponent = backgroundComponent;
      });
});
