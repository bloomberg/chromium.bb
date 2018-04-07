// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @unrestricted
 * @implements {SDK.SDKModelObserver<!SDK.DebuggerModel>}
 */
Bindings.BlackboxManager = class {
  /**
   * @param {!Bindings.DebuggerWorkspaceBinding} debuggerWorkspaceBinding
   */
  constructor(debuggerWorkspaceBinding) {
    this._debuggerWorkspaceBinding = debuggerWorkspaceBinding;

    SDK.targetManager.addModelListener(
        SDK.DebuggerModel, SDK.DebuggerModel.Events.ParsedScriptSource, this._parsedScriptSource, this);
    SDK.targetManager.addModelListener(
        SDK.DebuggerModel, SDK.DebuggerModel.Events.GlobalObjectCleared, this._globalObjectCleared, this);
    Common.moduleSetting('skipStackFramesPattern').addChangeListener(this._patternChanged.bind(this));
    Common.moduleSetting('skipContentScripts').addChangeListener(this._patternChanged.bind(this));

    /** @type {!Set<function()>} */
    this._listeners = new Set();

    /** @type {!Map<!SDK.DebuggerModel, !Map<string, !Array<!Protocol.Debugger.ScriptPosition>>>} */
    this._debuggerModelData = new Map();
    /** @type {!Map<string, boolean>} */
    this._isBlackboxedURLCache = new Map();

    SDK.targetManager.observeModels(SDK.DebuggerModel, this);
  }

  /**
   * @param {function()} listener
   */
  addChangeListener(listener) {
    this._listeners.add(listener);
  }

  /**
   * @param {function()} listener
   */
  removeChangeListener(listener) {
    this._listeners.delete(listener);
  }

  /**
   * @override
   * @param {!SDK.DebuggerModel} debuggerModel
   */
  modelAdded(debuggerModel) {
    this._setBlackboxPatterns(debuggerModel);
  }

  /**
   * @override
   * @param {!SDK.DebuggerModel} debuggerModel
   */
  modelRemoved(debuggerModel) {
    this._debuggerModelData.delete(debuggerModel);
    this._isBlackboxedURLCache.clear();
  }

  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @return {!Promise<boolean>}
   */
  _setBlackboxPatterns(debuggerModel) {
    const regexPatterns = Common.moduleSetting('skipStackFramesPattern').getAsArray();
    const patterns = /** @type {!Array<string>} */ ([]);
    for (const item of regexPatterns) {
      if (!item.disabled && item.pattern)
        patterns.push(item.pattern);
    }
    return debuggerModel.setBlackboxPatterns(patterns);
  }

  /**
   * @param {!SDK.DebuggerModel.Location} location
   * @return {boolean}
   */
  isBlackboxedRawLocation(location) {
    const script = location.script();
    if (!script)
      return false;
    const positions = this._scriptPositions(script);
    if (!positions)
      return this._isBlackboxedScript(script);
    const index = positions.lowerBound(location, comparator);
    return !!(index % 2);

    /**
     * @param {!SDK.DebuggerModel.Location} a
     * @param {!Protocol.Debugger.ScriptPosition} b
     * @return {number}
     */
    function comparator(a, b) {
      if (a.lineNumber !== b.lineNumber)
        return a.lineNumber - b.lineNumber;
      return a.columnNumber - b.columnNumber;
    }
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @return {boolean}
   */
  isBlackboxedUISourceCode(uiSourceCode) {
    const projectType = uiSourceCode.project().type();
    const isContentScript = projectType === Workspace.projectTypes.ContentScripts;
    if (isContentScript && Common.moduleSetting('skipContentScripts').get())
      return true;
    const url = this._uiSourceCodeURL(uiSourceCode);
    return url ? this.isBlackboxedURL(url) : false;
  }

  /**
   * @param {string} url
   * @param {boolean=} isContentScript
   * @return {boolean}
   */
  isBlackboxedURL(url, isContentScript) {
    if (this._isBlackboxedURLCache.has(url))
      return !!this._isBlackboxedURLCache.get(url);
    if (isContentScript && Common.moduleSetting('skipContentScripts').get())
      return true;
    const regex = Common.moduleSetting('skipStackFramesPattern').asRegExp();
    const isBlackboxed = regex && regex.test(url);
    this._isBlackboxedURLCache.set(url, isBlackboxed);
    return isBlackboxed;
  }

  /**
   * @param {!SDK.Script} script
   * @param {?SDK.SourceMap} sourceMap
   * @return {!Promise<undefined>}
   */
  sourceMapLoaded(script, sourceMap) {
    if (!sourceMap)
      return Promise.resolve();
    const previousScriptState = this._scriptPositions(script);
    if (!previousScriptState)
      return Promise.resolve();

    const hasBlackboxedMappings = sourceMap.sourceURLs().some(url => this.isBlackboxedURL(url));
    const mappings = hasBlackboxedMappings ? sourceMap.mappings().slice() : [];
    if (!mappings.length) {
      if (previousScriptState.length > 0)
        return this._setScriptState(script, []);
      return Promise.resolve();
    }
    mappings.sort(mappingComparator);

    let currentBlackboxed = false;
    let isBlackboxed = false;
    const positions = [];
    // If content in script file begin is not mapped and one or more ranges are blackboxed then blackbox it.
    if (mappings[0].lineNumber !== 0 || mappings[0].columnNumber !== 0) {
      positions.push({lineNumber: 0, columnNumber: 0});
      currentBlackboxed = true;
    }
    for (const mapping of mappings) {
      if (mapping.sourceURL && currentBlackboxed !== this.isBlackboxedURL(mapping.sourceURL)) {
        positions.push({lineNumber: mapping.lineNumber, columnNumber: mapping.columnNumber});
        currentBlackboxed = !currentBlackboxed;
      }
      isBlackboxed = currentBlackboxed || isBlackboxed;
    }
    return this._setScriptState(script, !isBlackboxed ? [] : positions);
    /**
     * @param {!SDK.SourceMapEntry} a
     * @param {!SDK.SourceMapEntry} b
     * @return {number}
     */
    function mappingComparator(a, b) {
      if (a.lineNumber !== b.lineNumber)
        return a.lineNumber - b.lineNumber;
      return a.columnNumber - b.columnNumber;
    }
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @return {?string}
   */
  _uiSourceCodeURL(uiSourceCode) {
    return uiSourceCode.project().type() === Workspace.projectTypes.Debugger ? null : uiSourceCode.url();
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @return {boolean}
   */
  canBlackboxUISourceCode(uiSourceCode) {
    const url = this._uiSourceCodeURL(uiSourceCode);
    return url ? !!this._urlToRegExpString(url) : false;
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   */
  blackboxUISourceCode(uiSourceCode) {
    const url = this._uiSourceCodeURL(uiSourceCode);
    if (url)
      this._blackboxURL(url);
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   */
  unblackboxUISourceCode(uiSourceCode) {
    const url = this._uiSourceCodeURL(uiSourceCode);
    if (url)
      this._unblackboxURL(url);
  }

  blackboxContentScripts() {
    Common.moduleSetting('skipContentScripts').set(true);
  }

  unblackboxContentScripts() {
    Common.moduleSetting('skipContentScripts').set(false);
  }

  /**
   * @param {string} url
   */
  _blackboxURL(url) {
    const regexPatterns = Common.moduleSetting('skipStackFramesPattern').getAsArray();
    const regexValue = this._urlToRegExpString(url);
    if (!regexValue)
      return;
    let found = false;
    for (let i = 0; i < regexPatterns.length; ++i) {
      const item = regexPatterns[i];
      if (item.pattern === regexValue) {
        item.disabled = false;
        found = true;
        break;
      }
    }
    if (!found)
      regexPatterns.push({pattern: regexValue});
    Common.moduleSetting('skipStackFramesPattern').setAsArray(regexPatterns);
  }

  /**
   * @param {string} url
   */
  _unblackboxURL(url) {
    let regexPatterns = Common.moduleSetting('skipStackFramesPattern').getAsArray();
    const regexValue = Bindings.blackboxManager._urlToRegExpString(url);
    if (!regexValue)
      return;
    regexPatterns = regexPatterns.filter(function(item) {
      return item.pattern !== regexValue;
    });
    for (let i = 0; i < regexPatterns.length; ++i) {
      const item = regexPatterns[i];
      if (item.disabled)
        continue;
      try {
        const regex = new RegExp(item.pattern);
        if (regex.test(url))
          item.disabled = true;
      } catch (e) {
      }
    }
    Common.moduleSetting('skipStackFramesPattern').setAsArray(regexPatterns);
  }

  _patternChanged() {
    this._isBlackboxedURLCache.clear();

    /** @type {!Array<!Promise>} */
    const promises = [];
    for (const debuggerModel of SDK.targetManager.models(SDK.DebuggerModel)) {
      promises.push(this._setBlackboxPatterns(debuggerModel));
      for (const script of debuggerModel.scripts())
        promises.push(this._addScript(script).then(loadSourceMap.bind(this, script)));
    }
    Promise.all(promises).then(() => {
      const listeners = Array.from(this._listeners);
      for (const listener of listeners)
        listener();
      this._patternChangeFinishedForTests();
    });

    /**
     * @param {!SDK.Script} script
     * @return {!Promise<undefined>}
     * @this {Bindings.BlackboxManager}
     */
    function loadSourceMap(script) {
      return this.sourceMapLoaded(script, this._debuggerWorkspaceBinding.sourceMapForScript(script));
    }
  }

  _patternChangeFinishedForTests() {
    // This method is sniffed in tests.
  }

  /**
   * @param {!Common.Event} event
   */
  _globalObjectCleared(event) {
    const debuggerModel = /** @type {!SDK.DebuggerModel} */ (event.data);
    this._debuggerModelData.delete(debuggerModel);
    this._isBlackboxedURLCache.clear();
  }

  /**
   * @param {!Common.Event} event
   */
  _parsedScriptSource(event) {
    const script = /** @type {!SDK.Script} */ (event.data);
    this._addScript(script);
  }

  /**
   * @param {!SDK.Script} script
   * @return {!Promise<undefined>}
   */
  _addScript(script) {
    if (!script.sourceURL && !script.sourceMapURL)
      return Promise.resolve();
    const blackboxed = this._isBlackboxedScript(script);
    return this._setScriptState(script, blackboxed ? [{lineNumber: 0, columnNumber: 0}] : []);
  }

  /**
   * @param {!SDK.Script} script
   * @return {boolean}
   */
  _isBlackboxedScript(script) {
    return this.isBlackboxedURL(script.sourceURL, script.isContentScript());
  }

  /**
   * @param {!SDK.Script} script
   * @return {?Array<!Protocol.Debugger.ScriptPosition>}
   */
  _scriptPositions(script) {
    if (this._debuggerModelData.has(script.debuggerModel))
      return this._debuggerModelData.get(script.debuggerModel).get(script.scriptId) || null;
    return null;
  }

  /**
   * @param {!SDK.Script} script
   * @param {!Array<!Protocol.Debugger.ScriptPosition>} positions
   */
  _setScriptPositions(script, positions) {
    const debuggerModel = script.debuggerModel;
    if (!this._debuggerModelData.has(debuggerModel))
      this._debuggerModelData.set(debuggerModel, new Map());
    this._debuggerModelData.get(debuggerModel).set(script.scriptId, positions);
  }

  /**
   * @param {!SDK.Script} script
   * @param {!Array<!Protocol.Debugger.ScriptPosition>} positions
   * @return {!Promise<undefined>}
   */
  _setScriptState(script, positions) {
    const previousScriptState = this._scriptPositions(script);
    if (previousScriptState) {
      let hasChanged = false;
      hasChanged = previousScriptState.length !== positions.length;
      for (let i = 0; !hasChanged && i < positions.length; ++i) {
        hasChanged = positions[i].lineNumber !== previousScriptState[i].lineNumber ||
            positions[i].columnNumber !== previousScriptState[i].columnNumber;
      }
      if (!hasChanged)
        return Promise.resolve();
    } else {
      if (positions.length === 0)
        return Promise.resolve().then(updateState.bind(this, false));
    }

    return script.setBlackboxedRanges(positions).then(updateState.bind(this));

    /**
     * @param {boolean} success
     * @this {Bindings.BlackboxManager}
     */
    function updateState(success) {
      if (success) {
        this._setScriptPositions(script, positions);
        this._debuggerWorkspaceBinding.updateLocations(script);
        const isBlackboxed = positions.length !== 0;
        if (!isBlackboxed && script.sourceMapURL)
          this._debuggerWorkspaceBinding.maybeLoadSourceMap(script);
      } else {
        const hasPositions = !!this._scriptPositions(script);
        if (!hasPositions)
          this._setScriptPositions(script, []);
      }
    }
  }

  /**
   * @param {string} url
   * @return {string}
   */
  _urlToRegExpString(url) {
    const parsedURL = new Common.ParsedURL(url);
    if (parsedURL.isAboutBlank() || parsedURL.isDataURL())
      return '';
    if (!parsedURL.isValid)
      return '^' + url.escapeForRegExp() + '$';
    let name = parsedURL.lastPathComponent;
    if (name)
      name = '/' + name;
    else if (parsedURL.folderPathComponents)
      name = parsedURL.folderPathComponents + '/';
    if (!name)
      name = parsedURL.host;
    if (!name)
      return '';
    const scheme = parsedURL.scheme;
    let prefix = '';
    if (scheme && scheme !== 'http' && scheme !== 'https') {
      prefix = '^' + scheme + '://';
      if (scheme === 'chrome-extension')
        prefix += parsedURL.host + '\\b';
      prefix += '.*';
    }
    return prefix + name.escapeForRegExp() + (url.endsWith(name) ? '$' : '\\b');
  }
};

/** @type {!Bindings.BlackboxManager} */
Bindings.blackboxManager;
