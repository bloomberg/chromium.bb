// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that Elements panel allows to change src attribute on iframes inside inspected page. See bug 41350.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('console_test_runner');

  var messagePromise = new Promise(x => ConsoleTestRunner.addConsoleSniffer(x));
  await TestRunner.loadHTML(`
      <iframe src="resources/iframe-from-different-domain-data.html" id="receiver"></iframe>
    `);
  var message = await messagePromise;
  TestRunner.addResult(message.messageText);

  var node = await new Promise(x => ElementsTestRunner.nodeWithId('receiver', x));
  var messagePromise = new Promise(x => ConsoleTestRunner.addConsoleSniffer(x));
  node.setAttribute('src', 'src="http://localhost:8000/devtools/resources/iframe-from-different-domain-data.html"');
  var message = await messagePromise;
  TestRunner.addResult(message.messageText);
  TestRunner.completeTest();
})();
