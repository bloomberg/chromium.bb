// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../../../../front_end/core/common/common.js';
import type * as Platform from '../../../../../front_end/core/platform/platform.js';
import type * as SDKModule from '../../../../../front_end/core/sdk/sdk.js';
import type * as Protocol from '../../../../../front_end/generated/protocol.js';
import * as Bindings from '../../../../../front_end/models/bindings/bindings.js';
import * as Workspace from '../../../../../front_end/models/workspace/workspace.js';
import {createTarget} from '../../helpers/EnvironmentHelpers.js';
import {describeWithMockConnection} from '../../helpers/MockConnection.js';

const {assert} = chai;

describeWithMockConnection('ResourceMapping', () => {
  let SDK: typeof SDKModule;
  before(async () => {
    SDK = await import('../../../../../front_end/core/sdk/sdk.js');
  });

  let debuggerModel: SDKModule.DebuggerModel.DebuggerModel;
  let resourceMapping: Bindings.ResourceMapping.ResourceMapping;
  let uiSourceCode: Workspace.UISourceCode.UISourceCode;

  // This test simulates the behavior of the ResourceMapping with the
  // following document, which contains two inline <script>s, one with
  // a `//# sourceURL` annotation and one without.
  //
  //  <!DOCTYPE html>
  //  <html>
  //  <head>
  //  <meta charset=utf-8>
  //  <script>
  //  function foo() { console.log("foo"); }
  //  foo();
  //  //# sourceURL=webpack:///src/foo.js
  //  </script>
  //  </head>
  //  <body>
  //  <script>console.log("bar");</script>
  //  </body>
  //  </html>
  //
  const url = 'http://example.com/index.html' as Platform.DevToolsPath.UrlString;
  const SCRIPTS = [
    {
      scriptId: '1' as Protocol.Runtime.ScriptId,
      startLine: 4,
      startColumn: 8,
      endLine: 8,
      endColumn: 0,
      sourceURL: 'webpack:///src/foo.js' as Platform.DevToolsPath.UrlString,
      hasSourceURLComment: true,
    },
    {
      scriptId: '2' as Protocol.Runtime.ScriptId,
      startLine: 11,
      startColumn: 8,
      endLine: 11,
      endColumn: 27,
      sourceURL: url,
      hasSourceURLComment: false,
    },
  ];
  const OTHER_SCRIPT_ID = '3' as Protocol.Runtime.ScriptId;

  beforeEach(() => {
    const target = createTarget();
    const targetManager = target.targetManager();
    const workspace = Workspace.Workspace.WorkspaceImpl.instance();
    resourceMapping = Bindings.ResourceMapping.ResourceMapping.instance({
      forceNew: true,
      targetManager,
      workspace,
    });

    // Inject the HTML document resource.
    const frameId = 'main' as Protocol.Page.FrameId;
    const mimeType = 'text/html';
    const resourceTreeModel =
        target.model(SDK.ResourceTreeModel.ResourceTreeModel) as SDKModule.ResourceTreeModel.ResourceTreeModel;
    const frame = resourceTreeModel.frameAttached(frameId, null) as SDKModule.ResourceTreeModel.ResourceTreeFrame;
    frame.addResource(new SDK.Resource.Resource(
        resourceTreeModel, null, url, url, frameId, null, Common.ResourceType.ResourceType.fromMimeType(mimeType),
        mimeType, null, null));
    uiSourceCode = workspace.uiSourceCodeForURL(url) as Workspace.UISourceCode.UISourceCode;
    assert.isNotNull(uiSourceCode);

    // Register the inline <script>s.
    const hash = '';
    const length = 0;
    const embedderName = url;
    const executionContextId = 1;
    debuggerModel = target.model(SDK.DebuggerModel.DebuggerModel) as SDKModule.DebuggerModel.DebuggerModel;
    SCRIPTS.forEach(({scriptId, startLine, startColumn, endLine, endColumn, sourceURL, hasSourceURLComment}) => {
      debuggerModel.parsedScriptSource(
          scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId, hash, undefined, false,
          undefined, hasSourceURLComment, false, length, false, null, null, null, null, embedderName);
    });
    assert.lengthOf(debuggerModel.scripts(), SCRIPTS.length);
  });

  describe('uiLocationToJSLocations', () => {
    it('does not map locations outside of <script> tags', () => {
      assert.isEmpty(resourceMapping.uiLocationToJSLocations(uiSourceCode, 0, 0));
      SCRIPTS.forEach(({startLine, startColumn, endLine, endColumn}) => {
        assert.isEmpty(resourceMapping.uiLocationToJSLocations(uiSourceCode, startLine, startColumn - 1));
        assert.isEmpty(resourceMapping.uiLocationToJSLocations(uiSourceCode, endLine, endColumn + 1));
      });
      assert.isEmpty(resourceMapping.uiLocationToJSLocations(uiSourceCode, 12, 1));
    });

    it('correctly maps inline <script> with a //# sourceURL annotation', () => {
      const {scriptId, startLine, startColumn, endLine, endColumn} = SCRIPTS[0];

      // Debugger locations in scripts with sourceURL annotations are relative to the beginning
      // of the script, rather than relative to the start of the surrounding document.
      assert.deepEqual(resourceMapping.uiLocationToJSLocations(uiSourceCode, startLine, startColumn), [
        debuggerModel.createRawLocationByScriptId(scriptId, 0, 0),
      ]);
      assert.deepEqual(resourceMapping.uiLocationToJSLocations(uiSourceCode, startLine, startColumn + 3), [
        // This location does not actually exist in the simulated document, but
        // the ResourceMapping doesn't know (and shouldn't care) about that.
        debuggerModel.createRawLocationByScriptId(scriptId, 0, 3),
      ]);
      assert.deepEqual(resourceMapping.uiLocationToJSLocations(uiSourceCode, startLine + 1, 5), [
        debuggerModel.createRawLocationByScriptId(scriptId, 1, 5),
      ]);
      assert.deepEqual(resourceMapping.uiLocationToJSLocations(uiSourceCode, endLine, endColumn), [
        debuggerModel.createRawLocationByScriptId(scriptId, endLine - startLine, endColumn),
      ]);
    });

    it('correctly maps inline <script> without //# sourceURL annotation', () => {
      const {scriptId, startLine, startColumn, endLine, endColumn} = SCRIPTS[1];

      // Debugger locations in scripts without sourceURL annotations are relative to the
      // beginning of the surrounding document, so this is basically a 1-1 mapping.
      assert.strictEqual(endLine, startLine);
      for (let column = startColumn; column <= endColumn; ++column) {
        assert.deepEqual(resourceMapping.uiLocationToJSLocations(uiSourceCode, startLine, column), [
          debuggerModel.createRawLocationByScriptId(scriptId, startLine, column),
        ]);
      }
    });
  });

  describe('jsLocationToUILocation', () => {
    it('does not map locations of unrelated scripts', () => {
      assert.isNull(
          resourceMapping.jsLocationToUILocation(debuggerModel.createRawLocationByScriptId(OTHER_SCRIPT_ID, 1, 1)));
      SCRIPTS.forEach(({startLine, startColumn, endLine, endColumn}) => {
        // Check that we also don't reverse map locations that overlap with the existing script locations.
        assert.isNull(resourceMapping.jsLocationToUILocation(
            debuggerModel.createRawLocationByScriptId(OTHER_SCRIPT_ID, startLine, startColumn)));
        assert.isNull(resourceMapping.jsLocationToUILocation(
            debuggerModel.createRawLocationByScriptId(OTHER_SCRIPT_ID, endLine, endColumn)));
      });
    });

    it('correctly maps inline <script> with //# sourceURL annotation', () => {
      const {scriptId, startLine, startColumn, endLine, endColumn} = SCRIPTS[0];

      // Debugger locations in scripts with sourceURL annotations are relative to the beginning
      // of the script, rather than relative to the start of the surrounding document.
      assert.deepEqual(
          resourceMapping.jsLocationToUILocation(debuggerModel.createRawLocationByScriptId(scriptId, 0, 0)),
          new Workspace.UISourceCode.UILocation(uiSourceCode, startLine, startColumn));
      assert.deepEqual(
          resourceMapping.jsLocationToUILocation(debuggerModel.createRawLocationByScriptId(scriptId, 0, 55)),
          // This location does not actually exist in the simulated document, but
          // the ResourceMapping doesn't know (and shouldn't care) about that.
          new Workspace.UISourceCode.UILocation(uiSourceCode, startLine, startColumn + 55));
      assert.deepEqual(
          resourceMapping.jsLocationToUILocation(debuggerModel.createRawLocationByScriptId(scriptId, 2, 0)),
          new Workspace.UISourceCode.UILocation(uiSourceCode, startLine + 2, 0));
      assert.deepEqual(
          resourceMapping.jsLocationToUILocation(
              debuggerModel.createRawLocationByScriptId(scriptId, endLine - startLine, endColumn)),
          new Workspace.UISourceCode.UILocation(uiSourceCode, endLine, endColumn));
    });

    it('correctly maps inline <script> without //# sourceURL annotation', () => {
      const {scriptId, startLine, startColumn, endLine, endColumn} = SCRIPTS[1];

      // Debugger locations in scripts without sourceURL annotations are relative to the
      // beginning of the surrounding document, so this is basically a 1-1 mapping.
      assert.strictEqual(endLine, startLine);
      for (let column = startColumn; column <= endColumn; ++column) {
        assert.deepEqual(
            resourceMapping.jsLocationToUILocation(
                debuggerModel.createRawLocationByScriptId(scriptId, startLine, column)),
            new Workspace.UISourceCode.UILocation(uiSourceCode, startLine, column));
      }
    });
  });
});
