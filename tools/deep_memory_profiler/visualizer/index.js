// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

$(function() {
  // Read original data and plot.
  $.getJSON('data/result.json', function(jsonData) {
    // Create model.
    var profiler = new Profiler(jsonData);
    // Create views subscribing model events.
    var graphView = new GraphView(profiler);
    var menuView = new MenuView(profiler);

    // initialize categories according to roots information.
    profiler.initialize();
  });
});
