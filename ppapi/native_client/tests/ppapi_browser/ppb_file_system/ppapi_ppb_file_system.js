// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupTests(tester, plugin) {
  tester.addAsyncTest('TestCreate', function(status) {
    var messageListener = status.wrap(function(message) {
      status.log('Received message: ' + message.data);
      plugin.removeEventListener('message', messageListener, false);
      status.assertEqual(message.data, 'TestCreate:PASSED');
      status.pass();
    });
    plugin.addEventListener("message", messageListener, false);
    plugin.postMessage("TestCreate");
  });
  tester.addAsyncTest('TestIsFileSystem', function(status) {
    var messageListener = status.wrap(function(message) {
      status.log('Received message: ' + message.data);
      plugin.removeEventListener('message', messageListener, false);
      status.assertEqual(message.data, 'TestIsFileSystem:PASSED');
      status.pass();
    });
    plugin.addEventListener("message", messageListener, false);
    plugin.postMessage("TestIsFileSystem");
  });
  tester.addAsyncTest('TestGetType', function(status) {
    var messageListener = status.wrap(function(message) {
      status.log('Received message: ' + message.data);
      plugin.removeEventListener('message', messageListener, false);
      status.assertEqual(message.data, 'TestGetType:PASSED');
      status.pass();
    });
    plugin.addEventListener("message", messageListener, false);
    plugin.postMessage("TestGetType");
  });
  tester.addAsyncTest('TestOpen', function(status) {
    var gotPassed = false;
    var messageListener = status.wrap(function(message) {
      status.log('Received message: ' + message.data);
      plugin.removeEventListener('message', messageListener, false);
      if (!gotPassed) {
        status.assertEqual(message.data, 'TestOpen:PASSED');
        gotPassed = true;
        plugin.addEventListener("message", messageListener, false);
      } else {
        status.assertEqual(message.data, 'OpenCallback');
        status.pass();
      }
    });
    plugin.addEventListener("message", messageListener, false);
    plugin.postMessage("TestOpen");
  });
}
