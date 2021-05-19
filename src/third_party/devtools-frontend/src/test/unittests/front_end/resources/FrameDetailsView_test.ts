// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as SDK from '../../../../front_end/core/sdk/sdk.js';
import * as Resources from '../../../../front_end/panels/application/application.js';
import * as Coordinator from '../../../../front_end/render_coordinator/render_coordinator.js';
import * as Components from '../../../../front_end/ui/components/components.js';
import {assertShadowRoot, getCleanTextContentFromElements, getElementWithinComponent, renderElementIntoDOM} from '../helpers/DOMHelpers.js';

const coordinator = Coordinator.RenderCoordinator.RenderCoordinator.instance();

const {assert} = chai;

const makeFrame = (): SDK.ResourceTreeModel.ResourceTreeFrame => {
  const newFrame: SDK.ResourceTreeModel.ResourceTreeFrame = {
    url: 'https://www.example.com/path/page.html',
    securityOrigin: 'https://www.example.com',
    displayName: () => 'TestTitle',
    unreachableUrl: () => '',
    adFrameType: () => Protocol.Page.AdFrameType.None,
    resourceForURL: () => null,
    isSecureContext: () => true,
    isCrossOriginIsolated: () => true,
    getSecureContextType: () => Protocol.Page.SecureContextType.SecureLocalhost,
    getGatedAPIFeatures: () =>
        [Protocol.Page.GatedAPIFeatures.SharedArrayBuffers,
         Protocol.Page.GatedAPIFeatures.SharedArrayBuffersTransferAllowed],
    getOwnerDOMNodeOrDocument: () => ({
      nodeName: () => 'iframe',
    }),
    resourceTreeModel: () => ({
      target: () => ({
        // set to true so that Linkifier.maybeLinkifyScriptLocation() exits
        // early and does not run into other problems with this mock
        isDisposed: () => true,
        model: () => ({
          getSecurityIsolationStatus: () => ({
            coep: {
              value: Protocol.Network.CrossOriginEmbedderPolicyValue.None,
              reportOnlyValue: Protocol.Network.CrossOriginEmbedderPolicyValue.None,
            },
            coop: {
              value: Protocol.Network.CrossOriginOpenerPolicyValue.SameOrigin,
              reportOnlyValue: Protocol.Network.CrossOriginOpenerPolicyValue.SameOrigin,
            },
          }),
        }),
      }),
    }),
    _creationStackTrace: {
      callFrames: [{
        functionName: 'function1',
        url: 'http://www.example.com/script.js',
        lineNumber: 15,
        columnNumber: 10,
        scriptId: 'someScriptId',
      }],
    },
  } as unknown as SDK.ResourceTreeModel.ResourceTreeFrame;
  return newFrame;
};

describe('FrameDetailsView', () => {
  it('renders with a title', async () => {
    const frame = makeFrame();
    const component = new Resources.FrameDetailsView.FrameDetailsReportView();
    renderElementIntoDOM(component);
    component.data = {
      frame: frame,
    };

    assertShadowRoot(component.shadowRoot);
    const report = getElementWithinComponent(component, 'devtools-report', Components.ReportView.Report);
    assertShadowRoot(report.shadowRoot);

    const titleElement = report.shadowRoot.querySelector('.report-title');
    assert.strictEqual(titleElement?.textContent, frame.displayName());
  });

  it('renders report keys and values', async () => {
    const frame = makeFrame();
    const component = new Resources.FrameDetailsView.FrameDetailsReportView();
    renderElementIntoDOM(component);
    component.data = {
      frame: frame,
    };

    assertShadowRoot(component.shadowRoot);
    await coordinator.done();
    await coordinator.done();  // 2nd call awaits async render

    const keys = getCleanTextContentFromElements(component.shadowRoot, 'devtools-report-key');
    assert.deepEqual(keys, [
      'URL',
      'Origin',
      'Owner Element',
      'Frame Creation Stack Trace',
      'Secure Context',
      'Cross-Origin Isolated',
      'Cross-Origin Embedder Policy',
      'Cross-Origin Opener Policy',
      'SharedArrayBuffers',
      'Measure Memory',
    ]);

    const values = getCleanTextContentFromElements(component.shadowRoot, 'devtools-report-value');
    assert.deepEqual(values, [
      'https://www.example.com/path/page.html',
      'https://www.example.com',
      '<iframe>',
      '',
      'Yes Localhost is always a secure context',
      'Yes',
      'None',
      'SameOrigin',
      'available, transferable',
      'available Learn more',
    ]);

    const stackTrace =
        getElementWithinComponent(component, 'devtools-resources-stack-trace', Resources.StackTrace.StackTrace);
    assertShadowRoot(stackTrace.shadowRoot);
    const expandableList =
        getElementWithinComponent(stackTrace, 'devtools-expandable-list', Components.ExpandableList.ExpandableList);
    assertShadowRoot(expandableList.shadowRoot);
    const expandableListText = getCleanTextContentFromElements(expandableList.shadowRoot, '.stack-trace-row');
    assert.deepEqual(expandableListText, ['function1 @ www.example.com/script.js:16']);
  });
});
