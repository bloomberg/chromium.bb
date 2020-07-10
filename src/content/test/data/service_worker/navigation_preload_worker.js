// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('activate', event => {
  event.waitUntil(self.registration.navigationPreload.enable());
});

self.addEventListener('fetch', event => {
  if (event.request.mode == 'navigate')
    event.respondWith(event.preloadResponse);
});
