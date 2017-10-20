// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests revision support in UISourceCode.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadHTML(`
      <a href="https://bugs.webkit.org/show_bug.cgi?id=97669">Bug 97669</a>
    `);

  var mockProjectId = 0;
  function createMockProject() {
    var workspace = new Workspace.Workspace();
    var project = new Bindings.ContentProviderBasedProject(workspace, 'mockProject:' + (++mockProjectId));
    project.requestFileContent = function(uri, callback) {
      callback('<script content>');
    };
    project.setFileContent = function(uri, newContent, callback) {};
    return project;
  }

  function dumpUISourceCodeWithRevisions(uiSourceCode) {
    var content = uiSourceCode._content;
    var revisionsString = '';
    var history = uiSourceCode.history();
    for (var i = 0; i < history.length; ++i) {
      var revision = history[i];
      if (i !== 0)
        revisionsString += ', ';
      revisionsString += '\'' + revision.content + '\'';
    }
    TestRunner.addResult('UISourceCode content: ' + content);
    TestRunner.addResult('    revisions: ' + revisionsString);
  }

  TestRunner.runTestSuite([
    function testAddRevisionsRevertToOriginal(next) {
      var uiSourceCode = new Workspace.UISourceCode(createMockProject(), 'url', Common.resourceTypes.Script);
      uiSourceCode.setWorkingCopy('content1');
      uiSourceCode.setWorkingCopy('content2');
      uiSourceCode.commitWorkingCopy(function() {});

      TestRunner.addResult('First revision added.');
      dumpUISourceCodeWithRevisions(uiSourceCode);
      uiSourceCode.setWorkingCopy('content3');
      uiSourceCode.setWorkingCopy('content4');
      uiSourceCode.commitWorkingCopy(function() {});

      TestRunner.addResult('Second revision added.');
      dumpUISourceCodeWithRevisions(uiSourceCode);
      uiSourceCode.revertToOriginal().then(onReverted);

      function onReverted() {
        TestRunner.addResult('Reverted to original.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        next();
      }
    },

    function testAddRevisionsRevertAndClearHistory(next) {
      var uiSourceCode = new Workspace.UISourceCode(createMockProject(), 'url2', Common.resourceTypes.Script);

      uiSourceCode.setWorkingCopy('content1');
      uiSourceCode.setWorkingCopy('content2');
      uiSourceCode.commitWorkingCopy(function() {});

      TestRunner.addResult('First revision added.');
      dumpUISourceCodeWithRevisions(uiSourceCode);
      uiSourceCode.setWorkingCopy('content3');
      uiSourceCode.setWorkingCopy('content4');
      uiSourceCode.commitWorkingCopy(function() {});

      TestRunner.addResult('Second revision added.');
      dumpUISourceCodeWithRevisions(uiSourceCode);
      uiSourceCode.revertAndClearHistory(revertedAndClearedHistory);

      function revertedAndClearedHistory() {
        TestRunner.addResult('Reverted and cleared history.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        next();
      }
    },

    function testAddRevisionsRevertToPrevious(next) {
      var uiSourceCode = new Workspace.UISourceCode(createMockProject(), 'url3', Common.resourceTypes.Script);

      uiSourceCode.setWorkingCopy('content1');
      uiSourceCode.setWorkingCopy('content2');
      uiSourceCode.commitWorkingCopy(function() {});

      TestRunner.addResult('First revision added.');
      dumpUISourceCodeWithRevisions(uiSourceCode);
      uiSourceCode.setWorkingCopy('content3');
      uiSourceCode.setWorkingCopy('content4');
      uiSourceCode.commitWorkingCopy(function() {});

      TestRunner.addResult('Second revision added.');
      dumpUISourceCodeWithRevisions(uiSourceCode);
      uiSourceCode.history()[0].revertToThis().then(onReverted);

      function onReverted() {
        TestRunner.addResult('Reverted to previous revision.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        next();
      }
    },

    function testRequestContentAddRevisionsRevertToOriginal(next) {
      var uiSourceCode = new Workspace.UISourceCode(createMockProject(), 'url4', Common.resourceTypes.Script);
      uiSourceCode.requestContent().then(contentReceived);

      function contentReceived() {
        TestRunner.addResult('Content requested.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        uiSourceCode.setWorkingCopy('content1');
        uiSourceCode.setWorkingCopy('content2');
        uiSourceCode.commitWorkingCopy(function() {});

        TestRunner.addResult('First revision added.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        uiSourceCode.setWorkingCopy('content3');
        uiSourceCode.setWorkingCopy('content4');
        uiSourceCode.commitWorkingCopy(function() {});

        TestRunner.addResult('Second revision added.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        uiSourceCode.revertToOriginal().then(onReverted);

        function onReverted() {
          TestRunner.addResult('Reverted to original.');
          dumpUISourceCodeWithRevisions(uiSourceCode);
          next();
        }
      }
    },

    function testRequestContentAddRevisionsRevertAndClearHistory(next) {
      var uiSourceCode = new Workspace.UISourceCode(createMockProject(), 'url5', Common.resourceTypes.Script);
      uiSourceCode.requestContent().then(contentReceived);

      function contentReceived() {
        TestRunner.addResult('Content requested.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        uiSourceCode.setWorkingCopy('content1');
        uiSourceCode.setWorkingCopy('content2');
        uiSourceCode.commitWorkingCopy(function() {});

        TestRunner.addResult('First revision added.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        uiSourceCode.setWorkingCopy('content3');
        uiSourceCode.setWorkingCopy('content4');
        uiSourceCode.commitWorkingCopy(function() {});

        TestRunner.addResult('Second revision added.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        uiSourceCode.revertAndClearHistory(revertedAndClearedHistory);

        function revertedAndClearedHistory() {
          TestRunner.addResult('Reverted and cleared history.');
          dumpUISourceCodeWithRevisions(uiSourceCode);
          next();
        }
      }
    },

    function testRequestContentAddRevisionsRevertToPrevious(next) {
      var uiSourceCode = new Workspace.UISourceCode(createMockProject(), 'url6', Common.resourceTypes.Script);
      uiSourceCode.requestContent().then(contentReceived);

      function contentReceived() {
        TestRunner.addResult('Content requested.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        uiSourceCode.setWorkingCopy('content1');
        uiSourceCode.setWorkingCopy('content2');
        uiSourceCode.commitWorkingCopy(function() {});

        TestRunner.addResult('First revision added.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        uiSourceCode.setWorkingCopy('content3');
        uiSourceCode.setWorkingCopy('content4');
        uiSourceCode.commitWorkingCopy(function() {});

        TestRunner.addResult('Second revision added.');
        dumpUISourceCodeWithRevisions(uiSourceCode);
        uiSourceCode.history()[0].revertToThis().then(onReverted);

        function onReverted() {
          TestRunner.addResult('Reverted to previous revision.');
          dumpUISourceCodeWithRevisions(uiSourceCode);
          next();
        }
      }
    },
  ]);
})();
