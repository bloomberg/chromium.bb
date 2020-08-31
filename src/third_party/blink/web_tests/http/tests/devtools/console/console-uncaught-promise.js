// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that uncaught promise rejections are logged into console.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      var tested = 0;
      function runNextPromiseTest()
      {
          ++tested;
          var name = "promiseTest" + tested;
          if (typeof window[name] !== "function")
              return false;
          // setTimeout to cut off VM call frames from the stack trace.
          setTimeout(function timeout() {
              window[name].call(window);
          }, 0);
          return true;
      }

      function promiseTest1()
      {
          Promise.reject(new Error("err1"))
              .then()
              .then()
              .then(); // Last is unhandled.
      }

      function promiseTest2()
      {
          var reject;
          var m0 = new Promise(function(res, rej) { reject = rej; });
          var m1 = m0.then(function() {});
          var m2 = m0.then(function() {});
          var m3 = m0.then(function() {});
          var m4 = 0;
          m0.catch(function() {
              m2.catch(function() {
                  m1.catch(function() {
                      m4 = m3.then(function() {}); // Unhandled.
                  });
              });
          });
          reject(new Error("err2"));
      }

      function promiseTest3()
      {
          var reject;
          var p = new Promise(function(res, rej) {
              reject = rej;
          });
          p.then().catch(function catcher() {
              throwDOMException();
          });
          reject(new Error("FAIL: Should not be printed to console"));

          function throwDOMException()
          {
              var a = document.createElement("div");
              var b = document.createElement("div");
              a.removeChild(b);
          }
      }

      function promiseTest4()
      {
          Promise.reject(42);
      }

      function promiseTest5()
      {
          Promise.reject(1e+100);
      }

      function promiseTest6()
      {
          Promise.reject("foo");
      }

      function promiseTest7()
      {
          Promise.reject({ foo: 42 });
      }

      function promiseTest8()
      {
          Promise.reject();
      }

      function promiseTest9()
      {
          navigator.serviceWorker.register('404');
      }
  `);

  while (await TestRunner.evaluateInPagePromise('runNextPromiseTest()')) {
      // Run all the test cases until there are no more.
  }

  ConsoleTestRunner.expandConsoleMessages(async () => {
      await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
      TestRunner.completeTest();
  });
})();
