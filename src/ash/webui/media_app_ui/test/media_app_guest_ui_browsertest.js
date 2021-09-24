// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for chrome-untrusted://media-app. */

import {GUEST_TEST} from './guest_query_receiver.js';

export function eventToPromise(eventType, target) {
  return new Promise(function(resolve, reject) {
    target.addEventListener(eventType, function f(e) {
      target.removeEventListener(eventType, f);
      resolve(e);
    });
  });
}

// Test web workers can be spawned from chrome-untrusted://media-app. Errors
// will be logged in console from web_ui_browser_test.cc.
GUEST_TEST('GuestCanSpawnWorkers', async () => {
  const worker = new Worker('test_worker.js');
  const workerResponse = new Promise((resolve, reject) => {
    /**
     * Registers onmessage event handler.
     * @param {MessageEvent} event Incoming message event.
     */
    worker.onmessage = resolve;
    worker.onerror = reject;
  });

  const MESSAGE = 'ping/pong message';

  worker.postMessage(MESSAGE);

  assertEquals('ping/pong message', (await workerResponse).data);
});

// Test that language is set correctly on the guest iframe.
GUEST_TEST('GuestHasLang', () => {
  assertEquals(document.documentElement.lang, 'en-US');
});

GUEST_TEST('GuestLoadsLoadTimeData', () => {
  const loadTimeData = window['loadTimeData'];
  // Check `LoadTimeData` exists on the global window object.
  chai.assert.isTrue(loadTimeData !== undefined);
  // Check data loaded into `LoadTimeData` by "strings.js" via
  // `source->UseStringsJs()` exists.
  assertEquals(loadTimeData.getValue('appLocale'), 'en-US');
});

// Test can load files with CSP restrictions. We expect `error` to be called
// as these tests are loading resources that don't exist. Note: we can't violate
// CSP in tests or Js Errors will cause test failures.
// TODO(crbug/1148090): PDF loading tests should also appear here, they are
// currently in media_app_integration_browsertest.cc due to 'wasm-eval' JS
// errors.
GUEST_TEST('GuestCanLoadWithCspRestrictions', async () => {
  // Can load images served from chrome-untrusted://media-app/.
  const image = new Image();
  image.src = 'chrome-untrusted://media-app/does-not-exist.png';
  await eventToPromise('error', image);

  // Can load image data urls.
  const imageData = new Image();
  imageData.src = 'data:image/png;base64,iVBORw0KG';
  await eventToPromise('error', imageData);

  // Can load image blobs.
  const imageBlob = new Image();
  imageBlob.src = 'blob:chrome-untrusted://media-app/my-fake-blob-hash';
  await eventToPromise('error', imageBlob);

  // Can load video blobs.
  const videoBlob =
      /** @type {!HTMLVideoElement} */ (document.createElement('video'));
  videoBlob.src = 'blob:chrome-untrusted://media-app/my-fake-blob-hash';
  await eventToPromise('error', videoBlob);
});

GUEST_TEST('GuestStartsWithDefaultFileList', async () => {
  chai.assert.isDefined(window.customLaunchData);
  chai.assert.isDefined(window.customLaunchData.files);
  chai.assert.isTrue(window.customLaunchData.files.length === 0);
});
