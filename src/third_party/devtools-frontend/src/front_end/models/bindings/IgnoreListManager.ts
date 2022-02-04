// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as Platform from '../../core/platform/platform.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as Workspace from '../workspace/workspace.js';

import type {DebuggerWorkspaceBinding} from './DebuggerWorkspaceBinding.js';

let ignoreListManagerInstance: IgnoreListManager;

export class IgnoreListManager implements SDK.TargetManager.SDKModelObserver<SDK.DebuggerModel.DebuggerModel> {
  readonly #debuggerWorkspaceBinding: DebuggerWorkspaceBinding;
  readonly #listeners: Set<() => void>;
  readonly #isIgnoreListedURLCache: Map<string, boolean>;

  private constructor(debuggerWorkspaceBinding: DebuggerWorkspaceBinding) {
    this.#debuggerWorkspaceBinding = debuggerWorkspaceBinding;

    SDK.TargetManager.TargetManager.instance().addModelListener(
        SDK.DebuggerModel.DebuggerModel, SDK.DebuggerModel.Events.GlobalObjectCleared,
        this.clearCacheIfNeeded.bind(this), this);
    Common.Settings.Settings.instance()
        .moduleSetting('skipStackFramesPattern')
        .addChangeListener(this.patternChanged.bind(this));
    Common.Settings.Settings.instance()
        .moduleSetting('skipContentScripts')
        .addChangeListener(this.patternChanged.bind(this));

    this.#listeners = new Set();

    this.#isIgnoreListedURLCache = new Map();

    SDK.TargetManager.TargetManager.instance().observeModels(SDK.DebuggerModel.DebuggerModel, this);
  }

  static instance(opts: {
    forceNew: boolean|null,
    debuggerWorkspaceBinding: DebuggerWorkspaceBinding|null,
  } = {forceNew: null, debuggerWorkspaceBinding: null}): IgnoreListManager {
    const {forceNew, debuggerWorkspaceBinding} = opts;
    if (!ignoreListManagerInstance || forceNew) {
      if (!debuggerWorkspaceBinding) {
        throw new Error(
            `Unable to create settings: targetManager, workspace, and debuggerWorkspaceBinding must be provided: ${
                new Error().stack}`);
      }

      ignoreListManagerInstance = new IgnoreListManager(debuggerWorkspaceBinding);
    }

    return ignoreListManagerInstance;
  }

  addChangeListener(listener: () => void): void {
    this.#listeners.add(listener);
  }

  removeChangeListener(listener: () => void): void {
    this.#listeners.delete(listener);
  }

  modelAdded(debuggerModel: SDK.DebuggerModel.DebuggerModel): void {
    void this.setIgnoreListPatterns(debuggerModel);
    const sourceMapManager = debuggerModel.sourceMapManager();
    sourceMapManager.addEventListener(SDK.SourceMapManager.Events.SourceMapAttached, this.sourceMapAttached, this);
    sourceMapManager.addEventListener(SDK.SourceMapManager.Events.SourceMapDetached, this.sourceMapDetached, this);
  }

  modelRemoved(debuggerModel: SDK.DebuggerModel.DebuggerModel): void {
    this.clearCacheIfNeeded();
    const sourceMapManager = debuggerModel.sourceMapManager();
    sourceMapManager.removeEventListener(SDK.SourceMapManager.Events.SourceMapAttached, this.sourceMapAttached, this);
    sourceMapManager.removeEventListener(SDK.SourceMapManager.Events.SourceMapDetached, this.sourceMapDetached, this);
  }

  private clearCacheIfNeeded(): void {
    if (this.#isIgnoreListedURLCache.size > 1024) {
      this.#isIgnoreListedURLCache.clear();
    }
  }

  private getSkipStackFramesPatternSetting(): Common.Settings.RegExpSetting {
    return Common.Settings.Settings.instance().moduleSetting('skipStackFramesPattern') as Common.Settings.RegExpSetting;
  }

  private setIgnoreListPatterns(debuggerModel: SDK.DebuggerModel.DebuggerModel): Promise<boolean> {
    const regexPatterns = this.getSkipStackFramesPatternSetting().getAsArray();
    const patterns = ([] as string[]);
    for (const item of regexPatterns) {
      if (!item.disabled && item.pattern) {
        patterns.push(item.pattern);
      }
    }
    return debuggerModel.setBlackboxPatterns(patterns);
  }

