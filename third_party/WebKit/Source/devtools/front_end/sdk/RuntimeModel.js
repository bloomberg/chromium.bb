/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
SDK.RuntimeModel = class extends SDK.SDKModel {
  /**
   * @param {!SDK.Target} target
   */
  constructor(target) {
    super(target);

    this._agent = target.runtimeAgent();
    this.target().registerRuntimeDispatcher(new SDK.RuntimeDispatcher(this));
    this._agent.enable();
    /** @type {!Map<number, !SDK.ExecutionContext>} */
    this._executionContextById = new Map();
    this._executionContextComparator = SDK.ExecutionContext.comparator;

    if (Common.moduleSetting('customFormatters').get())
      this._agent.setCustomObjectFormatterEnabled(true);

    Common.moduleSetting('customFormatters').addChangeListener(this._customFormattersStateChanged.bind(this));
  }

  /**
   * @param {string} code
   * @return {string}
   */
  static wrapObjectLiteralExpressionIfNeeded(code) {
    // Only parenthesize what appears to be an object literal.
    if (!(/^\s*\{/.test(code) && /\}\s*$/.test(code)))
      return code;

    try {
      // Check if the code can be interpreted as an expression.
      Function('return ' + code + ';');

      // No syntax error! Does it work parenthesized?
      var wrappedCode = '(' + code + ')';
      Function(wrappedCode);

      return wrappedCode;
    } catch (e) {
      return code;
    }
  }

  /**
   * @return {!SDK.DebuggerModel}
   */
  debuggerModel() {
    return /** @type {!SDK.DebuggerModel} */ (this.target().model(SDK.DebuggerModel));
  }

  /**
   * @return {!SDK.HeapProfilerModel}
   */
  heapProfilerModel() {
    return /** @type {!SDK.HeapProfilerModel} */ (this.target().model(SDK.HeapProfilerModel));
  }

  /**
   * @return {!Array.<!SDK.ExecutionContext>}
   */
  executionContexts() {
    return this._executionContextById.valuesArray().sort(this.executionContextComparator());
  }

  /**
   * @param {function(!SDK.ExecutionContext,!SDK.ExecutionContext)} comparator
   */
  setExecutionContextComparator(comparator) {
    this._executionContextComparator = comparator;
  }

  /**
   * @return {function(!SDK.ExecutionContext,!SDK.ExecutionContext)} comparator
   */
  executionContextComparator() {
    return this._executionContextComparator;
  }

  /**
   * @return {?SDK.ExecutionContext}
   */
  defaultExecutionContext() {
    for (var context of this._executionContextById.values()) {
      if (context.isDefault)
        return context;
    }
    return null;
  }

  /**
   * @param {!Protocol.Runtime.ExecutionContextId} id
   * @return {?SDK.ExecutionContext}
   */
  executionContext(id) {
    return this._executionContextById.get(id) || null;
  }

  /**
   * @param {!Protocol.Runtime.ExecutionContextDescription} context
   */
  _executionContextCreated(context) {
    var data = context.auxData || {isDefault: true};
    var executionContext =
        new SDK.ExecutionContext(this, context.id, context.name, context.origin, data['isDefault'], data['frameId']);
    this._executionContextById.set(executionContext.id, executionContext);
    this.dispatchEventToListeners(SDK.RuntimeModel.Events.ExecutionContextCreated, executionContext);
  }

  /**
   * @param {number} executionContextId
   */
  _executionContextDestroyed(executionContextId) {
    var executionContext = this._executionContextById.get(executionContextId);
    if (!executionContext)
      return;
    this.debuggerModel().executionContextDestroyed(executionContext);
    this._executionContextById.delete(executionContextId);
    this.dispatchEventToListeners(SDK.RuntimeModel.Events.ExecutionContextDestroyed, executionContext);
  }

  fireExecutionContextOrderChanged() {
    this.dispatchEventToListeners(SDK.RuntimeModel.Events.ExecutionContextOrderChanged, this);
  }

  _executionContextsCleared() {
    this.debuggerModel().globalObjectCleared();
    var contexts = this.executionContexts();
    this._executionContextById.clear();
    for (var i = 0; i < contexts.length; ++i)
      this.dispatchEventToListeners(SDK.RuntimeModel.Events.ExecutionContextDestroyed, contexts[i]);
  }

  /**
   * @param {!Protocol.Runtime.RemoteObject} payload
   * @return {!SDK.RemoteObject}
   */
  createRemoteObject(payload) {
    console.assert(typeof payload === 'object', 'Remote object payload should only be an object');
    return new SDK.RemoteObjectImpl(
        this, payload.objectId, payload.type, payload.subtype, payload.value, payload.unserializableValue,
        payload.description, payload.preview, payload.customPreview);
  }

  /**
   * @param {!Protocol.Runtime.RemoteObject} payload
   * @param {!SDK.ScopeRef} scopeRef
   * @return {!SDK.RemoteObject}
   */
  createScopeRemoteObject(payload, scopeRef) {
    return new SDK.ScopeRemoteObject(
        this, payload.objectId, scopeRef, payload.type, payload.subtype, payload.value, payload.unserializableValue,
        payload.description, payload.preview);
  }

  /**
   * @param {number|string|boolean|undefined} value
   * @return {!SDK.RemoteObject}
   */
  createRemoteObjectFromPrimitiveValue(value) {
    var type = typeof value;
    var unserializableValue = undefined;
    if (typeof value === 'number') {
      var description = String(value);
      if (value === 0 && 1 / value < 0)
        unserializableValue = Protocol.Runtime.UnserializableValue.Negative0;
      if (description === 'NaN')
        unserializableValue = Protocol.Runtime.UnserializableValue.NaN;
      if (description === 'Infinity')
        unserializableValue = Protocol.Runtime.UnserializableValue.Infinity;
      if (description === '-Infinity')
        unserializableValue = Protocol.Runtime.UnserializableValue.NegativeInfinity;
      if (typeof unserializableValue !== 'undefined')
        value = undefined;
    }
    return new SDK.RemoteObjectImpl(this, undefined, type, undefined, value, unserializableValue);
  }

  /**
   * @param {string} name
   * @param {number|string|boolean} value
   * @return {!SDK.RemoteObjectProperty}
   */
  createRemotePropertyFromPrimitiveValue(name, value) {
    return new SDK.RemoteObjectProperty(name, this.createRemoteObjectFromPrimitiveValue(value));
  }

  discardConsoleEntries() {
    this._agent.discardConsoleEntries();
  }

  /**
   * @param {string} objectGroupName
   */
  releaseObjectGroup(objectGroupName) {
    this._agent.releaseObjectGroup(objectGroupName);
  }

  runIfWaitingForDebugger() {
    this._agent.runIfWaitingForDebugger();
  }

  /**
   * @param {!Common.Event} event
   */
  _customFormattersStateChanged(event) {
    var enabled = /** @type {boolean} */ (event.data);
    this._agent.setCustomObjectFormatterEnabled(enabled);
  }

  /**
   * @param {string} expression
   * @param {string} sourceURL
   * @param {boolean} persistScript
   * @param {number} executionContextId
   * @param {function(!Protocol.Runtime.ScriptId=, ?Protocol.Runtime.ExceptionDetails=)=} callback
   */
  async compileScript(expression, sourceURL, persistScript, executionContextId, callback) {
    var response = await this._agent.invoke_compileScript({
      expression: expression,
      sourceURL: sourceURL,
      persistScript: persistScript,
      executionContextId: executionContextId
    });

    if (response[Protocol.Error]) {
      console.error(response[Protocol.Error]);
      return;
    }
    if (callback)
      callback(response.scriptId, response.exceptionDetails);
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @param {number} executionContextId
   * @param {string=} objectGroup
   * @param {boolean=} silent
   * @param {boolean=} includeCommandLineAPI
   * @param {boolean=} returnByValue
   * @param {boolean=} generatePreview
   * @param {boolean=} awaitPromise
   * @param {function(?Protocol.Runtime.RemoteObject, ?Protocol.Runtime.ExceptionDetails=)=} callback
   */
  async runScript(
      scriptId, executionContextId, objectGroup, silent, includeCommandLineAPI, returnByValue, generatePreview,
      awaitPromise, callback) {
    var response = await this._agent.invoke_runScript({
      scriptId,
      executionContextId,
      objectGroup,
      silent,
      includeCommandLineAPI,
      returnByValue,
      generatePreview,
      awaitPromise
    });

    if (response[Protocol.Error]) {
      console.error(response[Protocol.Error]);
      return;
    }
    if (callback)
      callback(response.result, response.exceptionDetails);
  }

  /**
   * @param {!Protocol.Runtime.RemoteObject} payload
   * @param {!Object=} hints
   */
  _inspectRequested(payload, hints) {
    var object = this.createRemoteObject(payload);

    if (hints.copyToClipboard) {
      this._copyRequested(object);
      return;
    }

    if (object.isNode()) {
      Common.Revealer.revealPromise(object).then(object.release.bind(object));
      return;
    }

    if (object.type === 'function') {
      SDK.RemoteFunction.objectAsFunction(object).targetFunctionDetails().then(didGetDetails);
      return;
    }

    /**
     * @param {?SDK.DebuggerModel.FunctionDetails} response
     */
    function didGetDetails(response) {
      object.release();
      if (!response || !response.location)
        return;
      Common.Revealer.reveal(response.location);
    }
    object.release();
  }

  /**
   * @param {!SDK.RemoteObject} object
   */
  _copyRequested(object) {
    if (!object.objectId) {
      InspectorFrontendHost.copyText(object.value);
      return;
    }
    object.callFunctionJSON(
        toStringForClipboard, [{value: object.subtype}], InspectorFrontendHost.copyText.bind(InspectorFrontendHost));

    /**
     * @param {string} subtype
     * @this {Object}
     * @suppressReceiverCheck
     */
    function toStringForClipboard(subtype) {
      if (subtype === 'node')
        return this.outerHTML;
      if (subtype && typeof this === 'undefined')
        return subtype + '';
      try {
        return JSON.stringify(this, null, '  ');
      } catch (e) {
        return '' + this;
      }
    }
  }

  /**
   * @param {!Protocol.Runtime.ExceptionDetails} exceptionDetails
   * @return {string}
   */
  static simpleTextFromException(exceptionDetails) {
    var text = exceptionDetails.text;
    if (exceptionDetails.exception && exceptionDetails.exception.description) {
      var description = exceptionDetails.exception.description;
      if (description.indexOf('\n') !== -1)
        description = description.substring(0, description.indexOf('\n'));
      text += ' ' + description;
    }
    return text;
  }

  /**
   * @param {number} timestamp
   * @param {!Protocol.Runtime.ExceptionDetails} exceptionDetails
   */
  exceptionThrown(timestamp, exceptionDetails) {
    var exceptionWithTimestamp = {timestamp: timestamp, details: exceptionDetails};
    this.dispatchEventToListeners(SDK.RuntimeModel.Events.ExceptionThrown, exceptionWithTimestamp);
  }

  /**
   * @param {number} exceptionId
   */
  _exceptionRevoked(exceptionId) {
    this.dispatchEventToListeners(SDK.RuntimeModel.Events.ExceptionRevoked, exceptionId);
  }

  /**
   * @param {string} type
   * @param {!Array.<!Protocol.Runtime.RemoteObject>} args
   * @param {number} executionContextId
   * @param {number} timestamp
   * @param {!Protocol.Runtime.StackTrace=} stackTrace
   */
  _consoleAPICalled(type, args, executionContextId, timestamp, stackTrace) {
    var consoleAPICall =
        {type: type, args: args, executionContextId: executionContextId, timestamp: timestamp, stackTrace: stackTrace};
    this.dispatchEventToListeners(SDK.RuntimeModel.Events.ConsoleAPICalled, consoleAPICall);
  }

  /**
   * @param {!Protocol.Runtime.ScriptId} scriptId
   * @return {number}
   */
  executionContextIdForScriptId(scriptId) {
    var script = this.debuggerModel().scriptForId(scriptId);
    return script ? script.executionContextId : 0;
  }

  /**
   * @param {!Protocol.Runtime.StackTrace} stackTrace
   * @return {number}
   */
  executionContextForStackTrace(stackTrace) {
    while (stackTrace && !stackTrace.callFrames.length)
      stackTrace = stackTrace.parent;
    if (!stackTrace || !stackTrace.callFrames.length)
      return 0;
    return this.executionContextIdForScriptId(stackTrace.callFrames[0].scriptId);
  }
};

SDK.SDKModel.register(SDK.RuntimeModel, SDK.Target.Capability.JS, true);

/** @enum {symbol} */
SDK.RuntimeModel.Events = {
  ExecutionContextCreated: Symbol('ExecutionContextCreated'),
  ExecutionContextDestroyed: Symbol('ExecutionContextDestroyed'),
  ExecutionContextChanged: Symbol('ExecutionContextChanged'),
  ExecutionContextOrderChanged: Symbol('ExecutionContextOrderChanged'),
  ExceptionThrown: Symbol('ExceptionThrown'),
  ExceptionRevoked: Symbol('ExceptionRevoked'),
  ConsoleAPICalled: Symbol('ConsoleAPICalled'),
};

/** @typedef {{timestamp: number, details: !Protocol.Runtime.ExceptionDetails}} */
SDK.RuntimeModel.ExceptionWithTimestamp;

/**
 * @typedef {{
 *    type: string,
 *    args: !Array<!Protocol.Runtime.RemoteObject>,
 *    executionContextId: number,
 *    timestamp: number,
 *    stackTrace: (!Protocol.Runtime.StackTrace|undefined)
 * }}
 */
SDK.RuntimeModel.ConsoleAPICall;

/**
 * @implements {Protocol.RuntimeDispatcher}
 * @unrestricted
 */
SDK.RuntimeDispatcher = class {
  /**
   * @param {!SDK.RuntimeModel} runtimeModel
   */
  constructor(runtimeModel) {
    this._runtimeModel = runtimeModel;
  }

  /**
   * @override
   * @param {!Protocol.Runtime.ExecutionContextDescription} context
   */
  executionContextCreated(context) {
    this._runtimeModel._executionContextCreated(context);
  }

  /**
   * @override
   * @param {!Protocol.Runtime.ExecutionContextId} executionContextId
   */
  executionContextDestroyed(executionContextId) {
    this._runtimeModel._executionContextDestroyed(executionContextId);
  }

  /**
   * @override
   */
  executionContextsCleared() {
    this._runtimeModel._executionContextsCleared();
  }

  /**
   * @override
   * @param {number} timestamp
   * @param {!Protocol.Runtime.ExceptionDetails} exceptionDetails
   */
  exceptionThrown(timestamp, exceptionDetails) {
    this._runtimeModel.exceptionThrown(timestamp, exceptionDetails);
  }

  /**
   * @override
   * @param {string} reason
   * @param {number} exceptionId
   */
  exceptionRevoked(reason, exceptionId) {
    this._runtimeModel._exceptionRevoked(exceptionId);
  }

  /**
   * @override
   * @param {string} type
   * @param {!Array.<!Protocol.Runtime.RemoteObject>} args
   * @param {number} executionContextId
   * @param {number} timestamp
   * @param {!Protocol.Runtime.StackTrace=} stackTrace
   */
  consoleAPICalled(type, args, executionContextId, timestamp, stackTrace) {
    this._runtimeModel._consoleAPICalled(type, args, executionContextId, timestamp, stackTrace);
  }

  /**
   * @override
   * @param {!Protocol.Runtime.RemoteObject} payload
   * @param {!Object=} hints
   */
  inspectRequested(payload, hints) {
    this._runtimeModel._inspectRequested(payload, hints);
  }
};

/**
 * @unrestricted
 */
SDK.ExecutionContext = class {
  /**
   * @param {!SDK.RuntimeModel} runtimeModel
   * @param {number} id
   * @param {string} name
   * @param {string} origin
   * @param {boolean} isDefault
   * @param {string=} frameId
   */
  constructor(runtimeModel, id, name, origin, isDefault, frameId) {
    this.id = id;
    this.name = name;
    this.origin = origin;
    this.isDefault = isDefault;
    this.runtimeModel = runtimeModel;
    this.debuggerModel = runtimeModel.debuggerModel();
    this.frameId = frameId;
    this._setLabel('');
  }

  /**
   * @return {!SDK.Target}
   */
  target() {
    return this.runtimeModel.target();
  }

  /**
   * @param {!SDK.ExecutionContext} a
   * @param {!SDK.ExecutionContext} b
   * @return {number}
   */
  static comparator(a, b) {
    /**
     * @param {!SDK.Target} target
     * @return {number}
     */
    function targetWeight(target) {
      if (!target.parentTarget())
        return 4;
      if (target.hasBrowserCapability())
        return 3;
      if (target.hasJSCapability())
        return 2;
      return 1;
    }

    var weightDiff = targetWeight(a.target()) - targetWeight(b.target());
    if (weightDiff)
      return -weightDiff;

    // Main world context should always go first.
    if (a.isDefault)
      return -1;
    if (b.isDefault)
      return +1;
    return a.name.localeCompare(b.name);
  }

  /**
   * @param {string} expression
   * @param {string} objectGroup
   * @param {boolean} includeCommandLineAPI
   * @param {boolean} silent
   * @param {boolean} returnByValue
   * @param {boolean} generatePreview
   * @param {boolean} userGesture
   * @param {function(?SDK.RemoteObject, !Protocol.Runtime.ExceptionDetails=, string=)} callback
   */
  evaluate(
      expression,
      objectGroup,
      includeCommandLineAPI,
      silent,
      returnByValue,
      generatePreview,
      userGesture,
      callback) {
    // FIXME: It will be moved to separate ExecutionContext.
    if (this.debuggerModel.selectedCallFrame()) {
      this.debuggerModel.evaluateOnSelectedCallFrame(
          expression, objectGroup, includeCommandLineAPI, silent, returnByValue, generatePreview, callback);
      return;
    }
    this._evaluateGlobal(
        expression, objectGroup, includeCommandLineAPI, silent, returnByValue, generatePreview, userGesture, callback);
  }

  /**
   * @param {string} objectGroup
   * @param {boolean} generatePreview
   * @param {function(?SDK.RemoteObject, !Protocol.Runtime.ExceptionDetails=, string=)} callback
   */
  globalObject(objectGroup, generatePreview, callback) {
    this._evaluateGlobal('this', objectGroup, false, true, false, generatePreview, false, callback);
  }

  /**
   * @param {string} expression
   * @param {string} objectGroup
   * @param {boolean} includeCommandLineAPI
   * @param {boolean} silent
   * @param {boolean} returnByValue
   * @param {boolean} generatePreview
   * @param {boolean} userGesture
   * @param {function(?SDK.RemoteObject, !Protocol.Runtime.ExceptionDetails=, string=)} callback
   */
  async _evaluateGlobal(
      expression, objectGroup, includeCommandLineAPI, silent, returnByValue, generatePreview, userGesture, callback) {
    if (!expression) {
      // There is no expression, so the completion should happen against global properties.
      expression = 'this';
    }

    var response = await this.runtimeModel._agent.invoke_evaluate({
      expression: expression,
      objectGroup: objectGroup,
      includeCommandLineAPI: includeCommandLineAPI,
      silent: silent,
      contextId: this.id,
      returnByValue: returnByValue,
      generatePreview: generatePreview,
      userGesture: userGesture,
      awaitPromise: false
    });

    var error = response[Protocol.Error];
    if (error) {
      console.error(error);
      callback(null, undefined, error);
      return;
    }
    callback(this.runtimeModel.createRemoteObject(response.result), response.exceptionDetails);
  }

  /**
   * @return {string}
   */
  label() {
    return this._label;
  }

  /**
   * @param {string} label
   */
  setLabel(label) {
    this._setLabel(label);
    this.runtimeModel.dispatchEventToListeners(SDK.RuntimeModel.Events.ExecutionContextChanged, this);
  }

  /**
   * @param {string} label
   */
  _setLabel(label) {
    if (label) {
      this._label = label;
      return;
    }
    if (this.name) {
      this._label = this.name;
      return;
    }
    var parsedUrl = this.origin.asParsedURL();
    this._label = parsedUrl ? parsedUrl.lastPathComponentWithFragment() : '';
  }
};
