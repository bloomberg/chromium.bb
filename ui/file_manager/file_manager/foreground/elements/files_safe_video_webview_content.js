// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  var FILES_APP_ORIGIN = 'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj';
  var messageSource;

  var content = document.getElementById('content');

  window.addEventListener('message', function(event) {
    // TODO(oka): Fix FOUC problem. When webview is first created and receives
    // the first message, sometimes the body size is smaller than the outer
    // window for a moment and it causes flush of unstyled content.
    if (event.origin !== FILES_APP_ORIGIN) {
      console.error('Unknown origin: ' + event.origin);
      return;
    }
    messageSource = event.source;
    content.src = event.data;
  });

  document.addEventListener('contextmenu', function(e) {
    e.preventDefault();
    return false;
  });

  document.addEventListener('click', function(e) {
    var data;
    if (e.target === content) {
      data = 'tap-inside';
    } else {
      data = 'tap-outside';
    }

    if (messageSource)
      messageSource.postMessage(data, FILES_APP_ORIGIN);
  });
};
