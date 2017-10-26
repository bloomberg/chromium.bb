// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests RemoteObject.getProperties on localStorage object. 66215\n`);
  await TestRunner.loadHTML(`
      <p>
      Tests RemoteObject.getProperties on localStorage object. <a href="https://bugs.webkit.org/show_bug.cgi?id=66215">66215</a>
      </p>
    `);
  await TestRunner.evaluateInPagePromise(`
      localStorage.testProperty = "testPropertyValue";
  `);

  var result = await TestRunner.RuntimeAgent.evaluate('localStorage');
  var localStorageHandle = TestRunner.runtimeModel.createRemoteObject(result);
  localStorageHandle.getOwnProperties(false, step2);

  function step2(properties) {
    for (var property of properties) {
      if (property.name !== 'testProperty')
        continue;
      property.value = {type: property.value.type, description: property.value.description};
      TestRunner.dump(property);
    }
    TestRunner.completeTest();
  }
})();
