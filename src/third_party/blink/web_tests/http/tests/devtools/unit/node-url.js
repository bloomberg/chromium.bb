// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Checks url patching for Node.js');
  const url = Host.isWin()
      ? 'c:\\prog\\foobar.js'
      : '/usr/local/home/prog/foobar.js'
  let obj = {
    id: 0,
    result: {
      result: {
        value: {
          url: url
        }
      },
      exceptionDetails: {
        url: url,
        stackTrace: {
          callFrames: [{
            columnNumber: 0,
            functionName: '',
            lineNumber: 0,
            scriptId: "0",
            url: url
          }]
        }
      }
    }
  };
  Protocol.NodeURL.patch(obj);
  TestRunner.addResult(`..result.value.url patched: ${
    obj.result.result.value.url !== url}`);
  TestRunner.addResult(`..exceptionDetails.url patched: ${
    obj.result.exceptionDetails.url !== url}`);
  TestRunner.addResult(`..stackTrace.callFrames[0].url patched: ${
    obj.result.exceptionDetails.stackTrace.callFrames[0].url !== url}`);
  TestRunner.completeTest();
})()
