// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  "use strict";

  var RESOURCES_TO_LOAD = [
    'chrome://resources/polymer/v1_0/iron-icons/iron-icons.html',
    'chrome://resources/polymer/v1_0/paper-button/paper-button.html'
  ];
  /* const */ var IDLE_TIMEOUT_MS = 200;

  window.addEventListener('load', function() {
    // The user pod template gets cloned shortly after the load event to make
    // the actual user pods. It then takes a few update cycles to style these
    // elements. Loading polymer will block the DOM, so we try to load polymer
    // after the user pods have been cloned and styled.
    requestIdleCallback(function() {
      for (var resourceUrl of RESOURCES_TO_LOAD) {
        var link = document.createElement('link');
        link.rel = 'import';
        link.href = resourceUrl;
        document.head.appendChild(link);
      }
    }, { timeout: IDLE_TIMEOUT_MS });
  });
})();
