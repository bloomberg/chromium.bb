// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
if (window.parent != window)  // Ignore subframes.
  return;
window.testRunner = {};
window.testRunner.isDone = false;

testRunner.waitUntilDone = function() {};
testRunner.dumpAsText = function() {};
testRunner.notifyDone = function() {
  this.isDone = true;
};

window.GCController = {};

GCController.collect = function() {
  gc();
};
})();
