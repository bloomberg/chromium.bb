// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for test related things.
 */
var test = test || {};

/**
 * Namespace for test utility functions.
 *
 * Public functions in the test.util.sync and the test.util.async namespaces are
 * published to test cases and can be called by using callRemoteTestUtil. The
 * arguments are serialized as JSON internally. If application ID is passed to
 * callRemoteTestUtil, the content window of the application is added as the
 * first argument. The functions in the test.util.async namespace are passed the
 * callback function as the last argument.
 */
test.util = {};

/**
 * Namespace for synchronous utility functions.
 */
test.util.sync = {};

/**
 * Namespace for asynchronous utility functions.
 */
test.util.async = {};

/**
 * Extension ID of the testing extension.
 * @type {string}
 * @const
 */
test.util.TESTING_EXTENSION_ID = 'oobinhbdbiehknkpbpejbbpdbkdjmoco';

/**
 * Opens the main Files.app's window and waits until it is ready.
 *
 * @param {Window} contentWindow Video player window to be chacked toOB.
 * @return {boolean} True if the video is playing, false otherwise.
 */
test.util.sync.isPlaying = function(contentWindow) {
  var selector = 'video[src]';
  var element = contentWindow.document.querySelector(selector);
  if (!element)
    return false;

  return !element.paused;
};

/**
 * Gets total Javascript error count from each app window.
 * @return {number} Error count.
 */
test.util.sync.getErrorCount = function() {
  var totalCount = JSErrorCount;
  for (var appId in appWindowsForTest) {
    var contentWindow = appWindowsForTest[appId].contentWindow;
    if (contentWindow.JSErrorCount)
      totalCount += contentWindow.JSErrorCount;
  }

  // Errors in the background page.
  totalCount += window.JSErrorCount;

  return totalCount;
};

/**
 * Registers message listener, which runs test utility functions.
 */
test.util.registerRemoteTestUtils = function() {
  // Register the message listener.
  var onMessage = chrome.runtime ? chrome.runtime.onMessageExternal :
      chrome.extension.onMessageExternal;
  // Return true for asynchronous functions and false for synchronous.
  onMessage.addListener(function(request, sender, sendResponse) {
    // Check the sender.
    if (sender.id != test.util.TESTING_EXTENSION_ID) {
      console.error('The testing extension must be white-listed.');
      sendResponse(false);
      return false;
    }
    if (!request.func || request.func[request.func.length - 1] == '_') {
      request.func = '';
    }
    // Prepare arguments.
    var args = request.args.slice();  // shallow copy
    if (request.file) {
      if (!appWindowsForTest[request.file]) {
        console.error('Specified window not found: ' + request.file);
        sendResponse(false);
        return false;
      }
      args.unshift(appWindowsForTest[request.file].contentWindow);
    }
    // Call the test utility function and respond the result.
    if (test.util.async[request.func]) {
      args[test.util.async[request.func].length - 1] = function() {
        console.debug('Received the result of ' + request.func);
        sendResponse.apply(null, arguments);
      };
      console.debug('Waiting for the result of ' + request.func);
      test.util.async[request.func].apply(null, args);
      return true;
    } else if (test.util.sync[request.func]) {
      sendResponse(test.util.sync[request.func].apply(null, args));
      return false;
    } else {
      console.error('Invalid function name.');
      sendResponse(false);
      return false;
    }
  }.wrap());
};

// Register the test utils.
test.util.registerRemoteTestUtils();
