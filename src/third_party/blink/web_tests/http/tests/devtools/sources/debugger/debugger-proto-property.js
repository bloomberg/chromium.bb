// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that object's __proto__ property is present in object properties section when script is paused on a breakpoint.Bug 41214\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function C()
      {
      }

      C.prototype = {
          m: function() { }
      }

      function testFunction()
      {
          var o = new C();
          var d = document.documentElement;
          debugger;
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.addSniffer(
        Sources.ScopeChainSidebarPane.prototype, '_sidebarPaneUpdatedForTest', onSidebarRendered, true);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(() => {});
  }

  function onSidebarRendered() {
    var localScope = SourcesTestRunner.scopeChainSections()[0];
    var properties = [
      localScope, ['o', '__proto__', '__proto__'], localScope,
      ['d', '__proto__', '__proto__', '__proto__', '__proto__', '__proto__']
    ];
    SourcesTestRunner.expandProperties(properties, step3);
  }

  function step3() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
