// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Retrieve a set of variables in Chrome local storage.
 * @param {string|!Array<string>} keys a single key or a list of keys
 *     specifying the variables
 * @returns {!Promise<!Object>} Promise object with the variables in key-value
 *     mappings.
 */
function getChromeLocalStorageVariables(keys) {
  return new Promise((resolve, reject) => {
    chrome.storage.local.get(keys, (items) => {
      if (chrome.runtime.lastError) {
        reject(`Unable to get variables from local storage!`);
      } else {
        resolve(items);
      }
    });
  });
}

/**
 * Store a set of variables in Chrome local storage.
 * @param {!Object} items an objecy containing key/value pairs of variable
 *     names/ variable values to store in local storage.
 * @returns {!Promise<!Object>} Promise object denoting whether the operation
 * succeeded.
 */
function setChromeLocalStorageVariables(items) {
  return new Promise((resolve, reject) => {
    chrome.storage.local.set(items, () => {
      if (chrome.runtime.lastError) {
        reject(`Unable to set variables in local storage!`);
      } else {
        resolve();
      }
    });
  });
}

/**
 * Send a single message to the background script.
 * @param {!Object} message the message to send to the background script. This
 *     message should be a JSON-ifiable object.
 * @returns {!Promise<!Object>} Promise object containing the background
 * script's response.
 */
function sendRuntimeMessageToBackgroundScript(message) {
  return new Promise((resolve, reject) => {
    chrome.runtime.sendMessage(chrome.runtime.id, message, null,
                               (response) => {
      if (chrome.runtime.lastError) {
        reject(chrome.runtime.lastError);
      } else {
        resolve(response);
      }
    });
  });
}
