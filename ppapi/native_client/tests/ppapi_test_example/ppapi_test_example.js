// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupTests(tester, plugin) {
  // TODO(tester): replace with your tests.

  // Below are sample test cases demonstrating postMessage as the test
  // driving mechanism.

  tester.addAsyncTest('Example::Simple', function(status) {
    var messageListener = status.wrap(function(message) {
      status.log('Received message: ' + message.data);
      plugin.removeEventListener('message', messageListener, false);
      status.assertEqual(message.data, 'TestSimple:PASSED');
      status.pass();
    });

    plugin.addEventListener("message", messageListener, false);
    plugin.postMessage("TestSimple");
  });

  tester.addAsyncTest('Example::Callback', function(status) {
    var gotPassed = false;
    var messageListener = status.wrap(function(message) {
      status.log('Received message: ' + message.data);
      plugin.removeEventListener('message', messageListener, false);
      if (!gotPassed) {
        status.assertEqual(message.data, 'TestCallback:PASSED');
        gotPassed = true;
        plugin.addEventListener("message", messageListener, false);
      } else {
        status.assertEqual(message.data, 'Callback');
        status.pass();
      }
    });

    plugin.addEventListener("message", messageListener, false);
    plugin.postMessage("TestCallback");
  });
}
