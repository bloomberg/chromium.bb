// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests curl command generation\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  var logView = UI.panels.network._networkLogView;

  function newRequest(headers, data, opt_url) {
    var request = new SDK.NetworkRequest(0, opt_url || 'http://example.org/path', 0, 0, 0);
    request.requestMethod = data ? 'POST' : 'GET';
    var headerList = [];
    if (headers) {
      for (var i in headers)
        headerList.push({name: i, value: headers[i]});
    }
    request.setRequestFormData(!!data, data);
    if (data)
      headerList.push({name: 'Content-Length', value: data.length});
    request.setRequestHeaders(headerList);
    return request;
  }

  async function dumpRequest(headers, data, opt_url) {
    var win = await logView._generateCurlCommand(newRequest(headers, data, opt_url), 'win');
    var unix = await logView._generateCurlCommand(newRequest(headers, data, opt_url), 'unix');
    TestRunner.addResult(`Windows: ${win}`);
    TestRunner.addResult(`Unix: ${unix}`);
  }

  await dumpRequest({});
  await dumpRequest({}, '123');
  await dumpRequest({'Content-Type': 'application/x-www-form-urlencoded'}, '1&b');
  await dumpRequest({'Content-Type': 'application/json'}, '{"a":1}');
  await dumpRequest({'Content-Type': 'application/binary'}, '1234\r\n\x30\x30\2\3\4\5\'"!');
  await dumpRequest({'Content-Type': 'application/binary'}, '1234\r\n\1\x30\x30\2\3\4\5\'"!');
  await dumpRequest(
      {'Content-Type': 'application/binary'},
      '%OS%\r\n%%OS%%\r\n"\\"\'$&!');  // Ensure %OS% for windows is not env evaled
  await dumpRequest(
      {'Content-Type': 'application/binary'},
      '!@#$%^&*()_+~`1234567890-=[]{};\':",./\r<>?\r\nqwer\nt\n\nyuiopasdfghjklmnbvcxzQWERTYUIOPLKJHGFDSAZXCVBNM');
  await dumpRequest({'Content-Type': 'application/binary'}, '\x7F\x80\x90\xFF\u0009\u0700');
  await dumpRequest(
      {}, '',
      'http://labs.ft.com/?querystring=[]{}');  // Character range symbols must be escaped (http://crbug.com/265449).
  await dumpRequest({'Content-Type': 'application/binary'}, '%PATH%$PATH');
  await dumpRequest({':host': 'h', 'version': 'v'});

  TestRunner.completeTest();
})();
