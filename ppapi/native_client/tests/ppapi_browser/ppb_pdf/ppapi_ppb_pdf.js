// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupTests(tester, plugin) {
  // This function takes an array of messages and asserts that the nexe
  // calls PostMessage with each of these messages, in order.
  function expectMessages(test, plugin, messages) {
    test.assert(messages.length > 0, 'Must provide at least one message');
    var listener = test.wrap(function(message) {
      plugin.removeEventListener('message', listener, false);
      test.assertEqual(message.data, messages.shift());
      if (messages.length == 0) {
        test.pass();
      } else {
        plugin.addEventListener('message', listener, false);
      }
    });
    plugin.addEventListener('message', listener, false);
  }

  function addTest(test_name, responses) {
    if (responses === undefined) {
      responses = [];
    }
    var expected_messages = [test_name + ':PASSED'];
    expected_messages.concat(responses);
    tester.addAsyncTest('PPB_PDF::' + test_name, function(test) {
      expectMessages(test, plugin, expected_messages);
      plugin.postMessage(test_name)
    });
  }

  addTest('TestGetLocalizedString');
  addTest('TestGetResourceImage');
  addTest('TestGetFontFileWithFallback');
  addTest('TestGetFontTableForPrivateFontFile');
  addTest('TestSearchString');
  addTest('TestDidStartLoading');
  addTest('TestDidStopLoading');
  addTest('TestUserMetricsRecordAction');
  addTest('TestSetContentRestriction');
  addTest('TestHistogramPDFPageCount');
  addTest('TestHasUnsupportedFeature');
  addTest('TestSaveAs');
}
