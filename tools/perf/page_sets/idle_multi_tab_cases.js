// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

__telemetry_spawned_tabs = [];

function __telemetry_spawn_tab() {
  var tab = window.open(window.location.href);
  if (typeof tab === "undefined") {
    throw "Cannot open tab. Forgot --disable-popup-blocking flag?";
  }
  __telemetry_spawned_tabs.push(tab);
  return __telemetry_spawned_tabs.length - 1;
}

function __telemetry_close_tab(tab) {
  __telemetry_spawned_tabs[tab].close();
  __telemetry_spawned_tabs[tab] = null;
}
