// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Components from '../../../../front_end/components/components.js';
import * as SDK from '../../../../front_end/core/sdk/sdk.js';
import * as Resources from '../../../../front_end/panels/application/application.js';
import * as UIComponents from '../../../../front_end/ui/components/components.js';
import {assertElement, assertShadowRoot, dispatchClickEvent, getCleanTextContentFromElements, getElementWithinComponent, renderElementIntoDOM} from '../helpers/DOMHelpers.js';

const {assert} = chai;

const makeFrame =
    (overrides: Partial<SDK.ResourceTreeModel.ResourceTreeFrame> = {}): SDK.ResourceTreeModel.ResourceTreeFrame => {
      const newFrame: SDK.ResourceTreeModel.ResourceTreeFrame = {
        resourceTreeModel: () => ({
          target: () => ({}),
        }),
        ...overrides,
      } as unknown as SDK.ResourceTreeModel.ResourceTreeFrame;
      return newFrame;
    };

function mockBuildStackTraceRows(
    stackTrace: Protocol.Runtime.StackTrace,
    _target: SDK.SDKModel.Target|null,
    _linkifier: Components.Linkifier.Linkifier,
    _tabStops: boolean|undefined,
    _updateCallback?: (arg0: (Components.JSPresentationUtils.StackTraceRegularRow|
                              Components.JSPresentationUtils.StackTraceAsyncRow)[]) => void,
    ): (Components.JSPresentationUtils.StackTraceRegularRow|Components.JSPresentationUtils.StackTraceAsyncRow)[] {
  return stackTrace.callFrames.map(callFrame => ({
                                     functionName: callFrame.functionName,
                                     ignoreListHide: callFrame.url.includes('hidden'),
                                     link: Components.Linkifier.Linkifier.linkifyURL(callFrame.url),
                                     rowCountHide: false,
                                   }));
}

describe('StackTrace', () => {
  it('does not generate rows when there is no data', () => {
    const component = new Resources.StackTrace.StackTrace();
    const rows = component.createRowTemplates();
    assert.deepEqual(rows, []);
  });

  it('generates rows from stack trace data', () => {
    const frame = makeFrame({
      _creationStackTrace: {
        callFrames: [
          {
            functionName: 'function1',
            url: 'http://www.example.com/script1.js',
            lineNumber: 15,
            columnNumber: 10,
            scriptId: 'someScriptId',
          },
          {
            functionName: 'function2',
            url: 'http://www.example.com/script2.js',
            lineNumber: 20,
            columnNumber: 5,
            scriptId: 'someScriptId',
          },
        ],
      },
    });
    const component = new Resources.StackTrace.StackTrace();
    renderElementIntoDOM(component);
    component.data = {
      frame: frame,
      buildStackTraceRows: mockBuildStackTraceRows,
    };

    assertShadowRoot(component.shadowRoot);
    const expandableList =
        getElementWithinComponent(component, 'devtools-expandable-list', UIComponents.ExpandableList.ExpandableList);
    assertShadowRoot(expandableList.shadowRoot);
    const expandButton = expandableList.shadowRoot.querySelector('button.arrow-icon-button');
    assertElement(expandButton, HTMLButtonElement);
    dispatchClickEvent(expandButton);

    const stackTraceText = getCleanTextContentFromElements(expandableList.shadowRoot, '.stack-trace-row');
    assert.deepEqual(stackTraceText, [
      'function1 @ www.example.com/script1.js',
      'function2 @ www.example.com/script2.js',
    ]);
  });

  it('hides hidden rows behind "show all" button', () => {
    const frame = makeFrame({
      _creationStackTrace: {
        callFrames: [
          {
            functionName: 'function1',
            url: 'http://www.example.com/script.js',
            lineNumber: 15,
            columnNumber: 10,
            scriptId: 'someScriptId',
          },
          {
            functionName: 'function2',
            url: 'http://www.example.com/hidden.js',
            lineNumber: 20,
            columnNumber: 5,
            scriptId: 'someScriptId',
          },
        ],
      },
    });
    const component = new Resources.StackTrace.StackTrace();
    renderElementIntoDOM(component);
    component.data = {
      frame: frame,
      buildStackTraceRows: mockBuildStackTraceRows,
    };

    assertShadowRoot(component.shadowRoot);
    const expandableList =
        getElementWithinComponent(component, 'devtools-expandable-list', UIComponents.ExpandableList.ExpandableList);
    assertShadowRoot(expandableList.shadowRoot);
    const expandButton = expandableList.shadowRoot.querySelector('button.arrow-icon-button');
    assertElement(expandButton, HTMLButtonElement);
    dispatchClickEvent(expandButton);

    let stackTraceText = getCleanTextContentFromElements(expandableList.shadowRoot, '.stack-trace-row');
    assert.deepEqual(stackTraceText, [
      'function1 @ www.example.com/script.js',
      'Show 1 more frame',
    ]);

    const showAllButton = expandableList.shadowRoot.querySelector('.stack-trace-row button.link');
    assertElement(showAllButton, HTMLButtonElement);
    dispatchClickEvent(showAllButton);

    stackTraceText = getCleanTextContentFromElements(expandableList.shadowRoot, '.stack-trace-row');
    assert.deepEqual(stackTraceText, [
      'function1 @ www.example.com/script.js',
      'function2 @ www.example.com/hidden.js',
    ]);
  });
});
