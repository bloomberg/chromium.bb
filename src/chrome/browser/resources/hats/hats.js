// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hats', function() {
  'use strict';

  // Install the survey script from HaTS.
  function setScript() {
    const webview = $('survey');
    const loadstop = function() {
      webview.executeScript({
        code: 'document.getElementById(\'contain-402\').style.position =' +
            ' \'static\''
      });
      chrome.send('afterShow');
    };
    webview.addEventListener('loadstop', loadstop);
  }
  // Return an object with all of the exports.
  return {
    setScript: setScript,
  };
});


document.addEventListener('DOMContentLoaded', hats.setScript);
