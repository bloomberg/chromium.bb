// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Stores the app windows OLNY for test purpose.
// We SHOULD NOT use it as it is except for test, since the files which have
// the same name will be overridden each other.
var appWindowsForTest = {};

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  if (!launchData || !launchData.items || launchData.items.length == 0)
    return;

  var getFilePromises = launchData.items.map(function(item) {
    var entry = item.entry;
    return new Promise(function(fullfill, reject) {
      entry.file(
          function(file) {
            fullfill({
              entry: entry,
              file: file,
              fileUrl: window.URL.createObjectURL(file)
            });
          },
          function() {
            fullfill({entry: entry, file: null, fileUrl: null});
          });
    });
  });

  Promise.all(getFilePromises).then(function(results) {
    results = results.filter(function(result) { return result.file !== null; });
    if (results.length > 0)
      open(results);
  }.wrap(),
  function() {
    // TODO(yoshiki): handle error in a smarter way.
    open('', 'error');  // Empty URL shows the error message.
  }.wrap());
}.wrap());

/**
 * Opens player window.
 * @param {Array.<Object>} videos List of videos to play.
 **/
function open(videos) {
  chrome.app.window.create('video_player.html', {
    id: 'video',
    frame: 'none',
    singleton: false,
    minWidth: 480,
    minHeight: 270
  },
  function(createdWindow) {
    // Stores the window for test purpose.
    appWindowsForTest[videos[0].entry.name] = createdWindow;

    createdWindow.setIcon('images/200/icon.png');
    createdWindow.contentWindow.videos = videos;
    chrome.runtime.sendMessage({ready: true}, function() {});
  }.wrap());
}
