// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests resources panel history.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.addStylesheetTag('../styles/resources/styles-history.css');

  ApplicationTestRunner.runAfterCachedResourcesProcessed(runTestSuite);

  var uiSourceCode;

  function runTestSuite() {
    TestRunner.runTestSuite([
      function testSetUp(next) {
        function visitUISourceCodes(currentUISourceCode) {
          if (currentUISourceCode.url().indexOf('styles-history.css') === -1)
            return;
          uiSourceCode = currentUISourceCode;
          next();
        }
        Workspace.workspace.uiSourceCodes().forEach(visitUISourceCodes);
      },

      function testSetResourceContentMinor(next) {
        TestRunner.addSniffer(Bindings.StyleFile.prototype, '_styleFileSyncedForTest', styleUpdatedMinor);
        uiSourceCode.setWorkingCopy('body {\n  margin: 15px;\n  padding: 10px;\n}');

        function styleUpdatedMinor() {
          dumpHistory(next)();
        }
      },

      function testSetResourceContentMajor(next) {
        TestRunner.addSniffer(Bindings.StyleFile.prototype, '_styleFileSyncedForTest', styleUpdatedMinor);
        uiSourceCode.setWorkingCopy('body {\n  margin: 20px;\n  padding: 10px;\n}');

        function styleUpdatedMinor() {
          TestRunner.addSniffer(Bindings.StyleFile.prototype, '_styleFileSyncedForTest', styleUpdatedMajor);
          uiSourceCode.commitWorkingCopy(function() {});

          function styleUpdatedMajor() {
            dumpHistory(next)();
          }
        }
      },

      function testSetContentViaModelMinor(next) {
        styleSheetForResource(step1);

        function step1(style) {
          var property = getLiveProperty(style, 'margin');
          property.setText('margin:25px;', false, true).then(dumpHistory(next));
        }
      },

      function testSetContentViaModelMajor(next) {
        styleSheetForResource(step1);

        function step1(style) {
          var property = getLiveProperty(style, 'margin');
          TestRunner.addSniffer(Workspace.UISourceCode.prototype, 'addRevision', dumpHistory(next));
          property.setText('margin:30px;', true, true);
        }
      }
    ]);
  }

  function styleSheetForResource(callback) {
    ElementsTestRunner.nodeWithId('mainBody', onNodeSelected);

    function onNodeSelected(node) {
      TestRunner.CSSAgent.invoke_getMatchedStylesForNode({nodeId: node.id}).then(onMatchedStylesForNode);
    }

    function onMatchedStylesForNode(response) {
      var error = response[Protocol.Error];
      if (error) {
        TestRunner.addResult('error: ' + error);
        TestRunner.completeTest();
        return;
      }
      for (var matchedStyle of response.matchedCSSRules) {
        var rule = matchedStyle.rule;
        if (rule.origin !== 'regular')
          continue;
        callback(new SDK.CSSStyleDeclaration(TestRunner.cssModel, null, rule.style));
        return;
      }
      TestRunner.addResult('error: did not find any regular rule');
      TestRunner.completeTest();
    }
  }

  function dumpHistory(next) {
    function result() {
      TestRunner.addResult('History length: ' + uiSourceCode.history().length);
      for (var i = 0; i < uiSourceCode.history().length; ++i) {
        TestRunner.addResult('Item ' + i + ':');
        TestRunner.addResult(uiSourceCode.history()[i].content);
      }
      next();
    }
    return result;
  }

  function getLiveProperty(style, name) {
    for (var property of style.allProperties()) {
      if (!property.activeInStyle())
        continue;
      if (property.name === name)
        return property;
    }
    return null;
  }
})();
