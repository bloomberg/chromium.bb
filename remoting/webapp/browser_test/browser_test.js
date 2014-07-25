// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {checkTypes}  By default, JSCompile is not run on test files.
 *    However, you can modify |remoting_webapp_files.gypi| locally to include
 *    the test in the package to expedite local development.  This suppress
 *    is here so that JSCompile won't complain.
 *
 * Provides basic functionality for JavaScript based browser test.
 *
 * To define a browser test, create a class under the browserTest namespace.
 * You can pass arbitrary object literals to the browser test from the C++ test
 * harness as the test data.  Each browser test class should implement the run
 * method.
 * For example:
 *
 * browserTest.My_Test = function() {};
 * browserTest.My_Test.prototype.run(myObjectLiteral) = function() { ... };
 *
 * The browser test is async in nature.  It will keep running until
 * browserTest.fail("My error message.") or browserTest.pass() is called.
 *
 * For example:
 *
 * browserTest.My_Test.prototype.run(myObjectLiteral) = function() {
 *   window.setTimeout(function() {
 *     if (doSomething(myObjectLiteral)) {
 *       browserTest.pass();
 *     } else {
 *       browserTest.fail('My error message.');
 *     }
 *   }, 1000);
 * };
 *
 * You will then invoke the test in C++ by calling:
 *
 *   RunJavaScriptTest(web_content, "My_Test", "{"
 *    "pin: '123123'"
 *  "}");
 */

'use strict';

var browserTest = {};

browserTest.init = function() {
  // The domAutomationController is used to communicate progress back to the
  // C++ calling code.  It will only exist if chrome is run with the flag
  // --dom-automation.  It is stubbed out here so that browser test can be run
  // under the regular app.
  browserTest.automationController_ = window.domAutomationController || {
    send: function(json) {
      var result = JSON.parse(json);
      if (result.succeeded) {
        console.log('Test Passed.');
      } else {
        console.error('Test Failed.\n' +
            result.error_message + '\n' + result.stack_trace);
      }
    }
  };
};

browserTest.expect = function(expr, message) {
  if (!expr) {
    message = (message) ? '<' + message + '>' : '';
    browserTest.fail('Expectation failed.' + message);
  }
};

browserTest.fail = function(error) {
  var error_message = error;
  var stack_trace = base.debug.callstack();

  if (error instanceof Error) {
    error_message = error.toString();
    stack_trace = error.stack;
  }

  // To run browserTest locally:
  // 1. Go to |remoting_webapp_files| and look for
  //    |remoting_webapp_js_browser_test_files| and uncomment it
  // 2. gclient runhooks
  // 3. rebuild the webapp
  // 4. Run it in the console browserTest.runTest(browserTest.MyTest, {});
  // 5. The line below will trap the test in the debugger in case of
  //    failure.
  debugger;

  browserTest.automationController_.send(JSON.stringify({
    succeeded: false,
    error_message: error_message,
    stack_trace: stack_trace
  }));
};

browserTest.pass = function() {
  browserTest.automationController_.send(JSON.stringify({
    succeeded: true,
    error_message: '',
    stack_trace: ''
  }));
};

browserTest.clickOnControl = function(id) {
  var element = document.getElementById(id);
  browserTest.expect(element);
  element.click();
};

/** @enum {number} */
browserTest.Timeout = {
  NONE: -1,
  DEFAULT: 5000
};

browserTest.onUIMode = function(expectedMode, opt_timeout) {
  if (expectedMode == remoting.currentMode) {
    // If the current mode is the same as the expected mode, return a fulfilled
    // promise.  For some reason, if we fulfill the promise in the same
    // callstack, V8 will assert at V8RecursionScope.h(66) with
    // ASSERT(!ScriptForbiddenScope::isScriptForbidden()).
    // To avoid the assert, execute the callback in a different callstack.
    return base.Promise.sleep(0);
  }

  return new Promise (function(fulfill, reject) {
    var uiModeChanged = remoting.testEvents.Names.uiModeChanged;
    var timerId = null;

    if (opt_timeout === undefined) {
      opt_timeout = browserTest.Timeout.DEFAULT;
    }

    function onTimeout() {
      remoting.testEvents.removeEventListener(uiModeChanged, onUIModeChanged);
      reject('Timeout waiting for ' + expectedMode);
    }

    function onUIModeChanged(mode) {
      if (mode == expectedMode) {
        remoting.testEvents.removeEventListener(uiModeChanged, onUIModeChanged);
        window.clearTimeout(timerId);
        timerId = null;
        fulfill();
      }
    }

    if (opt_timeout != browserTest.Timeout.NONE) {
      timerId = window.setTimeout(onTimeout, opt_timeout);
    }
    remoting.testEvents.addEventListener(uiModeChanged, onUIModeChanged);
  });
};

