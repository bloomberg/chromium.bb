// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function injectScripts(opt_tab) {
  chrome.tabs.executeScript({
    file: 'lib/axs_testing.js',
    allFrames: true
  }, function() {
    console.log('successfully injected axs_testing.js');
    chrome.tabs.executeScript({
      file: 'hide-images.js',
      allFrames: true
    }, function() {
      console.log('successfully injected script', opt_tab ? opt_tab.url : '');
      chrome.tabs.insertCSS({
        file: 'hide-images.css',
        allFrames: true
      }, function() {
        console.log('successfully injected css', opt_tab ? opt_tab.url : '');
        chrome.tabs.executeScript({
          code: 'toggleEnabled();',
          allFrames: true
        }, function() {
          console.log('created infobar');
          chrome.tabs.executeScript({
            code: 'createInfobar();'
          });
        });
      });
    });
  });
}

chrome.commands.onCommand.addListener(function(command) {
  console.log('command: ', command);
  if (command == 'example_keyboard_command') {
    injectScripts();
  }
});

chrome.browserAction.onClicked.addListener(function(tab) {
  injectScripts(tab);
});

