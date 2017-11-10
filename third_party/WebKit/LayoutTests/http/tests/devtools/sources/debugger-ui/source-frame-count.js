// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that scripts panel does not create too many source frames.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('resources/source-frame-count.html');

  var framesOpened = 0;

  SourcesTestRunner.runDebuggerTestSuite([function testSourceFramesCount(next) {
    var panel = UI.panels.sources;

    SourcesTestRunner.showScriptSource(
        'source-frame-count.html', function() {});
    SourcesTestRunner.showScriptSource('script1.js', function() {});
    SourcesTestRunner.showScriptSource('script2.js', function() {});
    SourcesTestRunner.showScriptSource('script3.js', function() {});
    SourcesTestRunner.showScriptSource('script4.js', function() {});
    SourcesTestRunner.showScriptSource('script5.js', reloadThePage);

    function reloadThePage() {
      TestRunner.addResult('Reloading page...');
      TestRunner.reloadPage(didReload);
      function didCreateSourceFrame() {
        framesOpened++;
      }
      TestRunner.addSniffer(
          SourceFrame.SourceFrame.prototype, 'wasShown', didCreateSourceFrame,
          true);
    }

    function didReload() {
      if (framesOpened > 3)
        TestRunner.addResult('Too many frames opened: ' + framesOpened);
      else
        TestRunner.addResult('Less than 3 frames opened');
      TestRunner.addResult(
          'Visible view: ' + panel.visibleView._uiSourceCode.name());
      next();
    }
  }]);
})();
