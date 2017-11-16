// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests how request headers are decoded in network item view.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.loadHTML(`
      <a href="https://bugs.webkit.org/show_bug.cgi?id=61205">Bug 74422</a>
    `);

  var value = 'Test+%21%40%23%24%25%5E%26*%28%29_%2B+parameters.';
  var parameterElement = Network.RequestHeadersView.prototype._formatParameter(value, '', true);
  TestRunner.addResult('Original value: ' + value);
  TestRunner.addResult('Decoded value: ' + parameterElement.textContent);
  TestRunner.completeTest();
})();
