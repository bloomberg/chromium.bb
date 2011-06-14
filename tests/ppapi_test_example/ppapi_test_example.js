// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupTests(tester, plugin) {
  // TODO(tester): replace with your tests.

  // Below are the before & after versions of sample test cases demonstrating
  // how to transition from synchronous scripting to postMessage as the test
  // driving mechanism.

  // Before.
  tester.addTest('Example::SimpleSync', function() {
    assert(plugin.TestSimpleSync());
  });

  // After.
  tester.addAsyncTest('Example::SimpleAsync', function(status) {
    var messageListener = status.wrap(function(message) {
      status.log('Received message: ' + message.data);
      plugin.removeEventListener('message', messageListener, false);
      status.assertEqual(message.data, 'TestSimpleAsync:PASSED');
      status.pass();
    });

    plugin.addEventListener("message", messageListener, false);
    plugin.postMessage("TestSimpleAsync");
  });

  // Before.
  tester.addTest('Example::CallbackSync', function(status) {
    status.assert(plugin.TestCallbackSync());
    status.waitForCallback('CallbackSync', 1);
  });

  // After.
  tester.addAsyncTest('Example::CallbackAsync', function(status) {
    var gotPassed = false;
    var messageListener = status.wrap(function(message) {
      // <IGNORE>
      // If you are consulting this example while porting or writing
      // addAsyncTest's, ignore this part. This is a short-term side-effect of
      // C++ test harness migration and affects only this example
      // that uses both old-style and new-style tests.
      // To simplify C++ test harness migration, the callback handler
      // notifies JS both via browser scripting and postMessage,
      // so we should ignore the message from TestCallbackSync above.
      if (message.data == 'CallbackSync')
        return;
      // </IGNORE>
      status.log('Received message: ' + message.data);
      plugin.removeEventListener('message', messageListener, false);
      if (!gotPassed) {
        status.assertEqual(message.data, 'TestCallbackAsync:PASSED');
        gotPassed = true;
        plugin.addEventListener("message", messageListener, false);
      } else {
        status.assertEqual(message.data, 'CallbackAsync');
        status.pass();
      }
    });

    plugin.addEventListener("message", messageListener, false);
    plugin.postMessage("TestCallbackAsync");
  });
}
