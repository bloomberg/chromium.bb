// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const params = new URLSearchParams(location.search);

async function run() {
  const response = await fetch(params.get('url'));
  if (!response.ok) {
    self.postMessage('bad response');
    return;
  }
  const text = await response.text();
  self.postMessage(text);
};

run();
