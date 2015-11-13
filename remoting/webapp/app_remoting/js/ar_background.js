// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {chrome.app.window.AppWindow} */
var mainWindow = null;

/**
 * The main window cannot delete its context menu entries on close because it
 * is being torn down at that point and doesn't have access to the necessary
 * APIs. Instead, it notifies the background page of the entries it creates,
 * and the background pages deletes them when the window is closed.
 *
 * @type {!Object}
 */
var contextMenuIds = {};

/** @param {Object=} opt_launchData */
function createWindow(opt_launchData) {
  // If there is already a window, give it focus.
  if (mainWindow) {
    mainWindow.focus();
    return;
  }

  var typed_screen = /** @type {{availWidth: number, availHeight: number}} */
      (screen);

  var windowAttributes = {
    resizable: false,
    frame: remoting.platformIsMac() ? 'chrome' : 'none',
    bounds: {
      width: typed_screen.availWidth,
      height: typed_screen.availHeight,
      left: undefined,
      top: undefined
    }
  };

  function onClosed() {
    mainWindow = null;
    var ids = Object.keys(contextMenuIds);
    for (var i = 0; i < ids.length; ++i) {
      chrome.contextMenus.remove(ids[i]);
    }
    contextMenuIds = {};
  };

  /** @param {chrome.app.window.AppWindow} appWindow */
  function onCreate(appWindow) {
    // Set the global window.
    mainWindow = appWindow;

    // Clean up the windows sub-menu when the application quits.
    appWindow.onClosed.addListener(onClosed);
  };

  // TODO(garykac) Add code to switch between dev and prod shared modules.
  chrome.app.window.create(
      '_modules/koejkfhmphamcgafjmkellhnekdkopod/main.html',
      windowAttributes, onCreate);
};

/** @param {Event} event */
function onWindowMessage(event) {
  var method = /** @type {string} */ (event.data['method']);
  var id = /** @type {string} */ (event.data['id']);
  switch (method) {
    case 'addContextMenuId':
      contextMenuIds[id] = true;
      break;
    case 'removeContextMenuId':
      delete contextMenuIds[id];
      break;
  }
};

chrome.app.runtime.onLaunched.addListener(createWindow);
window.addEventListener('message', onWindowMessage, false);
