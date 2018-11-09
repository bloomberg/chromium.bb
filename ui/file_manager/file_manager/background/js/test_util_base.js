// Copyright (c) 2015 The Chromium Authors. All rights reserved.
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
 * Loaded at runtime and invoked by the external message listener to handle
 * remote call requests.
 * @type{?function(*, function(*): void)}
 */
test.util.executeTestMessage = null;

/**
 * Registers message listener, which runs test utility functions.
 */
test.util.registerRemoteTestUtils = function() {
  /**
   * Responses that couldn't be sent while waiting for test scripts to load.
   * Null if there is no load in progress.
   * @type{Array<function(*)>}
   */
  let responsesWaitingForLoad = null;

  // Return true for asynchronous functions and false for synchronous.
  chrome.runtime.onMessageExternal.addListener(function(
      request, sender, sendResponse) {
    /**
     * List of extension ID of the testing extension.
     * @type {Array<string>}
     * @const
     */
    const kTestingExtensionIds = [
      'oobinhbdbiehknkpbpejbbpdbkdjmoco',  // File Manager test extension.
      'ejhcmmdhhpdhhgmifplfmjobgegbibkn',  // Gallery test extension.
      'ljoplibgfehghmibaoaepfagnmbbfiga',  // Video Player test extension.
      'ddabbgbggambiildohfagdkliahiecfl',  // Audio Player test extension.
    ];

    // Check the sender.
    if (!sender.id || kTestingExtensionIds.indexOf(sender.id) === -1) {
      // Silently return.  Don't return false; that short-circuits the
      // propagation of messages, and there are now other listeners that want to
      // handle external messages.
      return;
    }
    if (window.IN_TEST) {
      // If there are multiple foreground windows, the remote call API may have
      // already been initialised by one of them, so just return true to tell
      // the other window we are ready.
      if ('enableTesting' in request) {
        sendResponse(true);
        return false;  // No need to keep the connection alive.
      }
      return test.util.executeTestMessage(request, sendResponse);
    }

    // When a valid test extension connects, the first message sent must be an
    // enable tests request.
    if (!('enableTesting' in request))
      throw new Error('Expected enableTesting');

    if (responsesWaitingForLoad != null) {
      // Loading started, but not complete. Queue the response.
      responsesWaitingForLoad.push(sendResponse);
      return true;
    }
    responsesWaitingForLoad = [];

    let script = document.createElement('script');
    document.body.appendChild(script);
    script.onload = function() {
      // The runtime load should have populated test.util with
      // executeTestMessage, allowing it to be invoked on the next
      // onMessageExternal call.
      sendResponse(true);
      responsesWaitingForLoad.forEach((queuedResponse) => {
        queuedResponse(true);
      });
      responsesWaitingForLoad = null;

      // Set a global flag that we are in tests, so other components are aware
      // of it.
      window.IN_TEST = true;
    };
    script.onerror = function(/** Event */ event) {
      console.error('Script load failed ' + event);
      throw new Error('Script load failed.');
    };
    const kFileManagerExtension =
        'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj';
    const kTestScriptUrl =
        kFileManagerExtension + '/background/js/runtime_loaded_test_util.js';
    console.log('Loading ' + kTestScriptUrl);
    script.src = kTestScriptUrl;
    return true;
  });
};
