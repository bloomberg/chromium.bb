// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.onconnect = async e => {
  const params = new URLSearchParams(location.search);
  const response = await fetch(params.get('url'));
  if (!response.ok) {
    e.ports[0].postMessage(`Bad response: ${responses.statusText}`);
    return;
  }
  const text = await response.text();
  e.ports[0].postMessage(text);
};
