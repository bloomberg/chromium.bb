// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as Common from '../../core/common/common.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as FormatterModule from '../../models/formatter/formatter.js';
import * as Persistence from '../../models/persistence/persistence.js';
import * as Workspace from '../../models/workspace/workspace.js';
import * as SourceFrame from '../../ui/legacy/components/source_frame/source_frame.js';
import * as UI from '../../ui/legacy/legacy.js';

import type {EditorAction, EditorClosedEvent, SourcesView} from './SourcesView.js';
import {Events, registerEditorAction} from './SourcesView.js';

const UIStrings = {
  /**
  *@description Title of the pretty print button in the Sources panel
  *@example {file name} PH1
  */
  prettyPrintS: 'Pretty print {PH1}',
  /**
  *@description Text to pretty print a file
  */
  prettyPrint: 'Pretty print',
};
const str_ = i18n.i18n.registerUIStrings('panels/sources/ScriptFormatterEditorAction.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

let scriptFormatterEditorActionInstance: ScriptFormatterEditorAction;

export class ScriptFormatterEditorAction implements EditorAction {
  private readonly pathsToFormatOnLoad: Set<string>;
  private sourcesView!: SourcesView;
  private button!: UI.Toolbar.ToolbarButton;
  private constructor() {
    this.pathsToFormatOnLoad = new Set();
  }

  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): ScriptFormatterEditorAction {
    const {forceNew} = opts;
    if (!scriptFormatterEditorActionInstance || forceNew) {
      scriptFormatterEditorActionInstance = new ScriptFormatterEditorAction();
    }

    return scriptFormatterEditorActionInstance;
  }

  private editorSelected(event: Common.EventTarget.EventTargetEvent<Workspace.UISourceCode.UISourceCode>): void {
    const uiSourceCode = event.data;
    this.updateButton(uiSourceCode);

    if (this.isFormattableScript(uiSourceCode) && this.pathsToFormatOnLoad.has(uiSourceCode.url()) &&
        !FormatterModule.SourceFormatter.SourceFormatter.instance().hasFormatted(uiSourceCode)) {
      void this.showFormatted(uiSourceCode);
    }
  }

  private async editorClosed(event: Common.EventTarget.EventTargetEvent<EditorClosedEvent>): Promise<void> {
    const {uiSourceCode, wasSelected} = event.data;

    if (wasSelected) {
      this.updateButton(null);
    }
    const original =
        await FormatterModule.SourceFormatter.SourceFormatter.instance().discardFormattedUISourceCode(uiSourceCode);
    if (original) {
      this.pathsToFormatOnLoad.delete(original.url());
    }
  }

  private updateButton(uiSourceCode: Workspace.UISourceCode.UISourceCode|null): void {
    const isFormattable = this.isFormattableScript(uiSourceCode);
    this.button.element.classList.toggle('hidden', !isFormattable);
    if (uiSourceCode) {
      // We always update the title of the button, even if the {uiSourceCode} is
      // not formattable, since we use the title (the aria-label actually) as a
      // signal for the E2E tests that the source code loading is done.
      this.button.setTitle(i18nString(UIStrings.prettyPrintS, {PH1: uiSourceCode.name()}));
    }
  }

  getOrCreateButton(sourcesView: SourcesView): UI.Toolbar.ToolbarButton {
    if (this.button) {
      return this.button;
    }

    this.sourcesView = sourcesView;
    this.sourcesView.addEventListener(Events.EditorSelected, event => {
      this.editorSelected(event);
    });
    this.sourcesView.addEventListener(Events.EditorClosed, event => {
      void this.editorClosed(event);
    });

    this.button = new UI.Toolbar.ToolbarButton(i18nString(UIStrings.prettyPrint), 'largeicon-pretty-print');
    this.button.addEventListener(UI.Toolbar.ToolbarButton.Events.Click, this.onFormatScriptButtonClicked, this);
    this.updateButton(sourcesView.currentUISourceCode());

    return this.button;
  }

  private isFormattableScript(uiSourceCode: Workspace.UISourceCode.UISourceCode|null): boolean {
    if (!uiSourceCode) {
      return false;
    }
    if (uiSourceCode.project().canSetFileContent()) {
      return false;
    }
    if (uiSourceCode.project().type() === Workspace.Workspace.projectTypes.Formatter) {
      return false;
    }
    if (Persistence.Persistence.PersistenceImpl.instance().binding(uiSourceCode)) {
      return false;
    }
    if (uiSourceCode.mimeType() === 'application/wasm') {
      return false;
    }
    return uiSourceCode.contentType().hasScripts();
  }

  isCurrentUISourceCodeFormattable(): boolean {
    const uiSourceCode = this.sourcesView.currentUISourceCode();
    return this.isFormattableScript(uiSourceCode);
  }

  private onFormatScriptButtonClicked(): void {
    this.toggleFormatScriptSource();
  }

  toggleFormatScriptSource(): void {
    const uiSourceCode = this.sourcesView.currentUISourceCode();
    if (!uiSourceCode || !this.isFormattableScript(uiSourceCode)) {
      return;
    }
    this.pathsToFormatOnLoad.add(uiSourceCode.url());
    void this.showFormatted(uiSourceCode);
  }

  private async showFormatted(uiSourceCode: Workspace.UISourceCode.UISourceCode): Promise<void> {
    const formatData = await FormatterModule.SourceFormatter.SourceFormatter.instance().format(uiSourceCode);
    if (uiSourceCode !== this.sourcesView.currentUISourceCode()) {
      return;
    }
    const sourceFrame = this.sourcesView.viewForFile(uiSourceCode);
    let start: number[]|number[] = [0, 0];
    if (sourceFrame instanceof SourceFrame.SourceFrame.SourceFrameImpl) {
      const selection = sourceFrame.textEditor.toLineColumn(sourceFrame.textEditor.state.selection.main.head);
      start = formatData.mapping.originalToFormatted(selection.lineNumber, selection.columnNumber);
    }
    this.sourcesView.showSourceLocation(formatData.formattedSourceCode, {lineNumber: start[0], columnNumber: start[1]});
  }
}

registerEditorAction(ScriptFormatterEditorAction.instance);
