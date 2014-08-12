// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function(){

/** @return {boolean} */
function isAppsV2() {
  var manifest = chrome.runtime.getManifest();
  if (manifest && manifest.app && manifest.app.background) {
    return true;
  }
  return false;
}

/** @param {remoting.AppLauncher} appLauncher */
function initializeAppV2(appLauncher) {
  /** @type {string} */
  var kNewWindowId = 'new-window';

  /** @param {OnClickData} info */
  function onContextMenu(info) {
    if (info.menuItemId == kNewWindowId) {
      appLauncher.launch();
    }
  }

  function initializeContextMenu() {
    chrome.contextMenus.create({
       id: kNewWindowId,
       contexts: ['launcher'],
       title: chrome.i18n.getMessage(/*i18n-content*/'NEW_WINDOW')
    });
    chrome.contextMenus.onClicked.addListener(onContextMenu);
  }

  initializeContextMenu();
  chrome.app.runtime.onLaunched.addListener(
      appLauncher.launch.bind(appLauncher)
  );
}

function main() {
  /** @type {remoting.AppLauncher} */
  var appLauncher = new remoting.V1AppLauncher();
  if (isAppsV2()) {
    appLauncher = new remoting.V2AppLauncher();
    initializeAppV2(appLauncher);
  }
}

window.addEventListener('load', main, false);

}());
