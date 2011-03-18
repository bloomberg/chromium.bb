// Copyright 2011 The Native Client SDK Authors.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

/**
 * @file
 * Implement the RPC bridge from the test page to the Chrome extension.  These
 * functions are run inside the extension's process and can communicate with
 * the DOM of both the test page and the extension's background page.  The
 * NaCl module is loaded into the extension's background page.  For more
 * information, see:
 *     http://code.google.com/chrome/extensions/content_scripts.html
 */

/**
 * Process the test results by changing the innerHTML of the DOM element with
 * id "nacl_test_module".  If that DOM does not exist, do nothing.
 * @param {!Object} testResults A dictionary mapping test names with result
 *     values.  This dictionary is filled in by the extension.
 */
function dispatchTestResults(testResults) {
  var naclTestModule = document.getElementById('test_results_element');
  if (naclTestModule) {
    naclTestModule.onclick(testResults);
  }
}

/**
 * Run the unit tests.  This sends an RPC request to the extension.  The
 * extension, in turn, runs the tests and sends the results back to the
 * embedding page via a DOM event.
 */
function runAllNaClTests() {
  var msg = {type: 'runTests'};
  chrome.extension.sendRequest(msg,
      function(response) { dispatchTestResults(response); }
  );
}

// Run the unit tests once this script has actually been injected.
runAllNaClTests();
