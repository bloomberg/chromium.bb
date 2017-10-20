// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test device orientation\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.evaluateInPagePromise(`
      var mockAlpha = 1.1;
      var mockBeta = 2.2;
      var mockGamma = 3.3;
      var absolute = true;

      function setup()
      {
          if (window.testRunner)
              testRunner.setMockDeviceOrientation(true, mockAlpha, true, mockBeta, true, mockGamma, absolute);
          window.addEventListener("deviceorientation", handler, false);
      }

      function handler(evt)
      {
          console.log("alpha: " + evt.alpha + " beta: " + evt.beta + " gamma: " + evt.gamma);
      }
  `);

  TestRunner.runTestSuite([
    function setUp(next) {
      TestRunner.evaluateInPage('setup()', next);
    },

    function setOverride(next) {
      ConsoleTestRunner.addConsoleSniffer(next);
      TestRunner.DeviceOrientationAgent.setDeviceOrientationOverride(20, 30, 40);
    },

    async function clearOverride(next) {
      await TestRunner.DeviceOrientationAgent.clearDeviceOrientationOverride();
      ConsoleTestRunner.dumpConsoleMessages();
      next();
    },
  ]);
})();
