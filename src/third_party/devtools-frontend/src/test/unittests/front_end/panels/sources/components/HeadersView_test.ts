// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Workspace from '../../../../../../front_end/models/workspace/workspace.js';
import * as SourcesComponents from '../../../../../../front_end/panels/sources/components/components.js';
import * as Coordinator from '../../../../../../front_end/ui/components/render_coordinator/render_coordinator.js';
import {assertElement, assertShadowRoot, dispatchKeyDownEvent, renderElementIntoDOM} from '../../../helpers/DOMHelpers.js';
import {deinitializeGlobalVars, initializeGlobalVars} from '../../../helpers/EnvironmentHelpers.js';
import {createUISourceCode} from '../../../helpers/UISourceCodeHelpers.js';

import type * as Platform from '../../../../../../front_end/core/platform/platform.js';

const {assert} = chai;
const coordinator = Coordinator.RenderCoordinator.RenderCoordinator.instance();

describe('HeadersView', async () => {
  before(async () => {
    await initializeGlobalVars();
  });
  after(async () => {
    await deinitializeGlobalVars();
  });

  async function renderEditor(): Promise<SourcesComponents.HeadersView.HeadersViewComponent> {
    const editor = new SourcesComponents.HeadersView.HeadersViewComponent();
    editor.data = {
      headerOverrides: [
        {
          applyTo: '*',
          headers: [
            {
              name: 'server',
              value: 'DevTools Unit Test Server',
            },
            {
              name: 'access-control-allow-origin',
              value: '*',
            },
          ],
        },
        {
          applyTo: '*.jpg',
          headers: [
            {
              name: 'jpg-header',
              value: 'only for jpg files',
            },
          ],
        },
      ],
      parsingError: false,
      uiSourceCode: {
        name: () => '.headers',
        setWorkingCopy: () => {},
      } as unknown as Workspace.UISourceCode.UISourceCode,
    };
    renderElementIntoDOM(editor);
    assertShadowRoot(editor.shadowRoot);
    await coordinator.done();
    return editor;
  }

  async function renderEditorWithinWrapper(): Promise<SourcesComponents.HeadersView.HeadersViewComponent> {
    const workspace = Workspace.Workspace.WorkspaceImpl.instance();
    const headers = `[
      {
        "applyTo": "*",
        "headers": {
          "server": "DevTools Unit Test Server",
          "access-control-allow-origin": "*"
        }
      },
      {
        "applyTo": "*.jpg",
        "headers": {
          "jpg-header": "only for jpg files"
        }
      }
    ]`;
    const {uiSourceCode, project} = createUISourceCode({
      url: 'file:///path/to/overrides/example.html' as Platform.DevToolsPath.UrlString,
      mimeType: 'text/html',
      content: headers,
    });
    project.canSetFileContent = () => true;

    const editorWrapper = new SourcesComponents.HeadersView.HeadersView(uiSourceCode);
    await uiSourceCode.requestContent();
    await coordinator.done();
    const editor = editorWrapper.getComponent();
    renderElementIntoDOM(editor);
    assertShadowRoot(editor.shadowRoot);
    await coordinator.done();
    workspace.removeProject(project);
    return editor;
  }

  async function changeEditable(editable: HTMLElement, value: string): Promise<void> {
    editable.focus();
    editable.innerText = value;
    dispatchKeyDownEvent(editable, {
      key: 'Enter',
    });
    await coordinator.done();
  }

  async function pressButton(shadowRoot: ShadowRoot, rowIndex: number, selector: string): Promise<void> {
    const rowElements = shadowRoot.querySelectorAll('.row');
    const button = rowElements[rowIndex].querySelector(selector);
    assertElement(button, HTMLElement);
    button.click();
    await coordinator.done();
  }

  function getRowContent(shadowRoot: ShadowRoot): string[] {
    const rows = Array.from(shadowRoot.querySelectorAll('.row'));
    return rows.map(row => {
      return Array.from(row.querySelectorAll('div, .editable')).map(element => element.textContent).join('');
    });
  }

  function isWholeElementContentSelected(element: HTMLElement): boolean {
    const textContent = element.textContent;
    if (!textContent || textContent.length < 1 || !element.hasSelection()) {
      return false;
    }
    const selection = element.getComponentSelection();
    if (!selection || selection.rangeCount < 1) {
      return false;
    }
    if (selection.anchorNode !== selection.focusNode) {
      return false;
    }
    const range = selection.getRangeAt(0);
    return (range.endOffset - range.startOffset === textContent.length);
  }

  it('shows an error message when parsingError is true', async () => {
    const editor = new SourcesComponents.HeadersView.HeadersViewComponent();
    editor.data = {
      headerOverrides: [],
      parsingError: true,
      uiSourceCode: {
        name: () => '.headers',
      } as Workspace.UISourceCode.UISourceCode,
    };
    renderElementIntoDOM(editor);
    assertShadowRoot(editor.shadowRoot);
    await coordinator.done();

    const errorHeader = editor.shadowRoot.querySelector('.error-header');
    assert.strictEqual(errorHeader?.textContent, 'Error when parsing \'.headers\'.');
  });

  it('displays data and allows editing', async () => {
    const editor = await renderEditor();
    assertShadowRoot(editor.shadowRoot);

    let rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*',
      'server:DevTools Unit Test Server',
      'access-control-allow-origin:*',
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
    ]);

    const editables = editor.shadowRoot?.querySelectorAll('.editable');
    await changeEditable(editables[0] as HTMLElement, 'index.html');
    await changeEditable(editables[1] as HTMLElement, 'content-type');
    await changeEditable(editables[4] as HTMLElement, 'example.com');
    await changeEditable(editables[7] as HTMLElement, 'is image');

    rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:index.html',
      'content-type:DevTools Unit Test Server',
      'access-control-allow-origin:example.com',
      'Apply to:*.jpg',
      'jpg-header:is image',
    ]);
  });

  it('selects the whole content when clicking on an editable field', async () => {
    const editor = await renderEditor();
    assertShadowRoot(editor.shadowRoot);
    const editables = editor.shadowRoot?.querySelectorAll('.editable');

    let element = editables[0] as HTMLElement;
    element.focus();
    assert.isTrue(isWholeElementContentSelected(element));

    element = editables[1] as HTMLElement;
    element.focus();
    assert.isTrue(isWholeElementContentSelected(element));

    element = editables[2] as HTMLElement;
    element.focus();
    assert.isTrue(isWholeElementContentSelected(element));
  });

  it('un-selects the content when an editable field loses focus', async () => {
    const editor = await renderEditor();
    assertShadowRoot(editor.shadowRoot);
    const editables = editor.shadowRoot?.querySelectorAll('.editable');

    const element = editables[0] as HTMLElement;
    element.focus();
    assert.isTrue(isWholeElementContentSelected(element));
    element.blur();
    assert.isFalse(element.hasSelection());
  });

  it('moves focus to the next field when pressing \'Enter\'', async () => {
    const editor = await renderEditor();
    assertShadowRoot(editor.shadowRoot);
    const editables = editor.shadowRoot?.querySelectorAll('.editable');

    const first = editables[1] as HTMLSpanElement;
    const second = editables[2] as HTMLElement;
    assert.isFalse(first.hasSelection());
    assert.isFalse(second.hasSelection());

    first.focus();
    assert.isTrue(isWholeElementContentSelected(first));
    assert.isFalse(second.hasSelection());

    dispatchKeyDownEvent(first, {key: 'Enter', bubbles: true});
    assert.isFalse(first.hasSelection());
    assert.isTrue(isWholeElementContentSelected(second));
  });

  it('allows adding headers', async () => {
    const editor = await renderEditorWithinWrapper();
    await coordinator.done();
    assertShadowRoot(editor.shadowRoot);

    let rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*',
      'server:DevTools Unit Test Server',
      'access-control-allow-origin:*',
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
    ]);

    await pressButton(editor.shadowRoot, 1, '.add-header');

    rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*',
      'server:DevTools Unit Test Server',
      'headerName1:headerValue',
      'access-control-allow-origin:*',
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
    ]);

    const editables = editor.shadowRoot?.querySelectorAll('.editable');
    await changeEditable(editables[3] as HTMLElement, 'cache-control');
    await changeEditable(editables[4] as HTMLElement, 'max-age=1000');

    rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*',
      'server:DevTools Unit Test Server',
      'cache-control:max-age=1000',
      'access-control-allow-origin:*',
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
    ]);
  });

  it('allows adding "ApplyTo"-blocks', async () => {
    const editor = await renderEditorWithinWrapper();
    await coordinator.done();
    assertShadowRoot(editor.shadowRoot);

    let rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*',
      'server:DevTools Unit Test Server',
      'access-control-allow-origin:*',
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
    ]);

    const button = editor.shadowRoot.querySelector('.add-block');
    assertElement(button, HTMLElement);
    button.click();
    await coordinator.done();

    rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*',
      'server:DevTools Unit Test Server',
      'access-control-allow-origin:*',
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
      'Apply to:*',
      'headerName:headerValue',
    ]);

    const editables = editor.shadowRoot?.querySelectorAll('.editable');
    await changeEditable(editables[8] as HTMLElement, 'articles/*');
    await changeEditable(editables[9] as HTMLElement, 'cache-control');
    await changeEditable(editables[10] as HTMLElement, 'max-age=1000');

    rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*',
      'server:DevTools Unit Test Server',
      'access-control-allow-origin:*',
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
      'Apply to:articles/*',
      'cache-control:max-age=1000',
    ]);
  });

  it('allows removing headers', async () => {
    const editor = await renderEditorWithinWrapper();
    await coordinator.done();
    assertShadowRoot(editor.shadowRoot);

    let rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*',
      'server:DevTools Unit Test Server',
      'access-control-allow-origin:*',
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
    ]);

    await pressButton(editor.shadowRoot, 1, '.remove-header');

    rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*',
      'access-control-allow-origin:*',
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
    ]);

    await pressButton(editor.shadowRoot, 1, '.remove-header');

    rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*',
      'headerName1:headerValue',
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
    ]);
  });

  it('allows removing "ApplyTo"-blocks', async () => {
    const editor = await renderEditorWithinWrapper();
    await coordinator.done();
    assertShadowRoot(editor.shadowRoot);

    let rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*',
      'server:DevTools Unit Test Server',
      'access-control-allow-origin:*',
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
    ]);

    await pressButton(editor.shadowRoot, 0, '.remove-block');

    rows = getRowContent(editor.shadowRoot);
    assert.deepEqual(rows, [
      'Apply to:*.jpg',
      'jpg-header:only for jpg files',
    ]);
  });
});
