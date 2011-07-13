// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function startsWith(str, prefix) {
  return (str.indexOf(prefix) === 0);
}

function simulateMouseEvent(target, eventName) {
  var event = document.createEvent('MouseEvents');
  event.initMouseEvent(
      eventName,
      true,                       // bubbles
      true,                       // cancelable
      document.defaultView,
      1,                          // one click
      0, 0, 0, 0,                 // no co-ordinates
      false, false, false, false, // no key modifiers
      0,                          // button: 0 - left, 1 - middle, 2 - right
      null);                      // no related targets
  target.dispatchEvent(event);
}

function setupTests(tester, plugin) {
  //////////////////////////////////////////////////////////////////////////////
  // Test Helpers
  //////////////////////////////////////////////////////////////////////////////
  var numMessages = 0;
  function addTestListeners(numListeners, test, testFunction, runCheck) {
    var messageListener = test.wrap(function(message) {
      if (!startsWith(message.data, testFunction)) return;
      test.log(message.data);
      numMessages++;
      plugin.removeEventListener('message', messageListener, false);
      test.assertEqual(message.data, testFunction + ':PASSED');
      if (runCheck) test.assert(runCheck());
      if (numMessages < numListeners) {
        plugin.addEventListener('message', messageListener, false);
      } else {
        numMessages = 0;
        test.pass();
      }
    });
    plugin.addEventListener('message', messageListener, false);
  }

  function addTestListener(test, testFunction, runCheck) {
    return addTestListeners(1, test, testFunction, runCheck);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Tests
  //////////////////////////////////////////////////////////////////////////////

  tester.addTest('PPP_Instance::DidCreate', function() {
    assertEqual(plugin.lastError, '');
  });

  tester.addAsyncTest('PPP_Instance::DidChangeView', function(test) {
    addTestListeners(3, test, 'DidChangeView');
  });

  tester.addAsyncTest('PPP_Instance::DidChangeFocus', function(test) {
    // TODO(polina): How can I simulate focusing on Windows?
    // For now just pass explicitely.
    if (startsWith(navigator.platform, 'Win')) {
      test.log('skipping test on ' + navigator.platform);
      test.pass();
      return;
    }
    addTestListeners(2, test, 'DidChangeFocus');
    plugin.tabIndex = 0;
    plugin.focus();
    plugin.blur();
  });

  tester.addAsyncTest('PPP_Instance::HandleInputEvent_Handled', function(test) {
    addTestListener(test, 'HandleInputEvent');
    // TODO(polina): why does this forward and bubble the event even though it
    // was handled? Based on headers, the handlers below should contain
    // test.fail(). Figure out if this is a bug (crbug.com/88728) or if the
    // header comments are off.
    var event = 'mousedown';
    plugin.addEventListener(
        event,
        function(e) { test.log('plugin received ' + event); },
        false);
    plugin.parentNode.addEventListener(
        event,
        function(e) { test.log('plugin.parentNode received ' + event); },
        false);
    simulateMouseEvent(plugin, event);
  });

  tester.addAsyncTest('PPP_Instance::HandleInputEvent_Bubbled', function(test) {
    var forwarded = false;
    var bubbled = false;
    var event = 'mouseup';
    addTestListener(test, 'HandleInputEvent',
                    function() { return forwarded && bubbled; });
    plugin.addEventListener(event,
                            function(e) { forwarded = true; }, false);
    plugin.parentNode.addEventListener(event,
                                       function(e) { bubbled = true; }, false);
    simulateMouseEvent(plugin, event);
  });

  // TODO(polina/bbudge/abarth):
  // PPP_Instance::HandleDocumentLoad is only used with full-frame plugins.
  // See if we can test this in a content handling extension test:
  // write an extension that contains a simple NEXE and then have an extension
  // API test that shows that, when you have that extension installed, the nexe
  // loads. There might be a way to load an extension in chrome using a
  // command-line parameter if you have an instance of chrome in your testing
  // harness.

  tester.addAsyncTest('PPP_Instance::DidDestroy', function(test) {
    // Destroy the plugin triggering PPP_Instance::DidDestroy.
    // A message should be logged to stdout.
    plugin.parentNode.removeChild(plugin);
    // Unfortunately, PPB_Messaging is a per-instance interface, so
    // posting messages from PPP_Instance::DidDestory does not work.
    // TODO(polina): Is there a different way to detect this from JavaScript?
    // For now just pass explicitely.
    test.pass();
  });
}
