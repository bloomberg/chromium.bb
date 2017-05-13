/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @unrestricted
 */
SDK.DebuggerModel = class extends SDK.SDKModel {
  /**
   * @param {!SDK.Target} target
   */
  constructor(target) {
    super(target);

    target.registerDebuggerDispatcher(new SDK.DebuggerDispatcher(this));
    this._agent = target.debuggerAgent();
    this._runtimeModel = /** @type {!SDK.RuntimeModel} */ (target.model(SDK.RuntimeModel));

    /** @type {!SDK.SourceMapManager<!SDK.Script>} */
    this._sourceMapManager = new SDK.SourceMapManager(target);
    /** @type {!Map<string, !SDK.Script>} */
    this._sourceMapIdToScript = new Map();

    /** @type {?SDK.DebuggerPausedDetails} */
    this._debuggerPausedDetails = null;
    /** @type {!Map<string, !SDK.Script>} */
    this._scripts = new Map();
    /** @type {!Map.<string, !Array.<!SDK.Script>>} */
    this._scriptsBySourceURL = new Map();
    /** @type {!Array.<!SDK.Script>} */
    this._discardableScripts = [];

    /** @type {!Common.Object} */
    this._breakpointResolvedEventTarget = new Common.Object();

    this._isPausing = false;
    Common.moduleSetting('pauseOnExceptionEnabled').addChangeListener(this._pauseOnExceptionStateChanged, this);
    Common.moduleSetting('pauseOnCaughtException').addChangeListener(this._pauseOnExceptionStateChanged, this);
    Common.moduleSetting('disableAsyncStackTraces').addChangeListener(this._asyncStackTracesStateChanged, this);

    /** @type {!Map<string, string>} */
    this._fileURLToNodeJSPath = new Map();
    this._enableDebugger();

    /** @type {!Map<string, string>} */
    this._stringMap = new Map();
    this._sourceMapManager.setEnabled(Common.moduleSetting('jsSourceMapsEnabled').get());
    Common.moduleSetting('jsSourceMapsEnabled')
        .addChangeListener(event => this._sourceMapManager.setEnabled(/** @type {boolean} */ (event.data)));
  }

  /**
   * @param {string} executionContextId
   * @param {string} sourceURL
   * @param {?string} sourceMapURL
   * @return {?string}
   */
  static _sourceMapId(executionContextId, sourceURL, sourceMapURL) {
    if (!sourceMapURL)
      return null;
    return executionContextId + ':' + sourceURL + ':' + sourceMapURL;
  }

  /**
   * @return {!SDK.SourceMapManager<!SDK.Script>}
   */
  sourceMapManager() {
    return this._sourceMapManager;
  }

  /**
   * @return {!SDK.RuntimeModel}
   */
  runtimeModel() {
    return this._runtimeModel;
  }

  /**
   * @return {boolean}
   */
  debuggerEnabled() {
    return !!this._debuggerEnabled;
  }

  /**
   * @return {!Promise}
   */
  _enableDebugger() {
    if (this._debuggerEnabled)
      return Promise.resolve();
    this._debuggerEnabled = true;

    var enablePromise = new Promise(fulfill => this._agent.enable(fulfill));
    this._pauseOnExceptionStateChanged();
    this._asyncStackTracesStateChanged();
    this.dispatchEventToListeners(SDK.DebuggerModel.Events.DebuggerWasEnabled, this);
    return enablePromise;
  }

  /**
   * @return {!Promise}
   */
  _disableDebugger() {
    if (!this._debuggerEnabled)
      return Promise.resolve();
    this._debuggerEnabled = false;

    var disablePromise = new Promise(fulfill => this._agent.disable(fulfill));
    this._isPausing = false;
    this._asyncStackTracesStateChanged();
    this.globalObjectCleared();
    this.dispatchEventToListeners(SDK.DebuggerModel.Events.DebuggerWasDisabled);
    return disablePromise;
  }

  /**
   * @param {boolean} skip
   */
  _skipAllPauses(skip) {
    if (this._skipAllPausesTimeout) {
      clearTimeout(this._skipAllPausesTimeout);
      delete this._skipAllPausesTimeout;
    }
    this._agent.setSkipAllPauses(skip);
  }

  /**
   * @param {number} timeout
   */
  skipAllPausesUntilReloadOrTimeout(timeout) {
    if (this._skipAllPausesTimeout)
      clearTimeout(this._skipAllPausesTimeout);
    this._agent.setSkipAllPauses(true);
    // If reload happens before the timeout, the flag will be already unset and the timeout callback won't change anything.
    this._skipAllPausesTimeout = setTimeout(this._skipAllPauses.bind(this, false), timeout);
  }

  _pauseOnExceptionStateChanged() {
    var state;
    if (!Common.moduleSetting('pauseOnExceptionEnabled').get())
      state = SDK.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions;
    else if (Common.moduleSetting('pauseOnCaughtException').get())
      state = SDK.DebuggerModel.PauseOnExceptionsState.PauseOnAllExceptions;
    else
      state = SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions;

    this._agent.setPauseOnExceptions(state);
  }

  _asyncStackTracesStateChanged() {
    const maxAsyncStackChainDepth = 32;
    var enabled = !Common.moduleSetting('disableAsyncStackTraces').get() && this._debuggerEnabled;
    this._agent.setAsyncCallStackDepth(enabled ? maxAsyncStackChainDepth : 0);
  }

  stepInto() {
    this._agent.stepInto();
  }

  stepOver() {
    this._agent.stepOver();
  }

  stepOut() {
    this._agent.stepOut();
  }

  scheduleStepIntoAsync() {
    this._agent.scheduleStepIntoAsync();
  }

  resume() {
    this._agent.resume();
    this._isPausing = false;
  }

  pause() {
    this._isPausing = true;
    this._skipAllPauses(false);
    this._agent.pause();
  }

  /**
   * @param {boolean} active
   */
  setBreakpointsActive(active) {
    this._agent.setBreakpointsActive(active);
  }

  /**
   * @param {string} url
   * @param {number} lineNumber
   * @param {number=} columnNumber
   * @param {string=} condition
   * @param {function(?Protocol.Debugger.BreakpointId, !Array.<!SDK.DebuggerModel.Location>)=} callback
   */
  setBreakpointByURL(url, lineNumber, columnNumber, condition, callback) {
    // Convert file url to node-js path.
    if (this.target().isNodeJS() && this._fileURLToNodeJSPath.has(url))
      url = this._fileURLToNodeJSPath.get(url);
    // Adjust column if needed.
    var minColumnNumber = 0;
    var scripts = this._scriptsBySourceURL.get(url) || [];
    for (var i = 0, l = scripts.length; i < l; ++i) {
      var script = scripts[i];
      if (lineNumber === script.lineOffset)
        minColumnNumber = minColumnNumber ? Math.min(minColumnNumber, script.columnOffset) : script.columnOffset;
    }
    columnNumber = Math.max(columnNumber, minColumnNumber);

    /**
     * @param {?Protocol.Error} error
     * @param {!Protocol.Debugger.BreakpointId} breakpointId
     * @param {!Array.<!Protocol.Debugger.Location>} locations
     * @this {SDK.DebuggerModel}
     */
    function didSetBreakpoint(error, breakpointId, locations) {
      if (callback) {
        var rawLocations = locations ?
            locations.map(SDK.DebuggerModel.Location.fromPayload.bind(SDK.DebuggerModel.Location, this)) :
            [];
        callback(error ? null : breakpointId, rawLocations);
      }
    }
    this._agent.setBreakpointByUrl(lineNumber, url, undefined, columnNumber, condition, didSetBreakpoint.bind(this));
  }

  /**
   * @param {!SDK.DebuggerModel.Location} rawLocation
   * @param {string} condition
   * @param {function(?Protocol.Debugger.BreakpointId, !Array.<!SDK.DebuggerModel.Location>)=} callback
   */
  setBreakpointBySourceId(rawLocation, condition, callback) {
    /**
     * @this {SDK.DebuggerModel}
     * @param {?Protocol.Error} error
     * @param {!Protocol.Debugger.BreakpointId} breakpointId
     * @param {!Protocol.Debugger.Location} actualLocation
     */
    function didSetBreakpoint(error, breakpointId, actualLocation) {
      if (callback) {
        if (error || !actualLocation) {
          callback(null, []);
          return;
        }
        callback(breakpointId, [SDK.DebuggerModel.Location.fromPayload(this, actualLocation)]);
      }
    }
    this._agent.setBreakpoint(rawLocation.payload(), condition, didSetBreakpoint.bind(this));
  }

  /**
   * @param {!Protocol.Debugger.BreakpointId} breakpointId
   * @param {function()=} callback
   */
  removeBreakpoint(breakpointId, callback) {
    this._agent.removeBreakpoint(breakpointId, innerCallback);

    /**
     * @param {?Protocol.Error} error
     */
    function innerCallback(error) {
      if (error)
        console.error('Failed to remove breakpoint: ' + error);
      if (callback)
        callback();
    }
  }

  /**
   * @param {!SDK.DebuggerModel.Location} startLocation
   * @param {!SDK.DebuggerModel.Location} endLocation
   * @param {boolean} restrictToFunction
   * @return {!Promise<!Array<!SDK.DebuggerModel.BreakLocation>>}
   */
  async getPossibleBreakpoints(startLocation, endLocation, restrictToFunction) {
    var response = await this._agent.invoke_getPossibleBreakpoints(
        {start: startLocation.payload(), end: endLocation.payload(), restrictToFunction: restrictToFunction});
    if (response[Protocol.Error] || !response.locations)
      return [];
    return response.locations.map(location => SDK.DebuggerModel.BreakLocation.fromPayload(this, location));
  }

  /**
   * @param {!Protocol.Debugger.BreakpointId} breakpointId
   * @param {!Protocol.Debugger.Location} location
   */
  _breakpointResolved(breakpointId, location) {
    this._breakpointResolvedEventTarget.dispatchEventToListeners(
        breakpointId, SDK.DebuggerModel.Location.fromPayload(this, location));
  }

  globalObjectCleared() {
    this._setDebuggerPausedDetails(null);
    this._reset();
    // TODO(dgozman): move clients to ExecutionContextDestroyed/ScriptCollected events.
    this.dispatchEventToListeners(SDK.DebuggerModel.Events.GlobalObjectCleared, this);
  }

  _reset() {
    for (var scriptWithSourceMap of this._sourceMapIdToScript.values())
      this._sourceMapManager.detachSourceMap(scriptWithSourceMap);
    this._sourceMapIdToScript.clear();

    this._scripts.clear();
    this._scriptsBySourceURL.clear();
    this._stringMap.clear();
    this._discardableScripts = [];
  }

  /**
   * @return {!Array<!SDK.Script>}
   */
  scripts() {
    return Array.from(this._scripts.values());
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @return {?SDK.Script}
   */
  scriptForId(scriptId) {
    return this._scripts.get(scriptId) || null;
  }

  /**
   * @return {!Array.<!SDK.Script>}
   */
  scriptsForSourceURL(sourceURL) {
    if (!sourceURL)
      return [];
    return this._scriptsBySourceURL.get(sourceURL) || [];
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   * @return {!Array<!SDK.Script>}
   */
  scriptsForExecutionContext(executionContext) {
    var result = [];
    for (var script of this._scripts.values()) {
      if (script.executionContextId === executionContext.id)
        result.push(script);
    }
    return result;
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {string} newSource
   * @param {function(?Protocol.Error, !Protocol.Runtime.ExceptionDetails=)} callback
   */
  setScriptSource(scriptId, newSource, callback) {
    this._scripts.get(scriptId).editSource(
        newSource, this._didEditScriptSource.bind(this, scriptId, newSource, callback));
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {string} newSource
   * @param {function(?Protocol.Error, !Protocol.Runtime.ExceptionDetails=)} callback
   * @param {?Protocol.Error} error
   * @param {!Protocol.Runtime.ExceptionDetails=} exceptionDetails
   * @param {!Array.<!Protocol.Debugger.CallFrame>=} callFrames
   * @param {!Protocol.Runtime.StackTrace=} asyncStackTrace
   * @param {boolean=} needsStepIn
   */
  _didEditScriptSource(
      scriptId,
      newSource,
      callback,
      error,
      exceptionDetails,
      callFrames,
      asyncStackTrace,
      needsStepIn) {
    callback(error, exceptionDetails);
    if (needsStepIn) {
      this.stepInto();
      return;
    }

    if (!error && callFrames && callFrames.length) {
      this._pausedScript(
          callFrames, this._debuggerPausedDetails.reason, this._debuggerPausedDetails.auxData,
          this._debuggerPausedDetails.breakpointIds, asyncStackTrace);
    }
  }

  /**
   * @return {?Array.<!SDK.DebuggerModel.CallFrame>}
   */
  get callFrames() {
    return this._debuggerPausedDetails ? this._debuggerPausedDetails.callFrames : null;
  }

  /**
   * @return {?SDK.DebuggerPausedDetails}
   */
  debuggerPausedDetails() {
    return this._debuggerPausedDetails;
  }

  /**
   * @param {?SDK.DebuggerPausedDetails} debuggerPausedDetails
   * @return {boolean}
   */
  _setDebuggerPausedDetails(debuggerPausedDetails) {
    this._isPausing = false;
    this._debuggerPausedDetails = debuggerPausedDetails;
    if (this._debuggerPausedDetails) {
      if (Runtime.experiments.isEnabled('emptySourceMapAutoStepping') && this._beforePausedCallback) {
        if (!this._beforePausedCallback.call(null, this._debuggerPausedDetails))
          return false;
      }
      this.dispatchEventToListeners(SDK.DebuggerModel.Events.DebuggerPaused, this);
    }
    if (debuggerPausedDetails)
      this.setSelectedCallFrame(debuggerPausedDetails.callFrames[0]);
    else
      this.setSelectedCallFrame(null);
    return true;
  }

  /**
   * @param {?function(!SDK.DebuggerPausedDetails):boolean} callback
   */
  setBeforePausedCallback(callback) {
    this._beforePausedCallback = callback;
  }

  /**
   * @param {!Array.<!Protocol.Debugger.CallFrame>} callFrames
   * @param {string} reason
   * @param {!Object|undefined} auxData
   * @param {!Array.<string>} breakpointIds
   * @param {!Protocol.Runtime.StackTrace=} asyncStackTrace
   */
  _pausedScript(callFrames, reason, auxData, breakpointIds, asyncStackTrace) {
    var pausedDetails =
        new SDK.DebuggerPausedDetails(this, callFrames, reason, auxData, breakpointIds, asyncStackTrace);

    if (pausedDetails && this._continueToLocationCallback) {
      var callback = this._continueToLocationCallback;
      delete this._continueToLocationCallback;
      if (callback(pausedDetails))
        return;
    }

    if (!this._setDebuggerPausedDetails(pausedDetails))
      this._agent.stepInto();
  }

  _resumedScript() {
    this._setDebuggerPausedDetails(null);
    this.dispatchEventToListeners(SDK.DebuggerModel.Events.DebuggerResumed, this);
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {string} sourceURL
   * @param {number} startLine
   * @param {number} startColumn
   * @param {number} endLine
   * @param {number} endColumn
   * @param {!Protocol.Runtime.ExecutionContextId} executionContextId
   * @param {string} hash
   * @param {*|undefined} executionContextAuxData
   * @param {boolean} isLiveEdit
   * @param {string|undefined} sourceMapURL
   * @param {boolean} hasSourceURL
   * @param {boolean} hasSyntaxError
   * @param {number} length
   * @return {!SDK.Script}
   */
  _parsedScriptSource(
      scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId, hash,
      executionContextAuxData, isLiveEdit, sourceMapURL, hasSourceURL, hasSyntaxError, length) {
    var isContentScript = false;
    if (executionContextAuxData && ('isDefault' in executionContextAuxData))
      isContentScript = !executionContextAuxData['isDefault'];
    // Support file URL for node.js.
    if (this.target().isNodeJS() && sourceURL && sourceURL.startsWith('/')) {
      var nodeJSPath = sourceURL;
      sourceURL = Common.ParsedURL.platformPathToURL(nodeJSPath);
      sourceURL = this._internString(sourceURL);
      this._fileURLToNodeJSPath.set(sourceURL, nodeJSPath);
    } else {
      sourceURL = this._internString(sourceURL);
    }
    var script = new SDK.Script(
        this, scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId,
        this._internString(hash), isContentScript, isLiveEdit, sourceMapURL, hasSourceURL, length);
    this._registerScript(script);
    if (!hasSyntaxError)
      this.dispatchEventToListeners(SDK.DebuggerModel.Events.ParsedScriptSource, script);
    else
      this.dispatchEventToListeners(SDK.DebuggerModel.Events.FailedToParseScriptSource, script);

    var sourceMapId = SDK.DebuggerModel._sourceMapId(script.executionContextId, script.sourceURL, script.sourceMapURL);
    if (sourceMapId && !hasSyntaxError) {
      // Consecutive script evaluations in the same execution context with the same sourceURL
      // and sourceMappingURL should result in source map reloading.
      var previousScript = this._sourceMapIdToScript.get(sourceMapId);
      if (previousScript)
        this._sourceMapManager.detachSourceMap(previousScript);
      this._sourceMapIdToScript.set(sourceMapId, script);
      this._sourceMapManager.attachSourceMap(script, script.sourceURL, script.sourceMapURL);
    }

    var isDiscardable = hasSyntaxError && script.isAnonymousScript();
    if (isDiscardable) {
      this._discardableScripts.push(script);
      this._collectDiscardedScripts();
    }
    return script;
  }

  /**
   * @param {!SDK.Script} script
   * @param {string} newSourceMapURL
   */
  setSourceMapURL(script, newSourceMapURL) {
    var sourceMapId = SDK.DebuggerModel._sourceMapId(script.executionContextId, script.sourceURL, script.sourceMapURL);
    if (sourceMapId && this._sourceMapIdToScript.get(sourceMapId) === script)
      this._sourceMapIdToScript.delete(sourceMapId);
    this._sourceMapManager.detachSourceMap(script);

    script.sourceMapURL = newSourceMapURL;
    sourceMapId = SDK.DebuggerModel._sourceMapId(script.executionContextId, script.sourceURL, script.sourceMapURL);
    if (!sourceMapId)
      return;
    this._sourceMapIdToScript.set(sourceMapId, script);
    this._sourceMapManager.attachSourceMap(script, script.sourceURL, script.sourceMapURL);
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   */
  executionContextDestroyed(executionContext) {
    var sourceMapIds = Array.from(this._sourceMapIdToScript.keys());
    for (var sourceMapId of sourceMapIds) {
      var script = this._sourceMapIdToScript.get(sourceMapId);
      if (script.executionContextId === executionContext.id) {
        this._sourceMapIdToScript.delete(sourceMapId);
        this._sourceMapManager.detachSourceMap(script);
      }
    }
  }

  /**
   * @param {!SDK.Script} script
   */
  _registerScript(script) {
    this._scripts.set(script.scriptId, script);
    if (script.isAnonymousScript())
      return;

    var scripts = this._scriptsBySourceURL.get(script.sourceURL);
    if (!scripts) {
      scripts = [];
      this._scriptsBySourceURL.set(script.sourceURL, scripts);
    }
    scripts.push(script);
  }

  /**
   * @param {!SDK.Script} script
   */
  _unregisterScript(script) {
    console.assert(script.isAnonymousScript());
    this._scripts.delete(script.scriptId);
  }

  _collectDiscardedScripts() {
    if (this._discardableScripts.length < 1000)
      return;
    var scriptsToDiscard = this._discardableScripts.splice(0, 100);
    for (var script of scriptsToDiscard) {
      this._unregisterScript(script);
      this.dispatchEventToListeners(SDK.DebuggerModel.Events.DiscardedAnonymousScriptSource, script);
    }
  }

  /**
   * @param {!SDK.Script} script
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {?SDK.DebuggerModel.Location}
   */
  createRawLocation(script, lineNumber, columnNumber) {
    if (script.sourceURL)
      return this.createRawLocationByURL(script.sourceURL, lineNumber, columnNumber);
    return new SDK.DebuggerModel.Location(this, script.scriptId, lineNumber, columnNumber);
  }

  /**
   * @param {string} sourceURL
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {?SDK.DebuggerModel.Location}
   */
  createRawLocationByURL(sourceURL, lineNumber, columnNumber) {
    var closestScript = null;
    var scripts = this._scriptsBySourceURL.get(sourceURL) || [];
    for (var i = 0, l = scripts.length; i < l; ++i) {
      var script = scripts[i];
      if (!closestScript)
        closestScript = script;
      if (script.lineOffset > lineNumber || (script.lineOffset === lineNumber && script.columnOffset > columnNumber))
        continue;
      if (script.endLine < lineNumber || (script.endLine === lineNumber && script.endColumn <= columnNumber))
        continue;
      closestScript = script;
      break;
    }
    return closestScript ? new SDK.DebuggerModel.Location(this, closestScript.scriptId, lineNumber, columnNumber) :
                           null;
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {?SDK.DebuggerModel.Location}
   */
  createRawLocationByScriptId(scriptId, lineNumber, columnNumber) {
    var script = this.scriptForId(scriptId);
    return script ? this.createRawLocation(script, lineNumber, columnNumber) : null;
  }

  /**
   * @param {!Protocol.Runtime.StackTrace} stackTrace
   * @return {!Array<!SDK.DebuggerModel.Location>}
   */
  createRawLocationsByStackTrace(stackTrace) {
    var frames = [];
    while (stackTrace) {
      for (var frame of stackTrace.callFrames)
        frames.push(frame);
      stackTrace = stackTrace.parent;
    }

    var rawLocations = [];
    for (var frame of frames) {
      var rawLocation = this.createRawLocationByScriptId(frame.scriptId, frame.lineNumber, frame.columnNumber);
      if (rawLocation)
        rawLocations.push(rawLocation);
    }
    return rawLocations;
  }

  /**
   * @return {boolean}
   */
  isPaused() {
    return !!this.debuggerPausedDetails();
  }

  /**
   * @return {boolean}
   */
  isPausing() {
    return this._isPausing;
  }

  /**
   * @param {?SDK.DebuggerModel.CallFrame} callFrame
   */
  setSelectedCallFrame(callFrame) {
    if (this._selectedCallFrame === callFrame)
      return;
    this._selectedCallFrame = callFrame;
    this.dispatchEventToListeners(SDK.DebuggerModel.Events.CallFrameSelected, this);
  }

  /**
   * @return {?SDK.DebuggerModel.CallFrame}
   */
  selectedCallFrame() {
    return this._selectedCallFrame;
  }

  /**
   * @param {string} code
   * @param {string} objectGroup
   * @param {boolean} includeCommandLineAPI
   * @param {boolean} silent
   * @param {boolean} returnByValue
   * @param {boolean} generatePreview
   * @param {function(?SDK.RemoteObject, !Protocol.Runtime.ExceptionDetails=, string=)} callback
   */
  evaluateOnSelectedCallFrame(
      code,
      objectGroup,
      includeCommandLineAPI,
      silent,
      returnByValue,
      generatePreview,
      callback) {
    /**
     * @param {?Protocol.Runtime.RemoteObject} result
     * @param {!Protocol.Runtime.ExceptionDetails=} exceptionDetails
     * @param {string=} error
     * @this {SDK.DebuggerModel}
     */
    function didEvaluate(result, exceptionDetails, error) {
      if (!result)
        callback(null, undefined, error);
      else
        callback(this._runtimeModel.createRemoteObject(result), exceptionDetails);
    }

    this.selectedCallFrame().evaluate(
        code, objectGroup, includeCommandLineAPI, silent, returnByValue, generatePreview, didEvaluate.bind(this));
  }

  /**
   * @param {!SDK.RemoteObject} remoteObject
   * @return {!Promise<?SDK.DebuggerModel.FunctionDetails>}
   */
  functionDetailsPromise(remoteObject) {
    return remoteObject.getAllPropertiesPromise(false /* accessorPropertiesOnly */, false /* generatePreview */)
        .then(buildDetails.bind(this));

    /**
     * @param {!{properties: ?Array.<!SDK.RemoteObjectProperty>, internalProperties: ?Array.<!SDK.RemoteObjectProperty>}} response
     * @return {?SDK.DebuggerModel.FunctionDetails}
     * @this {!SDK.DebuggerModel}
     */
    function buildDetails(response) {
      if (!response)
        return null;
      var location = null;
      if (response.internalProperties) {
        for (var prop of response.internalProperties) {
          if (prop.name === '[[FunctionLocation]]')
            location = prop.value;
        }
      }
      var functionName = null;
      if (response.properties) {
        for (var prop of response.properties) {
          if (prop.name === 'name' && prop.value && prop.value.type === 'string')
            functionName = prop.value;
          if (prop.name === 'displayName' && prop.value && prop.value.type === 'string') {
            functionName = prop.value;
            break;
          }
        }
      }
      var debuggerLocation = null;
      if (location) {
        debuggerLocation = this.createRawLocationByScriptId(
            location.value.scriptId, location.value.lineNumber, location.value.columnNumber);
      }
      return {location: debuggerLocation, functionName: functionName ? functionName.value : ''};
    }
  }

  /**
   * @param {number} scopeNumber
   * @param {string} variableName
   * @param {!Protocol.Runtime.CallArgument} newValue
   * @param {string} callFrameId
   * @param {function(string=)=} callback
   */
  setVariableValue(scopeNumber, variableName, newValue, callFrameId, callback) {
    this._agent.setVariableValue(scopeNumber, variableName, newValue, callFrameId, innerCallback);

    /**
     * @param {?Protocol.Error} error
     */
    function innerCallback(error) {
      if (error) {
        console.error(error);
        if (callback)
          callback(error);
        return;
      }
      if (callback)
        callback();
    }
  }

  /**
   * @param {!Protocol.Debugger.BreakpointId} breakpointId
   * @param {function(!Common.Event)} listener
   * @param {!Object=} thisObject
   */
  addBreakpointListener(breakpointId, listener, thisObject) {
    this._breakpointResolvedEventTarget.addEventListener(breakpointId, listener, thisObject);
  }

  /**
   * @param {!Protocol.Debugger.BreakpointId} breakpointId
   * @param {function(!Common.Event)} listener
   * @param {!Object=} thisObject
   */
  removeBreakpointListener(breakpointId, listener, thisObject) {
    this._breakpointResolvedEventTarget.removeEventListener(breakpointId, listener, thisObject);
  }

  /**
   * @param {!Array<string>} patterns
   * @return {!Promise<boolean>}
   */
  setBlackboxPatterns(patterns) {
    var callback;
    var promise = new Promise(fulfill => callback = fulfill);
    this._agent.setBlackboxPatterns(patterns, patternsUpdated);
    return promise;

    /**
     * @param {?Protocol.Error} error
     */
    function patternsUpdated(error) {
      if (error)
        console.error(error);
      callback(!error);
    }
  }

  /**
   * @override
   */
  dispose() {
    this._sourceMapManager.dispose();
    Common.moduleSetting('pauseOnExceptionEnabled').removeChangeListener(this._pauseOnExceptionStateChanged, this);
    Common.moduleSetting('pauseOnCaughtException').removeChangeListener(this._pauseOnExceptionStateChanged, this);
    Common.moduleSetting('disableAsyncStackTraces').removeChangeListener(this._asyncStackTracesStateChanged, this);
  }

  /**
   * @override
   * @return {!Promise}
   */
  async suspendModel() {
    await this._disableDebugger();
  }

  /**
   * @override
   * @return {!Promise}
   */
  async resumeModel() {
    await this._enableDebugger();
  }

  /**
   * @param {string} string
   * @return {string} string
   */
  _internString(string) {
    if (!this._stringMap.has(string))
      this._stringMap.set(string, string);
    return this._stringMap.get(string);
  }
};

SDK.SDKModel.register(SDK.DebuggerModel, SDK.Target.Capability.JS, true);

/** @typedef {{location: ?SDK.DebuggerModel.Location, functionName: string}} */
SDK.DebuggerModel.FunctionDetails;

/**
 * Keep these in sync with WebCore::V8Debugger
 *
 * @enum {string}
 */
SDK.DebuggerModel.PauseOnExceptionsState = {
  DontPauseOnExceptions: 'none',
  PauseOnAllExceptions: 'all',
  PauseOnUncaughtExceptions: 'uncaught'
};

/** @enum {symbol} */
SDK.DebuggerModel.Events = {
  DebuggerWasEnabled: Symbol('DebuggerWasEnabled'),
  DebuggerWasDisabled: Symbol('DebuggerWasDisabled'),
  DebuggerPaused: Symbol('DebuggerPaused'),
  DebuggerResumed: Symbol('DebuggerResumed'),
  ParsedScriptSource: Symbol('ParsedScriptSource'),
  FailedToParseScriptSource: Symbol('FailedToParseScriptSource'),
  DiscardedAnonymousScriptSource: Symbol('DiscardedAnonymousScriptSource'),
  GlobalObjectCleared: Symbol('GlobalObjectCleared'),
  CallFrameSelected: Symbol('CallFrameSelected'),
  ConsoleCommandEvaluatedInSelectedCallFrame: Symbol('ConsoleCommandEvaluatedInSelectedCallFrame')
};

/** @enum {string} */
SDK.DebuggerModel.BreakReason = {
  DOM: 'DOM',
  EventListener: 'EventListener',
  XHR: 'XHR',
  Exception: 'exception',
  PromiseRejection: 'promiseRejection',
  Assert: 'assert',
  DebugCommand: 'debugCommand',
  OOM: 'OOM',
  Other: 'other'
};

/** @enum {string} */
SDK.DebuggerModel.BreakLocationType = {
  Return: 'return',
  Call: 'call',
  DebuggerStatement: 'debuggerStatement'
};

SDK.DebuggerEventTypes = {
  JavaScriptPause: 0,
  JavaScriptBreakpoint: 1,
  NativeBreakpoint: 2
};

/**
 * @implements {Protocol.DebuggerDispatcher}
 * @unrestricted
 */
SDK.DebuggerDispatcher = class {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   */
  constructor(debuggerModel) {
    this._debuggerModel = debuggerModel;
  }

  /**
   * @override
   * @param {!Array.<!Protocol.Debugger.CallFrame>} callFrames
   * @param {string} reason
   * @param {!Object=} auxData
   * @param {!Array.<string>=} breakpointIds
   * @param {!Protocol.Runtime.StackTrace=} asyncStackTrace
   */
  paused(callFrames, reason, auxData, breakpointIds, asyncStackTrace) {
    this._debuggerModel._pausedScript(callFrames, reason, auxData, breakpointIds || [], asyncStackTrace);
  }

  /**
   * @override
   */
  resumed() {
    this._debuggerModel._resumedScript();
  }

  /**
   * @override
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {string} sourceURL
   * @param {number} startLine
   * @param {number} startColumn
   * @param {number} endLine
   * @param {number} endColumn
   * @param {!Protocol.Runtime.ExecutionContextId} executionContextId
   * @param {string} hash
   * @param {*=} executionContextAuxData
   * @param {boolean=} isLiveEdit
   * @param {string=} sourceMapURL
   * @param {boolean=} hasSourceURL
   * @param {boolean=} isModule
   * @param {number=} length
   */
  scriptParsed(
      scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId, hash,
      executionContextAuxData, isLiveEdit, sourceMapURL, hasSourceURL, isModule, length) {
    this._debuggerModel._parsedScriptSource(
        scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId, hash,
        executionContextAuxData, !!isLiveEdit, sourceMapURL, !!hasSourceURL, false, length || 0);
  }

  /**
   * @override
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {string} sourceURL
   * @param {number} startLine
   * @param {number} startColumn
   * @param {number} endLine
   * @param {number} endColumn
   * @param {!Protocol.Runtime.ExecutionContextId} executionContextId
   * @param {string} hash
   * @param {*=} executionContextAuxData
   * @param {string=} sourceMapURL
   * @param {boolean=} hasSourceURL
   * @param {boolean=} isModule
   * @param {number=} length
   */
  scriptFailedToParse(
      scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId, hash,
      executionContextAuxData, sourceMapURL, hasSourceURL, isModule, length) {
    this._debuggerModel._parsedScriptSource(
        scriptId, sourceURL, startLine, startColumn, endLine, endColumn, executionContextId, hash,
        executionContextAuxData, false, sourceMapURL, !!hasSourceURL, true, length || 0);
  }

  /**
   * @override
   * @param {!Protocol.Debugger.BreakpointId} breakpointId
   * @param {!Protocol.Debugger.Location} location
   */
  breakpointResolved(breakpointId, location) {
    this._debuggerModel._breakpointResolved(breakpointId, location);
  }
};

/**
 * @unrestricted
 */
SDK.DebuggerModel.Location = class {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {string} scriptId
   * @param {number} lineNumber
   * @param {number=} columnNumber
   */
  constructor(debuggerModel, scriptId, lineNumber, columnNumber) {
    this.debuggerModel = debuggerModel;
    this.scriptId = scriptId;
    this.lineNumber = lineNumber;
    this.columnNumber = columnNumber || 0;
  }

  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!Protocol.Debugger.Location} payload
   * @return {!SDK.DebuggerModel.Location}
   */
  static fromPayload(debuggerModel, payload) {
    return new SDK.DebuggerModel.Location(debuggerModel, payload.scriptId, payload.lineNumber, payload.columnNumber);
  }

  /**
   * @return {!Protocol.Debugger.Location}
   */
  payload() {
    return {scriptId: this.scriptId, lineNumber: this.lineNumber, columnNumber: this.columnNumber};
  }

  /**
   * @return {?SDK.Script}
   */
  script() {
    return this.debuggerModel.scriptForId(this.scriptId);
  }

  /**
   * @param {function()=} pausedCallback
   */
  continueToLocation(pausedCallback) {
    if (pausedCallback)
      this.debuggerModel._continueToLocationCallback = this._paused.bind(this, pausedCallback);
    this.debuggerModel._agent.continueToLocation(this.payload());
  }

  /**
   * @param {function()|undefined} pausedCallback
   * @param {!SDK.DebuggerPausedDetails} debuggerPausedDetails
   * @return {boolean}
   */
  _paused(pausedCallback, debuggerPausedDetails) {
    var location = debuggerPausedDetails.callFrames[0].location();
    if (location.scriptId === this.scriptId && location.lineNumber === this.lineNumber &&
        location.columnNumber === this.columnNumber) {
      pausedCallback();
      return true;
    }
    return false;
  }

  /**
   * @return {string}
   */
  id() {
    return this.debuggerModel.target().id() + ':' + this.scriptId + ':' + this.lineNumber + ':' + this.columnNumber;
  }
};

/**
 * @unrestricted
 */
SDK.DebuggerModel.BreakLocation = class extends SDK.DebuggerModel.Location {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {string} scriptId
   * @param {number} lineNumber
   * @param {number=} columnNumber
   * @param {!Protocol.Debugger.BreakLocationType=} type
   */
  constructor(debuggerModel, scriptId, lineNumber, columnNumber, type) {
    super(debuggerModel, scriptId, lineNumber, columnNumber);
    if (type)
      this.type = type;
  }

  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!Protocol.Debugger.BreakLocation} payload
   * @return {!SDK.DebuggerModel.BreakLocation}
   */
  static fromPayload(debuggerModel, payload) {
    return new SDK.DebuggerModel.BreakLocation(
        debuggerModel, payload.scriptId, payload.lineNumber, payload.columnNumber, payload.type);
  }
};

/**
 * @unrestricted
 */
SDK.DebuggerModel.CallFrame = class {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!SDK.Script} script
   * @param {!Protocol.Debugger.CallFrame} payload
   */
  constructor(debuggerModel, script, payload) {
    this.debuggerModel = debuggerModel;
    this._script = script;
    this._payload = payload;
    this._location = SDK.DebuggerModel.Location.fromPayload(debuggerModel, payload.location);
    this._scopeChain = [];
    this._localScope = null;
    for (var i = 0; i < payload.scopeChain.length; ++i) {
      var scope = new SDK.DebuggerModel.Scope(this, i);
      this._scopeChain.push(scope);
      if (scope.type() === Protocol.Debugger.ScopeType.Local)
        this._localScope = scope;
    }
    if (payload.functionLocation)
      this._functionLocation = SDK.DebuggerModel.Location.fromPayload(debuggerModel, payload.functionLocation);
  }

  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!Array.<!Protocol.Debugger.CallFrame>} callFrames
   * @return {!Array.<!SDK.DebuggerModel.CallFrame>}
   */
  static fromPayloadArray(debuggerModel, callFrames) {
    var result = [];
    for (var i = 0; i < callFrames.length; ++i) {
      var callFrame = callFrames[i];
      var script = debuggerModel.scriptForId(callFrame.location.scriptId);
      if (script)
        result.push(new SDK.DebuggerModel.CallFrame(debuggerModel, script, callFrame));
    }
    return result;
  }

  /**
   * @return {!SDK.Script}
   */
  get script() {
    return this._script;
  }

  /**
   * @return {string}
   */
  get id() {
    return this._payload.callFrameId;
  }

  /**
   * @return {!Array.<!SDK.DebuggerModel.Scope>}
   */
  scopeChain() {
    return this._scopeChain;
  }

  /**
   * @return {?SDK.DebuggerModel.Scope}
   */
  localScope() {
    return this._localScope;
  }

  /**
   * @return {?SDK.RemoteObject}
   */
  thisObject() {
    return this._payload.this ? this.debuggerModel._runtimeModel.createRemoteObject(this._payload.this) : null;
  }

  /**
   * @return {?SDK.RemoteObject}
   */
  returnValue() {
    return this._payload.returnValue ? this.debuggerModel._runtimeModel.createRemoteObject(this._payload.returnValue) :
                                       null;
  }

  /**
   * @return {string}
   */
  get functionName() {
    return this._payload.functionName;
  }

  /**
   * @return {!SDK.DebuggerModel.Location}
   */
  location() {
    return this._location;
  }

  /**
   * @return {?SDK.DebuggerModel.Location}
   */
  functionLocation() {
    return this._functionLocation || null;
  }

  /**
   * @param {string} code
   * @param {string} objectGroup
   * @param {boolean} includeCommandLineAPI
   * @param {boolean} silent
   * @param {boolean} returnByValue
   * @param {boolean} generatePreview
   * @param {function(?Protocol.Runtime.RemoteObject, !Protocol.Runtime.ExceptionDetails=, string=)} callback
   */
  async evaluate(code, objectGroup, includeCommandLineAPI, silent, returnByValue, generatePreview, callback) {
    var response = await this.debuggerModel._agent.invoke_evaluateOnCallFrame({
      callFrameId: this._payload.callFrameId,
      expression: code,
      objectGroup: objectGroup,
      includeCommandLineAPI: includeCommandLineAPI,
      silent: silent,
      returnByValue: returnByValue,
      generatePreview: generatePreview
    });
    var error = response[Protocol.Error];
    if (error) {
      console.error(error);
      callback(null, undefined, error);
      return;
    }
    callback(response.result, response.exceptionDetails);
  }

  /**
   * @param {string} code
   * @param {string} objectGroup
   * @param {boolean} includeCommandLineAPI
   * @param {boolean} silent
   * @param {boolean} returnByValue
   * @param {boolean} generatePreview
   * @return {!Promise<?SDK.RemoteObject>}
   */
  evaluatePromise(code, objectGroup, includeCommandLineAPI, silent, returnByValue, generatePreview) {
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    this.evaluate(
        code, objectGroup, includeCommandLineAPI, silent, returnByValue, generatePreview, callback.bind(this));
    return promise;

    /**
     * @param {?Protocol.Runtime.RemoteObject} result
     * @param {!Protocol.Runtime.ExceptionDetails=} exceptionDetails
     * @param {string=} error
     * @this {SDK.DebuggerModel.CallFrame}
     */
    function callback(result, exceptionDetails, error) {
      if (!result || exceptionDetails)
        fulfill(null);
      else
        fulfill(this.debuggerModel._runtimeModel.createRemoteObject(result));
    }
  }

  /**
   * @param {function(?Protocol.Error=)=} callback
   */
  restart(callback) {
    /**
     * @param {?Protocol.Error} error
     * @param {!Array.<!Protocol.Debugger.CallFrame>=} callFrames
     * @param {!Protocol.Runtime.StackTrace=} asyncStackTrace
     * @this {SDK.DebuggerModel.CallFrame}
     */
    function protocolCallback(error, callFrames, asyncStackTrace) {
      if (!error)
        this.debuggerModel.stepInto();
      if (callback)
        callback(error);
    }
    this.debuggerModel._agent.restartFrame(this._payload.callFrameId, protocolCallback.bind(this));
  }
};


/**
 * @unrestricted
 */
SDK.DebuggerModel.Scope = class {
  /**
   * @param {!SDK.DebuggerModel.CallFrame} callFrame
   * @param {number} ordinal
   */
  constructor(callFrame, ordinal) {
    this._callFrame = callFrame;
    this._payload = callFrame._payload.scopeChain[ordinal];
    this._type = this._payload.type;
    this._name = this._payload.name;
    this._ordinal = ordinal;
    this._startLocation = this._payload.startLocation ?
        SDK.DebuggerModel.Location.fromPayload(callFrame.debuggerModel, this._payload.startLocation) :
        null;
    this._endLocation = this._payload.endLocation ?
        SDK.DebuggerModel.Location.fromPayload(callFrame.debuggerModel, this._payload.endLocation) :
        null;
  }

  /**
   * @return {!SDK.DebuggerModel.CallFrame}
   */
  callFrame() {
    return this._callFrame;
  }

  /**
   * @return {string}
   */
  type() {
    return this._type;
  }

  /**
   * @return {string}
   */
  typeName() {
    switch (this._type) {
      case Protocol.Debugger.ScopeType.Local:
        return Common.UIString('Local');
      case Protocol.Debugger.ScopeType.Closure:
        return Common.UIString('Closure');
      case Protocol.Debugger.ScopeType.Catch:
        return Common.UIString('Catch');
      case Protocol.Debugger.ScopeType.Block:
        return Common.UIString('Block');
      case Protocol.Debugger.ScopeType.Script:
        return Common.UIString('Script');
      case Protocol.Debugger.ScopeType.With:
        return Common.UIString('With Block');
      case Protocol.Debugger.ScopeType.Global:
        return Common.UIString('Global');
    }
    return '';
  }


  /**
   * @return {string|undefined}
   */
  name() {
    return this._name;
  }

  /**
   * @return {?SDK.DebuggerModel.Location}
   */
  startLocation() {
    return this._startLocation;
  }

  /**
   * @return {?SDK.DebuggerModel.Location}
   */
  endLocation() {
    return this._endLocation;
  }

  /**
   * @return {!SDK.RemoteObject}
   */
  object() {
    if (this._object)
      return this._object;
    var runtimeModel = this._callFrame.debuggerModel._runtimeModel;

    var declarativeScope =
        this._type !== Protocol.Debugger.ScopeType.With && this._type !== Protocol.Debugger.ScopeType.Global;
    if (declarativeScope) {
      this._object = runtimeModel.createScopeRemoteObject(
          this._payload.object, new SDK.ScopeRef(this._ordinal, this._callFrame.id));
    } else {
      this._object = runtimeModel.createRemoteObject(this._payload.object);
    }

    return this._object;
  }

  /**
   * @return {string}
   */
  description() {
    var declarativeScope =
        this._type !== Protocol.Debugger.ScopeType.With && this._type !== Protocol.Debugger.ScopeType.Global;
    return declarativeScope ? '' : (this._payload.object.description || '');
  }
};

/**
 * @unrestricted
 */
SDK.DebuggerPausedDetails = class {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!Array.<!Protocol.Debugger.CallFrame>} callFrames
   * @param {string} reason
   * @param {!Object|undefined} auxData
   * @param {!Array.<string>} breakpointIds
   * @param {!Protocol.Runtime.StackTrace=} asyncStackTrace
   */
  constructor(debuggerModel, callFrames, reason, auxData, breakpointIds, asyncStackTrace) {
    this.debuggerModel = debuggerModel;
    this.callFrames = SDK.DebuggerModel.CallFrame.fromPayloadArray(debuggerModel, callFrames);
    this.reason = reason;
    this.auxData = auxData;
    this.breakpointIds = breakpointIds;
    if (asyncStackTrace)
      this.asyncStackTrace = this._cleanRedundantFrames(asyncStackTrace);
  }

  /**
   * @return {?SDK.RemoteObject}
   */
  exception() {
    if (this.reason !== SDK.DebuggerModel.BreakReason.Exception &&
        this.reason !== SDK.DebuggerModel.BreakReason.PromiseRejection)
      return null;
    return this.debuggerModel._runtimeModel.createRemoteObject(
        /** @type {!Protocol.Runtime.RemoteObject} */ (this.auxData));
  }

  /**
   * @param {!Protocol.Runtime.StackTrace} asyncStackTrace
   * @return {!Protocol.Runtime.StackTrace}
   */
  _cleanRedundantFrames(asyncStackTrace) {
    var stack = asyncStackTrace;
    var previous = null;
    while (stack) {
      if (stack.description === 'async function' && stack.callFrames.length)
        stack.callFrames.shift();
      if (previous && (!stack.callFrames.length && !stack.promiseCreationFrame))
        previous.parent = stack.parent;
      else
        previous = stack;
      stack = stack.parent;
    }
    return asyncStackTrace;
  }
};
