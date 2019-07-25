// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function postMessageToWindow(msg) {
  for (const client of await clients.matchAll({includeUncontrolled: true}))
    client.postMessage(msg);
}

self.addEventListener('contentdelete', event => {
  event.waitUntil(postMessageToWindow(event.id));
});
