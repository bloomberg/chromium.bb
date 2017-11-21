// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that geolocation emulation with latitude and longitude works as expected.\n`);

  TestRunner.printDevToolsConsole();
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.addScriptTag('../resources/permissions-helper.js');
  await TestRunner.evaluateInPagePromise(`
      function grantGeolocationPermission() {
        PermissionsHelper.setPermission('geolocation', 'granted').then(function(p) {
          console.log("Permission granted.");
        });
      }

      function serializeGeolocationError(error) {
          var result = "Unknown error"
          switch (error.code)
          {
              case error.PERMISSION_DENIED:
                  result = "Permission denied";
                  break;
              case error.POSITION_UNAVAILABLE:
                  result = "Position unavailable";
                  break;
              case error.TIMEOUT:
                  result = "Request timed out";
                  break;
          }
          if (error.message)
              result += " (" + error.message + ")";
          return result;
      }

      function overrideGeolocation()
      {
          function testSuccess(position)
          {
              if (position && position.coords)
                  console.log("Latitude: " + position.coords.latitude + " Longitude: " + position.coords.longitude);
              else
                  console.log("Unexpected error occured. Test failed.");
          }

          function testFailed(error)
          {
              console.log(serializeGeolocationError(error));
          }

          navigator.geolocation.getCurrentPosition(testSuccess, testFailed);
      }

      function overridenTimestampGeolocation()
      {
          function testSuccess(position)
          {
              if ((new Date(position.timestamp)).toDateString() == (new Date()).toDateString())
                  console.log("PASSED");
              else
                  console.log("Unexpected error occured. Test failed.");
          }

          function testFailed(error)
          {
              console.log(serializeGeolocationError(error));
          }

          navigator.geolocation.getCurrentPosition(testSuccess, testFailed);
      }
  `);

  function consoleSniffAndDump(next) {
    ConsoleTestRunner.addConsoleSniffer(() => {
        ConsoleTestRunner.dumpConsoleMessages();
        Console.ConsoleView.clearConsole();
        next();
    });
  }

  TestRunner.runTestSuite([
    function testPermissionGranted(next) {
      consoleSniffAndDump(next);
      TestRunner.evaluateInPage('grantGeolocationPermission()');
    },

    function testGeolocationUnavailable(next) {
      TestRunner.EmulationAgent.setGeolocationOverride();
      consoleSniffAndDump(next);
      TestRunner.evaluateInPage('overrideGeolocation()');
    },

    function testOverridenGeolocation(next) {
      TestRunner.EmulationAgent.setGeolocationOverride(50, 100, 95);
      consoleSniffAndDump(next);
      TestRunner.evaluateInPage('overrideGeolocation()');
    },

    function testInvalidParam(next) {
      TestRunner.EmulationAgent.setGeolocationOverride(true, 100, 95);
      next();
    },

    function testInvalidGeolocation(next) {
      TestRunner.EmulationAgent.setGeolocationOverride(200, 300, 95);
      consoleSniffAndDump(next);
      TestRunner.evaluateInPage('overrideGeolocation()');
    },

    function testTimestampOfOverridenPosition(next) {
      TestRunner.EmulationAgent.setGeolocationOverride(50, 100, 95);
      consoleSniffAndDump(next);
      TestRunner.evaluateInPage('overridenTimestampGeolocation()');
    },
  ]);
})();
