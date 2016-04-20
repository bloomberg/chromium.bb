// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  "use strict";

  var START_LOADING_DELAY = 1000;

  function doLazyLoad() {
    function lazyLoadUrl(url) {
      var link = document.createElement('link');
      link.rel = 'import';
      link.href = url;
      document.body.appendChild(link);
    }

    lazyLoadUrl('chrome://resources/polymer/v1_0/iron-icons/iron-icons.html');
    lazyLoadUrl(
        'chrome://resources/polymer/v1_0/paper-button/paper-button.html');
  }

  window.addEventListener('load', function() {
    setTimeout(doLazyLoad, START_LOADING_DELAY);
  });
})();
