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

  var entry = launchData.items[0].entry;
  entry.file(function(file) {
    var url = window.URL.createObjectURL(file);
    open(url, entry.name);
  }.wrap(),
  function() {
    // TODO(yoshiki): handle error in a smarter way.
    open('', 'error');  // Empty URL shows the error message.
  }.wrap());
}.wrap());

function open(url, title) {
  chrome.app.window.create('video_player.html', {
    id: 'video',
    singleton: false,
    minWidth: 160,
    minHeight: 100
  },
  function(createdWindow) {
    // Stores the window for test purpose.
    appWindowsForTest[title] = createdWindow;

    createdWindow.setIcon('images/200/icon.png');
    createdWindow.contentWindow.videoUrl = url;
    createdWindow.contentWindow.videoTitle = title;
  }.wrap());
}
