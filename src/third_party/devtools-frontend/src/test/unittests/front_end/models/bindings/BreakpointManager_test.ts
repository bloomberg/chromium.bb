// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {assert} = chai;
import * as Root from '../../../../../front_end/core/root/root.js';
import * as SDK from '../../../../../front_end/core/sdk/sdk.js';
import * as Workspace from '../../../../../front_end/models/workspace/workspace.js';
import * as Bindings from '../../../../../front_end/models/bindings/bindings.js';
import type * as Platform from '../../../../../front_end/core/platform/platform.js';
import type * as Protocol from '../../../../../front_end/generated/protocol.js';
import * as Common from '../../../../../front_end/core/common/common.js';
import * as Persistence from '../../../../../front_end/models/persistence/persistence.js';

import {describeWithRealConnection} from '../../helpers/RealConnection.js';
import {createContentProviderUISourceCode, createFileSystemUISourceCode} from '../../helpers/UISourceCodeHelpers.js';
import {assertNotNullOrUndefined} from '../../../../../front_end/core/platform/platform.js';
import {TestPlugin} from '../../helpers/LanguagePluginHelpers.js';
import type {Chrome} from '../../../../../extension-api/ExtensionAPI.js';

describeWithRealConnection('BreakpointManager', () => {
  const URL = 'file:///tmp/example.html' as Platform.DevToolsPath.UrlString;
  const SCRIPT_ID = 'SCRIPT_ID' as Protocol.Runtime.ScriptId;
  const BREAKPOINT_ID = 'BREAKPOINT_ID' as Protocol.Debugger.BreakpointId;
  const JS_MIME_TYPE = 'text/javascript';

  let target: SDK.Target.Target;
  let breakpointManager: Bindings.BreakpointManager.BreakpointManager;
  class TestDebuggerModel extends SDK.DebuggerModel.DebuggerModel {
    constructor(target: SDK.Target.Target) {
      super(target);
    }

    async setBreakpointByURL(
        _url: Platform.DevToolsPath.UrlString, _lineNumber: number, _columnNumber?: number,
        _condition?: string): Promise<SDK.DebuggerModel.SetBreakpointResult> {
      return Promise.resolve(
          {breakpointId: BREAKPOINT_ID, locations: [new SDK.DebuggerModel.Location(this, SCRIPT_ID, 42)]});
    }

    async removeBreakpoint(): Promise<void> {
      return;
    }

    scriptForId(scriptId: string): SDK.Script.Script|null {
      if (scriptId === SCRIPT_ID) {
        return new SDK.Script.Script(
            this, scriptId as Protocol.Runtime.ScriptId, URL, 0, 0, 0, 0, 0, '', false, false, undefined, false, 0,
            null, null, null, null, null, null);
      }
      return null;
    }
  }

  function createFakeScriptMapping(debuggerModel: TestDebuggerModel, SCRIPT_ID: Protocol.Runtime.ScriptId):
      Bindings.DebuggerWorkspaceBinding.DebuggerSourceMapping {
    const sdkLocation = new SDK.DebuggerModel.Location(debuggerModel, SCRIPT_ID as Protocol.Runtime.ScriptId, 13);
    const mapping = {
      rawLocationToUILocation: (_: SDK.DebuggerModel.Location) => null,
      uiLocationToRawLocations:
          (_uiSourceCode: Workspace.UISourceCode.UISourceCode, _lineNumber: number,
           _columnNumber?: number) => [sdkLocation],
    };
    return mapping;
  }

  // Tests if a breakpoint set on a filesystem file was successfully moved to the network file when we expect it.
  async function runBreakpointMovedTest(fileSystem: {
    uiSourceCode: Workspace.UISourceCode.UISourceCode,
    project: Persistence.FileSystemWorkspaceBinding.FileSystem,
  }) {
    const debuggerModel = new TestDebuggerModel(target);
    const breakpoint = await breakpointManager.setBreakpoint(fileSystem.uiSourceCode, 0, 0, '', true);

    const content = await fileSystem.project.requestFileContent(fileSystem.uiSourceCode);
    const metadata = await fileSystem.project.requestMetadata(fileSystem.uiSourceCode);
    assertNotNullOrUndefined(metadata);
    assertNotNullOrUndefined(content.content);

    const networkURL = 'http://www.google.com/example.js' as Platform.DevToolsPath.UrlString;
    const network = createContentProviderUISourceCode({
      url: networkURL,
      content: content.content,
      mimeType: fileSystem.uiSourceCode.mimeType(),
      metadata,
      projectType: Workspace.Workspace.projectTypes.Network,
    });

    const script = new SDK.Script.Script(
        debuggerModel, SCRIPT_ID, networkURL, 0, 0, 43, 0, 0, '0', true, false, undefined, false, 10, null, null, null,
        null, null, null);

    // Check that only the filesystem project UISourceCode has a breakpoint.
    assert.lengthOf(breakpointManager.breakpointLocationsForUISourceCode(fileSystem.uiSourceCode), 1);
    assert.isEmpty(breakpointManager.breakpointLocationsForUISourceCode(network.uiSourceCode));

    // Get the UISourceCode and await binding updates. This call should make sure to update all breakpoints.
    await breakpointManager.getUISourceCodeWithUpdatedBreakpointInfo(script);

    // Check that the network project UISourceCode has a breakpoint now.
    const uiLocations = breakpointManager.breakpointLocationsForUISourceCode(network.uiSourceCode);
    assert.lengthOf(uiLocations, 1);

    // We need to remove the breakpoint on the file system and on the network project.
    breakpointManager.removeBreakpoint(breakpoint, true);
    breakpointManager.removeBreakpoint(uiLocations[0].breakpoint, true);

    Workspace.Workspace.WorkspaceImpl.instance().removeProject(network.project);
    Workspace.Workspace.WorkspaceImpl.instance().removeProject(fileSystem.project);
  }

  beforeEach(() => {
    breakpointManager = Bindings.BreakpointManager.BreakpointManager.instance();
    assertNotNullOrUndefined(breakpointManager);

    const targetManager = SDK.TargetManager.TargetManager.instance();
    const mainTarget = targetManager.mainTarget();
    assertNotNullOrUndefined(mainTarget);
    target = mainTarget;
  });

  it('allows awaiting the restoration of breakpoints', async () => {
    Root.Runtime.experiments.enableForTest(Root.Runtime.ExperimentName.INSTRUMENTATION_BREAKPOINTS);

    const {uiSourceCode, project} = createContentProviderUISourceCode({url: URL, mimeType: JS_MIME_TYPE});
    const breakpoint = await breakpointManager.setBreakpoint(uiSourceCode, 0, 0, '', true);

    // Create a new DebuggerModel and notify the breakpoint engine about it.
    const debuggerModel = new TestDebuggerModel(target);
    breakpoint.modelAdded(debuggerModel);

    // Make sure that we await all updates that are triggered by adding the model.
    await breakpoint.updateBreakpoint();

    // Retrieve the ModelBreakpoint that is linked to our DebuggerModel.
    const modelBreakpoint = breakpoint.modelBreakpoint(debuggerModel);
    assertNotNullOrUndefined(modelBreakpoint);

    // Make sure that we do not have a linked script yet.
    assert.isNull(modelBreakpoint.currentState);

    // Create a fake mapping that can be used to set a breakpoint.
    const mapping = createFakeScriptMapping(debuggerModel, SCRIPT_ID);
    Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().addSourceMapping(mapping);

    // Now await restoring the breakpoint.
    // A successful restore should update the ModelBreakpoint of the DebuggerModel
    // to reflect a state, in which we have successfully set a breakpoint (i.e. a script id
    // is available).
    const script = debuggerModel.scriptForId(SCRIPT_ID);
    assertNotNullOrUndefined(script);
    await breakpointManager.restoreBreakpointsForScript(script);
    assertNotNullOrUndefined(modelBreakpoint.currentState);
    assert.lengthOf(modelBreakpoint.currentState.positions, 1);
    assert.strictEqual(modelBreakpoint.currentState.positions[0].scriptId, SCRIPT_ID);

    // Clean up.
    breakpointManager.removeBreakpoint(breakpoint, true);
    breakpointManager.modelRemoved(debuggerModel);
    Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().removeSourceMapping(mapping);
    Workspace.Workspace.WorkspaceImpl.instance().removeProject(project);
    Root.Runtime.experiments.disableForTest(Root.Runtime.ExperimentName.INSTRUMENTATION_BREAKPOINTS);
  });

  it('allows awaiting the restoration of breakpoints with language plugins', async () => {
    Root.Runtime.experiments.enableForTest(Root.Runtime.ExperimentName.INSTRUMENTATION_BREAKPOINTS);
    Root.Runtime.experiments.enableForTest(Root.Runtime.ExperimentName.WASM_DWARF_DEBUGGING);

    const pluginManager =
        Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().initPluginManagerForTest();
    assertNotNullOrUndefined(pluginManager);

    const debuggerModel = target.model(SDK.DebuggerModel.DebuggerModel);
    assertNotNullOrUndefined(debuggerModel);

    const {uiSourceCode, project} =
        createContentProviderUISourceCode({url: 'test.cc' as Platform.DevToolsPath.UrlString, mimeType: JS_MIME_TYPE});
    assertNotNullOrUndefined(uiSourceCode);
    const breakpoint = await breakpointManager.setBreakpoint(uiSourceCode, 0, 0, '', true);

    // Make sure that we await all updates that are triggered by adding the model.
    await breakpoint.updateBreakpoint();

    // Retrieve the ModelBreakpoint that is linked to our DebuggerModel.
    const modelBreakpoint = breakpoint.modelBreakpoint(debuggerModel);
    assertNotNullOrUndefined(modelBreakpoint);

    // Make sure that we do not have a linked script yet.
    assert.isNull(modelBreakpoint.currentState);

    class Plugin extends TestPlugin {
      constructor() {
        super('InstrumentationBreakpoints');
      }

      handleScript(script: SDK.Script.Script) {
        return script.scriptId === SCRIPT_ID;
      }

      async sourceLocationToRawLocation(sourceLocation: Chrome.DevTools.SourceLocation):
          Promise<Chrome.DevTools.RawLocationRange[]> {
        const {rawModuleId, columnNumber, lineNumber, sourceFileURL} = sourceLocation;
        if (lineNumber === 0 && columnNumber === 0 && sourceFileURL === 'test.cc') {
          return [{rawModuleId, startOffset: 0, endOffset: 0}];
        }
        return [];
      }

      async addRawModule(_rawModuleId: string, _symbolsURL: string, _rawModule: Chrome.DevTools.RawModule):
          Promise<string[]> {
        return ['test.cc'];  // need to return something to get the script associated with the plugin.
      }
    }
    // Create a plugin that is able to produce a mapping for our script.
    pluginManager.addPlugin(new Plugin());

    const script = debuggerModel.parsedScriptSource(
        SCRIPT_ID, URL, 0, 0, 0, 0, 0, '', undefined, false, undefined, false, false, 0, null, null, null, null, null,
        null);
    await pluginManager.getSourcesForScript(script);  // wait for plugin source setup to finish.

    await breakpointManager.restoreBreakpointsForScript(script);
    assertNotNullOrUndefined(modelBreakpoint.currentState);
    assert.lengthOf(modelBreakpoint.currentState.positions, 1);
    assert.strictEqual(modelBreakpoint.currentState.positions[0].scriptId, SCRIPT_ID);

    // Clean up.
    breakpointManager.removeBreakpoint(breakpoint, true);
    Root.Runtime.experiments.disableForTest(Root.Runtime.ExperimentName.INSTRUMENTATION_BREAKPOINTS);
    Root.Runtime.experiments.disableForTest(Root.Runtime.ExperimentName.WASM_DWARF_DEBUGGING);
    Workspace.Workspace.WorkspaceImpl.instance().removeProject(project);
    Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().initPluginManagerForTest();
    debuggerModel.globalObjectCleared();
  });

  it('allows awaiting on scheduled update in debugger', async () => {
    const {uiSourceCode, project} = createContentProviderUISourceCode({url: URL, mimeType: JS_MIME_TYPE});

    const debuggerModel = new TestDebuggerModel(target);
    const breakpoint = await breakpointManager.setBreakpoint(uiSourceCode, 42, 0, '', true);

    const modelBreakpoint = new Bindings.BreakpointManager.ModelBreakpoint(
        debuggerModel, breakpoint, breakpointManager.debuggerWorkspaceBinding);
    const mapping = createFakeScriptMapping(debuggerModel, SCRIPT_ID);
    Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().addSourceMapping(mapping);
    assert.isNull(breakpoint.currentState);
    const update = modelBreakpoint.scheduleUpdateInDebugger();
    assert.isNull(breakpoint.currentState);
    await update;
    assert.strictEqual(breakpoint.currentState?.positions[0]?.lineNumber, 13);
    Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().removeSourceMapping(mapping);
    breakpointManager.removeBreakpoint(breakpoint, true);
    Workspace.Workspace.WorkspaceImpl.instance().removeProject(project);
  });

  it('allows awaiting on removal of breakpoint in debugger', async () => {
    const {uiSourceCode, project} = createContentProviderUISourceCode({url: URL, mimeType: JS_MIME_TYPE});
    // Set up breakpoint with UISourceCode, and fake DebuggerModel.
    const debuggerModel = new TestDebuggerModel(target);
    const removeSpy = sinon.spy(debuggerModel, 'removeBreakpoint');
    const setSpy = sinon.spy(debuggerModel, 'setBreakpointByURL');

    // We need to stub the debuggerModel of the real connection to make sure that we
    // can await the removal of the breakpoint (since it will await updating all
    // DebuggerModels, including the one with the real connection).
    const realDebugger = target.model(SDK.DebuggerModel.DebuggerModel);
    assertNotNullOrUndefined(realDebugger);
    sinon.stub(realDebugger, 'setBreakpointByURL')
        .callsFake(() => Promise.resolve({breakpointId: BREAKPOINT_ID, locations: []}));
    sinon.stub(realDebugger, 'removeBreakpoint').callsFake(() => Promise.resolve());

    const mapping = createFakeScriptMapping(debuggerModel, SCRIPT_ID);
    Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().addSourceMapping(mapping);

    const breakpoint = await breakpointManager.setBreakpoint(uiSourceCode, 42, 0, '', true);
    breakpoint.modelAdded(debuggerModel);

    // Make sure that the location could be resolved, and that we could set a breakpoint.
    const modelBreakpoint = breakpoint.modelBreakpoint(debuggerModel);
    assertNotNullOrUndefined(modelBreakpoint);
    await modelBreakpoint.scheduleUpdateInDebugger();
    assertNotNullOrUndefined(modelBreakpoint.currentState);
    assert.isTrue(setSpy.calledOnce);

    // Test if awaiting breakpoint.remove is actually removing the state.
    await breakpoint.remove(false);
    assert.isNull(modelBreakpoint.currentState);
    assert.isTrue(removeSpy.calledOnce);

    Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().removeSourceMapping(mapping);
    breakpointManager.removeBreakpoint(breakpoint, true);
    Workspace.Workspace.WorkspaceImpl.instance().removeProject(project);
  });

  it('can wait for file system breakpoints to be mapped to network ui source code', async () => {
    const url = 'file://example.js' as Platform.DevToolsPath.UrlString;
    const content = 'console.log(3)';
    const metadata = new Workspace.UISourceCode.UISourceCodeMetadata(new Date(), content.length);

    const fileSystem =
        createFileSystemUISourceCode({url, content, mimeType: JS_MIME_TYPE, metadata, autoMapping: true});

    await runBreakpointMovedTest(fileSystem);
  });

  describe('with persistence network overrides enabled', () => {
    let currentPersistenceSetting: boolean;

    beforeEach(() => {
      // Temporarily enable overrides for test.
      currentPersistenceSetting =
          Common.Settings.Settings.instance().moduleSetting('persistenceNetworkOverridesEnabled').get();
    });

    afterEach(() => {
      // Reset default setting.
      Common.Settings.Settings.instance()
          .moduleSetting('persistenceNetworkOverridesEnabled')
          .set(currentPersistenceSetting);
    });

    it('can wait for breakpoints in overrides to be mapped to network ui source code', async () => {
      Common.Settings.Settings.instance().moduleSetting('persistenceNetworkOverridesEnabled').set('true');
      const metadata = new Workspace.UISourceCode.UISourceCodeMetadata(new Date(), 0);
      const url = 'file://path/to/overrides/www.google.com/example.js' as Platform.DevToolsPath.UrlString;
      const fileSystem = createFileSystemUISourceCode({
        url,
        metadata,
        mimeType: JS_MIME_TYPE,
        autoMapping: true,
        type: 'overrides',
        fileSystemPath: 'file://path/to/overrides',
      });

      // Add a spy to make sure that the binding is coming from the NetworkPersistenceManager, and not the Automapping.
      const spy = sinon.spy(Persistence.Persistence.PersistenceImpl.instance(), 'addBinding');
      await Persistence.NetworkPersistenceManager.NetworkPersistenceManager.instance().setProject(fileSystem.project);
      await runBreakpointMovedTest(fileSystem);
      assert.isTrue(spy.calledOnce);
    });
  });
});
