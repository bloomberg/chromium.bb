// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

function perfTestCSSValue(options) {
  var property = options.property;
  var from = options.from;
  var to = options.to;
  console.assert(property);
  console.assert(from);
  console.assert(to);
  console.assert(PerfTestHelper);
  console.assert(document.getElementById('container'));

  var duration = 15000;

  var N = PerfTestHelper.getN(1000);
  var targets = [];
  for (var i = 0; i < N; i++) {
    var target = document.createElement('target');
    targets.push(target);
    container.appendChild(target);
  }

  var api = PerfTestHelper.getParameter('api');
  switch (api) {
  case 'css_animations':
    var style = document.createElement('style');
    style.textContent = '' +
      '@-webkit-keyframes anim {\n' +
      '  from {' + property + ': ' + from + ';}\n' +
      '  to {' + property + ': ' + to + ';}\n' +
      '}\n' +
      'target {\n' +
      '  -webkit-animation: anim ' + duration + 'ms linear infinite;\n' +
      '}\n';
    document.head.appendChild(style);
    break;
  case 'web_animations':
    var keyframes = [{}, {}];
    keyframes[0][property] = from;
    keyframes[1][property] = to;
    targets.forEach(function(target) {
      target.animate(keyframes, {
        duration: duration,
        iterations: Infinity,
      });
    });
    break;
  default:
    throw 'Invalid api: ' + api;
  }

  PerfTestHelper.signalReady();
}
