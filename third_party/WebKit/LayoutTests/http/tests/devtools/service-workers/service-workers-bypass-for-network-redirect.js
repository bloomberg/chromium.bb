// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests "Bypass for network" checkbox with redirection doesn't cause crash.\n`);
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('resources');
  await TestRunner.loadHTML(`
      <p>Tests &quot;Bypass for network&quot; checkbox with redirection doesn't cause crash.</p>
    `);

  const url = 'http://localhost:8000/devtools/service-workers/resources/' +
      'bypass-for-network-redirect.php';
  const frameId = 'frame_id';

  UI.inspectorView.showPanel('sources')
      .then(function() {
        Common.settings.settingForTest('bypassServiceWorker').set(true);
        let callback;
        const promise = new Promise((fulfill) => callback = fulfill);
        ConsoleTestRunner.addConsoleSniffer(message => {
          if (message.messageText == 'getRegistration finished') {
            callback();
          }
        }, true);
        TestRunner.addIframe(url, {id: frameId});
        return promise;
      })
      .then(function() {
        TestRunner.addResult('Success');
        TestRunner.completeTest();
      })
      .catch(function(exception) {
        TestRunner.addResult('Error');
        TestRunner.addResult(exception);
        TestRunner.completeTest();
      });
})();
