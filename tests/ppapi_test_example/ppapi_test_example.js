// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupTests(tester, plugin) {
  // TODO(tester): replace with your tests.
  tester.addTest('Example::Foo', function() {
    assert(plugin.testFoo());
  });
  tester.addTest('Example::PostMessage', function() {
    // TODO(polina): use postMessage instead when proxy support is available.
    //plugin.postMessage("dummy");
    assert(plugin.testHandleMessage());
    tester.waitForCallback('MessageHandled', 1);
  });
}
