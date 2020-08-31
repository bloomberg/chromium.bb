// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/**
 * Takes the |moduleListData| input argument which represents data about
 * the currently available modules and populates the html jstemplate
 * with that data. It expects an object structure like the above.
 * @param {Object} moduleListData Information about available modules
 */
function renderTemplate(moduleListData) {
  // Process the template.
  const input = new JsEvalContext(moduleListData);
  const output = $('naclInfoTemplate');
  jstProcess(input, output);
}

/**
 * Asks the C++ NaClUIDOMHandler to get details about the NaCl and
 * re-populates the page with the data.
 */
function requestNaClInfo() {
  sendWithPromise('requestNaClInfo').then((moduleListData) => {
    $('loading-message').hidden = true;
    $('body-container').hidden = false;
    renderTemplate(moduleListData);
  });
}

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', requestNaClInfo);
