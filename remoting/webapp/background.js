// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {string} */
var kNewWindowId = 'new-window';

function createWindow() {
  chrome.app.window.create('main.html', {
    'width': 800,
    'height': 600,
    'frame': 'none'
  });
};

/** @param {OnClickData} info */
function onContextMenu(info) {
  if (info.menuItemId == kNewWindowId) {
    createWindow();
  }
};

function initializeContextMenu() {
  chrome.contextMenus.create({
     id: kNewWindowId,
     contexts: ['launcher'],
     title: chrome.i18n.getMessage(/*i18n-content*/'NEW_WINDOW')
  });
}

chrome.app.runtime.onLaunched.addListener(createWindow);
chrome.contextMenus.onClicked.addListener(onContextMenu);
initializeContextMenu();
