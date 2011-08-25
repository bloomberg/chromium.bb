// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupTests(tester, plugin) {
  var kTests = ['TestStreamToFileSimple',
                'TestProgressSimple',
                'TestOpenSimple',
                'TestResponseInfoSimple',
                'TestCloseSimple',
                'TestReadSimple',

                'TestOpenReadMismatch',
                'TestAbort',
                'TestDoubleOpen',
                'TestOpenSuccess',
                'TestOpenRedirect',
                'TestOpenFailure',
                'TestStreamToFile',
  ];


  for (var i = 0; i < kTests.length; i++) {
    var name = kTests[i];

    // NOTE: closure semantics in JS will always use the
    // last value of "name".
    var f = function(n) {
      return function(test) {
        test.expectMessageSequence(plugin, [n +':PASSED']);
        plugin.postMessage(n);
      }} (name);

    tester.addAsyncTest(name, f);
  }
}
