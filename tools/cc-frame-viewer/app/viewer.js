/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

base.exportTo('ccfv', function() {
  function assert(bool) {
    if (bool)
      return;
    throw new Error("Expected true");
  }

  chromeapp.addEventListener('launch', init);
  chrome.app.window.current().onClosed.addListener(onClosed);
  chrome.app.window.current().focus();

  var viewerEl;
  function init(launch_event) {
    viewerEl = document.querySelector('#viewer')
    window.g_viewerEl = viewerEl;

    var num_args = launch_event.args[0]
    chromeapp.sendEvent('load', 'please', onLoadResult, onLoadError);
  }

  function onLoadResult(res) {
    var trace = JSON.parse(res);

    window.g_lastResult = trace;
    var model = new ccfv.Model();
    try {
        model.initFromTraceEvents(trace);
    } catch(e) {
        onLoadError(e);
        return;
    }
    window.g_model = model;

    var modelViewEl = new ccfv.ModelView();
    modelViewEl.model = model;
    window.g_modelViewEl = modelViewEl;

    viewerEl.appendChild(modelViewEl);
  }

  function onLoadError(err) {
    viewerEl.textContent = 'error loading: ' + err;
  }

  function onClosed() {
    chromeapp.exit(1);
  }

});
