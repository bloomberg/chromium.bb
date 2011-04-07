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
 * Run the unit tests.  This opens a port to the extension.  The
 * extension, in turn, runs the tests and sends the results back to the
 * embedding page via a DOM event.
 */
function runAllNaClTests() {
  var naclTestModule = document.getElementById('test_results_element');
  if(naclTestModule) {
    naclTestModule.onclick.log('Bridge script injected.');
  }
  // Opening the channel will kick off testing.
  port = chrome.extension.connect({name: 'testChannel'});
  port.onMessage.addListener(function(e) {
    var naclTestModule = document.getElementById('test_results_element');
    if(!naclTestModule) return;
    if(e.type == 'processTestResults') {
      // Run the tests
      naclTestModule.onclick.run(e.testResults);
    } else if(e.type == 'log') {
      // A generic log message
      naclTestModule.onclick.log(e.message);
    }
  });
  // This might happen if the extension isn't listening yet...
  port.onDisconnect.addListener(function(e) {
    var naclTestModule = document.getElementById('test_results_element');
    naclTestModule.onclick.log('The port is disconnected!');
  });
}

runAllNaClTests();
