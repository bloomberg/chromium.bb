// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupTests(tester, plugin) {
  //////////////////////////////////////////////////////////////////////////////
  // Test Helpers
  //////////////////////////////////////////////////////////////////////////////

  // These are the coordinates of the upper-left corner of the nacl plugin
  // TODO(mball): Avoid hardcoding this location by querying the location
  // of the plugin.
  var leftCorner = 8, topCorner = 79

  function simulateMouseEvent(eventName) {
    var event = document.createEvent('MouseEvents');
    event.initMouseEvent(
        eventName,
        true,                      // bubbles
        true,                       // cancelable
        document.defaultView,
        1,                          // one click
        0, 0,                       // Screen coordinate
        leftCorner, topCorner,      // Client coordinate
        false, false, false, false, // no key modifiers
        0,                          // button: 0 - left, 1 - middle, 2 - right
        null);                      // no related targets
    plugin.dispatchEvent(event);
  }

  function addTest(testName, testCaseEvent, eventResponse,
                   eventGeneratorCallback) {
    tester.addAsyncTest('PPP_InputEvent::' + testName + '_' + testCaseEvent,
                        function(test) {
      var messages = [testName + ':PASSED',
                      'HandleInputEvent:' + eventResponse,
                      'Cleanup:PASSED'];
      // TODO(mball,polina): These listener functions should fail when an event
      // is not bubbled up from the Nacl module.
      var pluginListener = test.wrap(function(message) {
        test.log('plugin received ' + message);
      });
      var parentListener = test.wrap(function(message) {
        test.log('plugin.parentNode received ' + message);
      });
      test.expectMessageSequence(
        plugin, messages,
        function(message) {
          if (message == messages[0]) {
            // TODO(polina): why does this forward and bubble the event even
            // though it was handled? Based on headers, the listeners below
            // should contain test.fail(). Figure out if this is a bug
            // (crbug.com/88728) or if the header comments are off.
            plugin.addEventListener(testCaseEvent, pluginListener, false);
            plugin.parentNode.addEventListener(
              testCaseEvent, parentListener, false);
            eventGeneratorCallback(testCaseEvent);
          } else if (message == messages[1]) {
            plugin.removeEventListener(testCaseEvent, pluginListener, false);
            plugin.parentNode.removeEventListener(
              testCaseEvent, parentListener, false);
            plugin.postMessage('Cleanup');
          } else if (message != messages[2]) {
            test.fail('Unknown message: ' + message);
          }
        });
      plugin.postMessage(testName);
    });
  }

  //////////////////////////////////////////////////////////////////////////////
  // Tests
  //////////////////////////////////////////////////////////////////////////////
  var mouseEventTests = [
    'TestUnfilteredMouseEvents',
    'TestBubbledMouseEvents',
    'TestUnbubbledMouseEvents'];

  mouseEventTests.map(function(testName) {
    addTest(
      testName,
      'mousedown',
      'PP_INPUTEVENT_TYPE_MOUSEDOWN' +
          ':PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN' +
          ':PP_INPUTEVENT_MOUSEBUTTON_LEFT' +
          ':Position=0,0:ClickCount=1',
      simulateMouseEvent);
  });
}
