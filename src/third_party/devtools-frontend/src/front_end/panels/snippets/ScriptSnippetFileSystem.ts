// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Platform from '../../core/platform/platform.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as Protocol from '../../generated/protocol.js';
import * as Persistence from '../../models/persistence/persistence.js';

import type * as TextUtils from '../../models/text_utils/text_utils.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as Workspace from '../../models/workspace/workspace.js';

const UIStrings = {
  /**
  *@description Default snippet name when a new snippet is created in the Sources panel
  *@example {1} PH1
  */
  scriptSnippet: 'Script snippet #{PH1}',
  /**
  *@description Text to show something is linked to another
  *@example {example.url} PH1
  */
  linkedTo: 'Linked to {PH1}',
};
const str_ = i18n.i18n.registerUIStrings('panels/snippets/ScriptSnippetFileSystem.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

function escapeSnippetName(name: string): string {
  return escape(name);
}

function unescapeSnippetName(name: string): string {
  return unescape(name);
}

export class SnippetFileSystem extends Persistence.PlatformFileSystem.PlatformFileSystem {
  private readonly lastSnippetIdentifierSetting: Common.Settings.Setting<number>;
  private readonly snippetsSetting: Common.Settings.Setting<Snippet[]>;
  constructor() {
    super('snippet://', 'snippets');
    this.lastSnippetIdentifierSetting =
        Common.Settings.Settings.instance().createSetting('scriptSnippets_lastIdentifier', 0);
    this.snippetsSetting = Common.Settings.Settings.instance().createSetting('scriptSnippets', []);
  }

  initialFilePaths(): string[] {
    const savedSnippets: Snippet[] = this.snippetsSetting.get();
    return savedSnippets.map(snippet => escapeSnippetName(snippet.name));
  }

  async createFile(_path: string, _name: string|null): Promise<string|null> {
    const nextId = this.lastSnippetIdentifierSetting.get() + 1;
    this.lastSnippetIdentifierSetting.set(nextId);

    const snippetName = i18nString(UIStrings.scriptSnippet, {PH1: nextId});
    const snippets = this.snippetsSetting.get();
    snippets.push({name: snippetName, content: ''});
    this.snippetsSetting.set(snippets);

    return escapeSnippetName(snippetName);
  }

  async deleteFile(path: string): Promise<boolean> {
    const name = unescapeSnippetName(path.substring(1));
    const allSnippets: Snippet[] = this.snippetsSetting.get();
    const snippets = allSnippets.filter(snippet => snippet.name !== name);
    if (allSnippets.length !== snippets.length) {
      this.snippetsSetting.set(snippets);
      return true;
    }
    return false;
  }

  async requestFileContent(path: string): Promise<TextUtils.ContentProvider.DeferredContent> {
    const name = unescapeSnippetName(path.substring(1));
    const snippets: Snippet[] = this.snippetsSetting.get();
    const snippet = snippets.find(snippet => snippet.name === name);
    if (snippet) {
      return {content: snippet.content, isEncoded: false};
    }
    return {content: null, isEncoded: false, error: `A snippet with name '${name}' was not found`};
  }

  async setFileContent(path: string, content: string, _isBase64: boolean): Promise<boolean> {
    const name = unescapeSnippetName(path.substring(1));
    const snippets: Snippet[] = this.snippetsSetting.get();
    const snippet = snippets.find(snippet => snippet.name === name);
    if (snippet) {
      snippet.content = content;
      this.snippetsSetting.set(snippets);
      return true;
    }
    return false;
  }

  renameFile(path: string, newName: string, callback: (arg0: boolean, arg1?: string|undefined) => void): void {
    const name = unescapeSnippetName(path.substring(1));
    const snippets: Snippet[] = this.snippetsSetting.get();
    const snippet = snippets.find(snippet => snippet.name === name);
    newName = newName.trim();
    if (!snippet || newName.length === 0 || snippets.find(snippet => snippet.name === newName)) {
      callback(false);
      return;
    }
    snippet.name = newName;
    this.snippetsSetting.set(snippets);
    callback(true, newName);
  }

  async searchInPath(query: string, _progress: Common.Progress.Progress): Promise<string[]> {
    const re = new RegExp(Platform.StringUtilities.escapeForRegExp(query), 'i');
    const allSnippets: Snippet[] = this.snippetsSetting.get();
    const matchedSnippets = allSnippets.filter(snippet => snippet.content.match(re));
    return matchedSnippets.map(snippet => `snippet:///${escapeSnippetName(snippet.name)}`);
  }

  mimeFromPath(_path: string): string {
    return 'text/javascript';
  }

  contentType(_path: string): Common.ResourceType.ResourceType {
    return Common.ResourceType.resourceTypes.Script;
  }

  tooltipForURL(url: string): string {
    return i18nString(UIStrings.linkedTo, {PH1: unescapeSnippetName(url.substring(this.path().length))});
  }

  supportsAutomapping(): boolean {
    return true;
  }
}

export async function evaluateScriptSnippet(uiSourceCode: Workspace.UISourceCode.UISourceCode): Promise<void> {
  if (!uiSourceCode.url().startsWith('snippet://')) {
    return;
  }

  const executionContext = UI.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext);
  if (!executionContext) {
    return;
  }

  const runtimeModel = executionContext.runtimeModel;
  await uiSourceCode.requestContent();
  uiSourceCode.commitWorkingCopy();
  const expression = uiSourceCode.workingCopy();
  Common.Console.Console.instance().show();

  const url = uiSourceCode.url();

  const result = await executionContext.evaluate(
      {
        expression: `${expression}\n//# sourceURL=${url}`,
        objectGroup: 'console',
        silent: false,
        includeCommandLineAPI: true,
        returnByValue: false,
        generatePreview: true,
        replMode: true,
      } as SDK.RuntimeModel.EvaluationOptions,
      false, true);

  if ('exceptionDetails' in result && result.exceptionDetails) {
    SDK.ConsoleModel.ConsoleModel.instance().addMessage(SDK.ConsoleModel.ConsoleMessage.fromException(
        runtimeModel, result.exceptionDetails, /* messageType */ undefined, /* timestamp */ undefined, url));
    return;
  }
  if (!('object' in result) || !result.object) {
    return;
  }

  const scripts = executionContext.debuggerModel.scriptsForSourceURL(url);
  if (scripts.length < 1) {
    return;
  }
  const scriptId = scripts[scripts.length - 1].scriptId;
  const details = {
    type: SDK.ConsoleModel.FrontendMessageType.Result,
    url,
    parameters: [result.object],
    executionContextId: executionContext.id,
    scriptId,
  };
  SDK.ConsoleModel.ConsoleModel.instance().addMessage(new SDK.ConsoleModel.ConsoleMessage(
      runtimeModel, Protocol.Log.LogEntrySource.Javascript, Protocol.Log.LogEntryLevel.Info, '', details));
}

export function isSnippetsUISourceCode(uiSourceCode: Workspace.UISourceCode.UISourceCode): boolean {
  return uiSourceCode.url().startsWith('snippet://');
}

export function isSnippetsProject(project: Workspace.Workspace.Project): boolean {
  return project.type() === Workspace.Workspace.projectTypes.FileSystem &&
      Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.fileSystemType(project) === 'snippets';
}

export function findSnippetsProject(): Workspace.Workspace.Project {
  const workspaceProject =
      Workspace.Workspace.WorkspaceImpl.instance()
          .projectsForType(Workspace.Workspace.projectTypes.FileSystem)
          .find(
              project => Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.fileSystemType(project) ===
                  'snippets');

  if (!workspaceProject) {
    throw new Error('Unable to find workspace project for the snippets file system');
  }

  return workspaceProject;
}
export interface Snippet {
  name: string;
  content: string;
}