  isIgnoreListedUISourceCode(uiSourceCode: Workspace.UISourceCode.UISourceCode): boolean {
    const projectType = uiSourceCode.project().type();
    const isContentScript = projectType === Workspace.Workspace.projectTypes.ContentScripts;
    if (isContentScript && Common.Settings.Settings.instance().moduleSetting('skipContentScripts').get()) {
      return true;
    }
    const url = this.uiSourceCodeURL(uiSourceCode);
    return url ? this.isIgnoreListedURL(url) : false;
  }

  isIgnoreListedURL(url: string, isContentScript?: boolean): boolean {
    if (this.#isIgnoreListedURLCache.has(url)) {
      return Boolean(this.#isIgnoreListedURLCache.get(url));
    }
    if (isContentScript && Common.Settings.Settings.instance().moduleSetting('skipContentScripts').get()) {
      return true;
    }
    const regex = this.getSkipStackFramesPatternSetting().asRegExp();
    const isIgnoreListed = (regex && regex.test(url)) || false;
    this.#isIgnoreListedURLCache.set(url, isIgnoreListed);
    return isIgnoreListed;
  }

  private sourceMapAttached(
      event: Common.EventTarget.EventTargetEvent<{client: SDK.Script.Script, sourceMap: SDK.SourceMap.SourceMap}>):
      void {
    const script = event.data.client;
    const sourceMap = event.data.sourceMap;
    void this.updateScriptRanges(script, sourceMap);
  }

  private sourceMapDetached(
      event: Common.EventTarget.EventTargetEvent<{client: SDK.Script.Script, sourceMap: SDK.SourceMap.SourceMap}>):
      void {
    const script = event.data.client;
    void this.updateScriptRanges(script, null);
  }

  private async updateScriptRanges(script: SDK.Script.Script, sourceMap: SDK.SourceMap.SourceMap|null): Promise<void> {
    let hasIgnoreListedMappings = false;
    if (!IgnoreListManager.instance().isIgnoreListedURL(script.sourceURL, script.isContentScript())) {
      hasIgnoreListedMappings = sourceMap ? sourceMap.sourceURLs().some(url => this.isIgnoreListedURL(url)) : false;
    }
    if (!hasIgnoreListedMappings) {
      if (scriptToRange.get(script) && await script.setBlackboxedRanges([])) {
        scriptToRange.delete(script);
      }
      await this.#debuggerWorkspaceBinding.updateLocations(script);
      return;
    }

    if (!sourceMap) {
      return;
    }

    const mappings = sourceMap.mappings();
    const newRanges: SourceRange[] = [];
    if (mappings.length > 0) {
      let currentIgnoreListed = false;
      if (mappings[0].lineNumber !== 0 || mappings[0].columnNumber !== 0) {
        newRanges.push({lineNumber: 0, columnNumber: 0});
        currentIgnoreListed = true;
      }
      for (const mapping of mappings) {
        if (mapping.sourceURL && currentIgnoreListed !== this.isIgnoreListedURL(mapping.sourceURL)) {
          newRanges.push({lineNumber: mapping.lineNumber, columnNumber: mapping.columnNumber});
          currentIgnoreListed = !currentIgnoreListed;
        }
      }
    }

    const oldRanges = scriptToRange.get(script) || [];
    if (!isEqual(oldRanges, newRanges) && await script.setBlackboxedRanges(newRanges)) {
      scriptToRange.set(script, newRanges);
    }
    void this.#debuggerWorkspaceBinding.updateLocations(script);

    function isEqual(rangesA: SourceRange[], rangesB: SourceRange[]): boolean {
      if (rangesA.length !== rangesB.length) {
        return false;
      }
      for (let i = 0; i < rangesA.length; ++i) {
        if (rangesA[i].lineNumber !== rangesB[i].lineNumber || rangesA[i].columnNumber !== rangesB[i].columnNumber) {
          return false;
        }
      }
      return true;
    }
  }

  private uiSourceCodeURL(uiSourceCode: Workspace.UISourceCode.UISourceCode): string|null {
    return uiSourceCode.project().type() === Workspace.Workspace.projectTypes.Debugger ? null : uiSourceCode.url();
  }

  canIgnoreListUISourceCode(uiSourceCode: Workspace.UISourceCode.UISourceCode): boolean {
    const url = this.uiSourceCodeURL(uiSourceCode);
    return url ? Boolean(this.urlToRegExpString(url)) : false;
  }

  ignoreListUISourceCode(uiSourceCode: Workspace.UISourceCode.UISourceCode): void {
    const url = this.uiSourceCodeURL(uiSourceCode);
    if (url) {
      this.ignoreListURL(url);
    }
  }

  unIgnoreListUISourceCode(uiSourceCode: Workspace.UISourceCode.UISourceCode): void {
    const url = this.uiSourceCodeURL(uiSourceCode);
    if (url) {
      this.unIgnoreListURL(url);
    }
  }

  ignoreListContentScripts(): void {
    Common.Settings.Settings.instance().moduleSetting('skipContentScripts').set(true);
  }

  unIgnoreListContentScripts(): void {
    Common.Settings.Settings.instance().moduleSetting('skipContentScripts').set(false);
  }

  private ignoreListURL(url: string): void {
    const regexPatterns = this.getSkipStackFramesPatternSetting().getAsArray();
    const regexValue = this.urlToRegExpString(url);
    if (!regexValue) {
      return;
    }
    let found = false;
    for (let i = 0; i < regexPatterns.length; ++i) {
      const item = regexPatterns[i];
      if (item.pattern === regexValue) {
        item.disabled = false;
        found = true;
        break;
      }
    }
    if (!found) {
      regexPatterns.push({pattern: regexValue, disabled: undefined});
    }
    this.getSkipStackFramesPatternSetting().setAsArray(regexPatterns);
  }

  private unIgnoreListURL(url: string): void {
    let regexPatterns = this.getSkipStackFramesPatternSetting().getAsArray();
    const regexValue = IgnoreListManager.instance().urlToRegExpString(url);
    if (!regexValue) {
      return;
    }
    regexPatterns = regexPatterns.filter(function(item) {
      return item.pattern !== regexValue;
    });
    for (let i = 0; i < regexPatterns.length; ++i) {
      const item = regexPatterns[i];
      if (item.disabled) {
        continue;
      }
      try {
        const regex = new RegExp(item.pattern);
        if (regex.test(url)) {
          item.disabled = true;
        }
      } catch (e) {
      }
    }
    this.getSkipStackFramesPatternSetting().setAsArray(regexPatterns);
  }

  private async patternChanged(): Promise<void> {
    this.#isIgnoreListedURLCache.clear();

    const promises: Promise<unknown>[] = [];
    for (const debuggerModel of SDK.TargetManager.TargetManager.instance().models(SDK.DebuggerModel.DebuggerModel)) {
      promises.push(this.setIgnoreListPatterns(debuggerModel));
      const sourceMapManager = debuggerModel.sourceMapManager();
      for (const script of debuggerModel.scripts()) {
        promises.push(this.updateScriptRanges(script, sourceMapManager.sourceMapForClient(script)));
      }
    }
    await Promise.all(promises);
    const listeners = Array.from(this.#listeners);
    for (const listener of listeners) {
      listener();
    }
    this.patternChangeFinishedForTests();
  }

  private patternChangeFinishedForTests(): void {
    // This method is sniffed in tests.
  }

  private urlToRegExpString(url: string): string {
    const parsedURL = new Common.ParsedURL.ParsedURL(url);
    if (parsedURL.isAboutBlank() || parsedURL.isDataURL()) {
      return '';
    }
    if (!parsedURL.isValid) {
      return '^' + Platform.StringUtilities.escapeForRegExp(url) + '$';
    }
    let name: string = parsedURL.lastPathComponent;
    if (name) {
      name = '/' + name;
    } else if (parsedURL.folderPathComponents) {
      name = parsedURL.folderPathComponents + '/';
    }
    if (!name) {
      name = parsedURL.host;
    }
    if (!name) {
      return '';
    }
    const scheme = parsedURL.scheme;
    let prefix = '';
    if (scheme && scheme !== 'http' && scheme !== 'https') {
      prefix = '^' + scheme + '://';
      if (scheme === 'chrome-extension') {
        prefix += parsedURL.host + '\\b';
      }
      prefix += '.*';
    }
    return prefix + Platform.StringUtilities.escapeForRegExp(name) + (url.endsWith(name) ? '$' : '\\b');
  }
}
export interface SourceRange {
  lineNumber: number;
  columnNumber: number;
}

const scriptToRange = new WeakMap<SDK.Script.Script, SourceRange[]>();
