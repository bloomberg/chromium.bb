// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

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

function sendRuntimeMessageToBackgroundScript(message, options) {
  return new Promise((resolve, reject) => {
      chrome.runtime.sendMessage(chrome.runtime.id, message, options,
                                 (response) => {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError);
        } else {
          resolve(response);
        }
      });
    });
}
