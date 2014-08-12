// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * AppLauncher is an interface that allows the client code to launch and close
 * the app without knowing the implementation difference between a v1 app and
 * a v2 app.
 *
 * To launch an app:
 *   var appLauncher = new remoting.V1AppLauncher();
 *   var appId = "";
 *   appLauncher.launch({arg1:'someValue'}).then(function(id){
 *    appId = id;
 *   });
 *
 * To close an app:
 *   appLauncher.close(appId);
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @interface */
remoting.AppLauncher = function() {};

/**
 * @param {Object=} opt_launchArgs
 * @return {Promise} The promise will resolve when the app is launched.  It will
 * provide the caller with the appId (which is either the id of the hosting tab
 * or window). The caller can use the appId to close the app.
 */
remoting.AppLauncher.prototype.launch = function(opt_launchArgs) { };

/**
 * @param {string} id  The id of the app to close.
 * @return {Promise} The promise will resolve when the app is closed.
 */
remoting.AppLauncher.prototype.close = function(id) {};

/**
 * @constructor
 * @implements {remoting.AppLauncher}
 */
remoting.V1AppLauncher = function() {};

remoting.V1AppLauncher.prototype.launch = function(opt_launchArgs) {
  var url = base.urlJoin('main.html', opt_launchArgs);

  /**
   * @param {function(*=):void} resolve
   * @param {function(*=):void} reject
   */
  return new Promise(function(resolve, reject) {
      chrome.tabs.create({ url: url, selected: true },
        /** @param {chrome.Tab} tab The created tab. */
        function(tab) {
          if (!tab) {
            reject(new Error(chrome.runtime.lastError.message));
          } else {
            resolve(tab.id);
          }
      });
  });
};

remoting.V1AppLauncher.prototype.close = function(id) {
  /**
   * @param {function(*=):void} resolve
   * @param {function(*=):void} reject
   */
  return new Promise(function(resolve, reject) {
    /** @param {chrome.Tab} tab The retrieved tab. */
    chrome.tabs.get(id, function(tab) {
      if (!tab) {
        reject(new Error(chrome.runtime.lastError.message));
      } else {
        chrome.tabs.remove(tab.id, resolve);
      }
    });
  });
};


/**
 * @constructor
 * @implements {remoting.AppLauncher}
 */
remoting.V2AppLauncher = function() {};

/**
 * @type {number}
 * @private
 */
remoting.V2AppLauncher.nextWindowId_ = 0;

remoting.V2AppLauncher.prototype.launch = function(opt_launchArgs) {
  var url = base.urlJoin('main.html', opt_launchArgs);

  /**
   * @param {function(*=):void} resolve
   * @param {function(*=):void} reject
   */
  return new Promise(function(resolve, reject) {
    chrome.app.window.create(url, {
        'width': 800,
        'height': 600,
        'frame': 'none',
        'id': String(remoting.V2AppLauncher.nextWindowId_++)
      },
      /** @param {AppWindow} appWindow */
      function(appWindow) {
        if (!appWindow) {
          reject(new Error(chrome.runtime.lastError.message));
        } else {
          resolve(appWindow.id);
        }
      });
  });
};

remoting.V2AppLauncher.prototype.close = function(id) {
  var appWindow = chrome.app.window.get(id);
  if (!appWindow) {
    return Promise.reject(new Error(chrome.runtime.lastError.message));
  }
  appWindow.close();
  return Promise.resolve();
};
