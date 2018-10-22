// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function MessageHandler(e) {
  const response = await fetch(e.data.url);
  if (!response.ok) {
    e.ports[0].postMessage('bad response');
    return;
  }
  const text = await response.text();
  e.ports[0].postMessage(text);
}

self.addEventListener('message', e => {
  e.waitUntil(MessageHandler(e));
});
