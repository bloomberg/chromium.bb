// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupTests(tester, plugin) {
  function addTest(test_name) {
    tester.addAsyncTest('PPB_URLRequestInfo::' + test_name, function(test) {
      test.expectMessageSequence(plugin, [test_name + ':PASSED']);
      plugin.postMessage(test_name);
    });
  }

  addTest("TestCreate");
  addTest("TestIsURLRequestInfo");
  addTest("TestSetProperty");
  addTest("TestAppendDataToBody");
  // TODO(elijahtaylor): enable when crbug.com/90878 is fixed.
  //addTest("TestAppendFileToBody");
  addTest("TestStress");
}
