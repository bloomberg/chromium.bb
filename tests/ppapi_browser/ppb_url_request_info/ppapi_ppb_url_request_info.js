// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupTests(tester, plugin) {
  function addTest(test_name) {
    tester.addAsyncTest('PPB_URLRequestInfo::' + test_name, function(test) {
      var messageListener = test.wrap(function(message) {
        plugin.removeEventListener('message', messageListener, false);
        test.assertEqual(message.data, test_name + ':PASSED');
        test.pass();
      });

      plugin.addEventListener('message', messageListener, false);
      plugin.postMessage(test_name);
    });
  }

  addTest("TestCreate");
  addTest("TestIsURLRequestInfo");
  addTest("TestSetProperty");
  addTest("TestStress");
}
