// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getPowerSourceInfo() {
    chrome.system.powerSource.getPowerSourceInfo(
        chrome.test.callbackPass(info => {
          chrome.test.assertEq([{active: true, type: 'mains'}], info);
        }));
  },

  function onPowerChanged() {
    chrome.test.listenOnce(chrome.system.powerSource.onPowerChanged, info => {
      chrome.test.assertEq([{active: true, type: 'mains'}], info);
    });
    chrome.system.powerSource.requestStatusUpdate();
  },
]);
