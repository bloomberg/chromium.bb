// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.setupStartupTest('resources/console-log-before-frame-navigation.html');
  TestRunner.addResult(
      `Tests that Web Inspector won't crash if there are messages written to console from a frame which has already navigated to a page from a different domain.\n`);
  await TestRunner.loadModule('console_test_runner');
  var waitFor = 7 - ConsoleModel.consoleModel.messages().length;
  if (waitFor > 0)
    await ConsoleTestRunner.waitUntilNthMessageReceivedPromise(waitFor);
  var messages = ConsoleModel.consoleModel.messages();
  TestRunner.addResult('Received console messages:');
  for (var i = 0; i < messages.length; ++i) {
    var m = messages[i];
    TestRunner.addResult('Message: ' + Bindings.displayNameForURL(m.url) + ':' + m.line + ' ' + m.messageText);
  }
  TestRunner.addResult('TEST COMPLETE.');
  TestRunner.completeTest();
})();