browserTest.connectMe2Me = function() {
  var AppMode = remoting.AppMode;
  browserTest.clickOnControl('this-host-connect');
  return browserTest.onUIMode(AppMode.CLIENT_HOST_NEEDS_UPGRADE).then(
    function() {
      // On fulfilled.
      browserTest.clickOnControl('host-needs-update-connect-button');
    }, function() {
      // On time out.
      return Promise.resolve();
    }).then(function() {
      return browserTest.onUIMode(AppMode.CLIENT_PIN_PROMPT);
    });
};

browserTest.expectMe2MeError = function(errorTag) {
  var AppMode = remoting.AppMode;
  var Timeout = browserTest.Timeout;

  var onConnected = browserTest.onUIMode(AppMode.IN_SESSION, Timeout.None);
  var onFailure = browserTest.onUIMode(AppMode.CLIENT_CONNECT_FAILED_ME2ME);

  onConnected = onConnected.then(function() {
    return Promise.reject(
        'Expected the Me2Me connection to fail.');
  });

  onFailure = onFailure.then(function() {
    var errorDiv = document.getElementById('connect-error-message');
    var actual = errorDiv.innerText;
    var expected = l10n.getTranslationOrError(errorTag);

    browserTest.clickOnControl('client-finished-me2me-button');

    if (actual != expected) {
      return Promise.reject('Unexpected failure. actual:' + actual +
                     ' expected:' + expected);
    }
  });

  return Promise.race([onConnected, onFailure]);
};

browserTest.expectMe2MeConnected = function() {
  var AppMode = remoting.AppMode;
  // Timeout if the session is not connected within 30 seconds.
  var SESSION_CONNECTION_TIMEOUT = 30000;
  var onConnected = browserTest.onUIMode(AppMode.IN_SESSION,
                                         SESSION_CONNECTION_TIMEOUT);
  var onFailure = browserTest.onUIMode(AppMode.CLIENT_CONNECT_FAILED_ME2ME,
                                       browserTest.Timeout.NONE);
  onFailure = onFailure.then(function() {
    var errorDiv = document.getElementById('connect-error-message');
    var errorMsg = errorDiv.innerText;
    return Promise.reject('Unexpected error - ' + errorMsg);
  });
  return Promise.race([onConnected, onFailure]);
};

browserTest.runTest = function(testClass, data) {
  try {
    var test = new testClass();
    browserTest.expect(typeof test.run == 'function');
    test.run(data);
  } catch (e) {
    browserTest.fail(e);
  }
};

browserTest.setupPIN = function(newPin) {
  var AppMode = remoting.AppMode;
  var HOST_SETUP_WAIT = 10000;
  var Timeout = browserTest.Timeout;

  return browserTest.onUIMode(AppMode.HOST_SETUP_ASK_PIN).then(function() {
    document.getElementById('daemon-pin-entry').value = newPin;
    document.getElementById('daemon-pin-confirm').value = newPin;
    browserTest.clickOnControl('daemon-pin-ok');

    var success = browserTest.onUIMode(AppMode.HOST_SETUP_DONE, Timeout.NONE);
    var failure = browserTest.onUIMode(AppMode.HOST_SETUP_ERROR, Timeout.NONE);
    failure = failure.then(function(){
      return Promise.reject('Unexpected host setup failure');
    });
    return Promise.race([success, failure]);
  }).then(function() {
    console.log('browserTest: PIN Setup is done.');
    browserTest.clickOnControl('host-config-done-dismiss');

    // On Linux, we restart the host after changing the PIN, need to sleep
    // for ten seconds before the host is ready for connection.
    return base.Promise.sleep(HOST_SETUP_WAIT);
  });
};

browserTest.isLocalHostStarted = function() {
  return new Promise(function(resolve) {
    remoting.hostController.getLocalHostState(function(state) {
      resolve(remoting.HostController.State.STARTED == state);
    });
  });
};

browserTest.ensureHostStartedWithPIN = function(pin) {
  // Return if host is already
  return browserTest.isLocalHostStarted().then(function(started){
    if (!started) {
      console.log('browserTest: Enabling remote connection.');
      browserTest.clickOnControl('start-daemon');
    } else {
      console.log('browserTest: Changing the PIN of the host to: ' + pin + '.');
      browserTest.clickOnControl('change-daemon-pin');
    }
    return browserTest.setupPIN(pin);
  });
};

// Called by Browser Test in C++
browserTest.ensureRemoteConnectionEnabled = function(pin) {
  browserTest.ensureHostStartedWithPIN(pin).then(function(){
    browserTest.automationController_.send(true);
  }).catch(function(errorMessage){
    console.error(errorMessage);
    browserTest.automationController_.send(false);
  });
};

browserTest.init();