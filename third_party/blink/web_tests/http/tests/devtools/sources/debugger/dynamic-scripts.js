// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that scripts for dynamically added script elements are shown in sources panel if loaded with inspector open.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function appendDynamicScriptElement(src, content)
      {
          var scriptElement = document.createElement("script");
          if (src)
              scriptElement.src = src;
          else
              scriptElement.textContent = content;
          document.head.appendChild(scriptElement);
      }

      function loadScripts()
      {
          var sourceURLComment = "\\n //# sourceURL=";
          window.eval("function fooEval() {}");
          window.eval("function fooEvalSourceURL() {}" + sourceURLComment + "evalSourceURL.js");
          appendDynamicScriptElement("", "function fooScriptElementContent1() {}");
          appendDynamicScriptElement("", "function fooScriptElementContent2() {}");
          appendDynamicScriptElement("", "function fooScriptElementContentSourceURL() {}" + sourceURLComment + "scriptElementContentSourceURL.js");
          appendDynamicScriptElement("resources/dynamic-script.js");
      }

      function scriptLoaded()
      {
          console.log("Done.");
      }
  `);

  ConsoleTestRunner.addConsoleSniffer(step1);
  TestRunner.evaluateInPage('loadScripts()', function() {});

  function step1() {
    SourcesTestRunner.startDebuggerTest(step2);
  }

  function step2() {
    TestRunner.deprecatedRunAfterPendingDispatches(step3);
  }

  function step3() {
    var panel = UI.panels.sources;
    var uiSourceCodes = Workspace.workspace.uiSourceCodesForProjectType(Workspace.projectTypes.Network);
    var urls = uiSourceCodes.map(function(uiSourceCode) {
      return uiSourceCode.name();
    });
    urls.sort();

    var whiteList = [
      'debugger-test.js', 'dynamic-script.js', 'dynamic-scripts.js', 'evalSourceURL.js', 'inspector-test.js',
      'scriptElementContentSourceURL.js'
    ];
    function filter(url) {
      for (var i = 0; i < whiteList.length; ++i) {
        if (url === whiteList[i])
          return true;
      }

      return false;
    }
    urls = urls.filter(filter);

    TestRunner.addResult('UISourceCodes:');
    var lastURL;
    for (var i = 0; i < urls.length; ++i) {
      // Hack around the problem with scripts surviving between tests.
      if (urls[i] === lastURL && !lastURL.endsWith('.html'))
        continue;
      TestRunner.addResult('    ' + urls[i]);
      lastURL = urls[i];
    }
    SourcesTestRunner.completeDebuggerTest();
  }
})();
