// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that breakpoint added to file system can be removed\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.showPanel('sources');

  const testMapping = BindingsTestRunner.initializeTestMapping();
  const fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  const fsEntry = BindingsTestRunner.addFooJSFile(fs);
  await fs.reportCreatedPromise();
  const fsUISourceCode = await TestRunner.waitForUISourceCode(
      'foo.js', Workspace.projectTypes.FileSystem);
  const fsSourceFrame =
      await SourcesTestRunner.showUISourceCodePromise(fsUISourceCode);
  TestRunner.addResult('Add breakpoint');
  SourcesTestRunner.setBreakpoint(fsSourceFrame, 1, '', true);
  await waitAndDumpBreakpointsState(fsSourceFrame);

  TestRunner.addResult('Remove breakpoint');
  SourcesTestRunner.toggleBreakpoint(fsSourceFrame, 1, false);
  await waitAndDumpBreakpointsState(fsSourceFrame);

  TestRunner.addResult('Add breakpoint again');
  SourcesTestRunner.setBreakpoint(fsSourceFrame, 1, '', true);
  await waitAndDumpBreakpointsState(fsSourceFrame);

  // TODO(kozyatinskiy): after this step we should have both breakpoints by fs
  // url and by network url in storage otherwise if we close DevTools right now
  // and reopen it later without established mapping then we won't show
  // breakpoints in fileSystem UISourceCode.
  TestRunner.addResult('Add network mapping');
  await TestRunner.addIframe('resources/frame-with-foo-js.html');
  testMapping.addBinding('foo.js');
  await BindingsTestRunner.waitForBinding('foo.js');
  let networkUISourceCode = await TestRunner.waitForUISourceCode(
      'foo.js', Workspace.projectTypes.Network);
  let networkSourceFrame =
      await SourcesTestRunner.showUISourceCodePromise(networkUISourceCode);
  await waitAndDumpBreakpointsState(networkSourceFrame);

  TestRunner.addResult('Remove breakpoint');
  SourcesTestRunner.toggleBreakpoint(networkSourceFrame, 2, false);
  await waitAndDumpBreakpointsState(networkSourceFrame);

  TestRunner.addResult('Add breakpoint again');
  SourcesTestRunner.setBreakpoint(networkSourceFrame, 1, '', true);
  await waitAndDumpBreakpointsState(networkSourceFrame);

  TestRunner.addResult('Remove binding and drop iframe');
  await testMapping.removeBinding('foo.js');
  TestRunner.evaluateInPageAsync(`
    (function(){
      const frame = document.querySelector('iframe');
      frame.parentNode.removeChild(frame);
    })();
  `);
  await waitAndDumpBreakpointsState(fsSourceFrame);

  // TODO(kozyatinskiy): after this step we should have no breakpoints in
  // storage
  TestRunner.addResult('Remove breakpoint');
  SourcesTestRunner.toggleBreakpoint(fsSourceFrame, 1, false);
  await waitAndDumpBreakpointsState(fsSourceFrame);

  TestRunner.addResult('Add mapping back');
  TestRunner.addIframe('resources/frame-with-foo-js.html');
  testMapping.addBinding('foo.js');
  await BindingsTestRunner.waitForBinding('foo.js');
  networkUISourceCode = await TestRunner.waitForUISourceCode(
      'foo.js', Workspace.projectTypes.Network);
  networkSourceFrame =
      await SourcesTestRunner.showUISourceCodePromise(networkUISourceCode);
  await waitAndDumpBreakpointsState(networkSourceFrame);

  TestRunner.completeTest();

  function dumpBreakpointStorage() {
    var breakpointManager = Bindings.breakpointManager;
    var breakpoints = breakpointManager._storage._setting.get();
    TestRunner.addResult('Dumping breakpoint storage');
    for (const {lineNumber, url} of breakpoints)
      TestRunner.addResult(`  ${url}:${lineNumber}`);
  }

  async function waitAndDumpBreakpointsState(sourceFrame) {
    await SourcesTestRunner.waitBreakpointSidebarPane();
    dumpBreakpointStorage();
    SourcesTestRunner.dumpBreakpointSidebarPane();
    TestRunner.addResult('Source frame breakpoints');
    SourcesTestRunner.dumpJavaScriptSourceFrameBreakpoints(sourceFrame);
    TestRunner.addResult('');
  }
})();
