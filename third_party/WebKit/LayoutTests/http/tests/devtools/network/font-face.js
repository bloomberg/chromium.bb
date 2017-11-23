// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that a used font-face is reported and an unused font-face is not reported.\n`);
  await TestRunner.showPanel('network');

  function onRequest(eventType, event) {
    var request = event.data;
    if (request.name() === 'done') {
      TestRunner.completeTest();
      return;
    }
    TestRunner.addResult(eventType + ': ' + request.name());
  }
  TestRunner.networkManager.addEventListener(
      SDK.NetworkManager.Events.RequestStarted, onRequest.bind(null, 'RequestStarted'));
  TestRunner.networkManager.addEventListener(
      SDK.NetworkManager.Events.RequestFinished, onRequest.bind(null, 'RequestFinished'));

  await TestRunner.addIframe('resources/font-face.html');
})();
