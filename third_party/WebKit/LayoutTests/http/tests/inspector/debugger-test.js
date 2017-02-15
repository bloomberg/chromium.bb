function scheduleTestFunction()
{
    setTimeout(testFunction, 0);
}

var initialize_DebuggerTest = function() {

InspectorTest.preloadPanel("sources");

InspectorTest.startDebuggerTest = function(callback, quiet)
{
    console.assert(InspectorTest.debuggerModel.debuggerEnabled(), "Debugger has to be enabled");
    if (quiet !== undefined)
        InspectorTest._quiet = quiet;
    UI.viewManager.showView("sources");

    InspectorTest.addSniffer(SDK.DebuggerModel.prototype, "_pausedScript", InspectorTest._pausedScript, true);
    InspectorTest.addSniffer(SDK.DebuggerModel.prototype, "_resumedScript", InspectorTest._resumedScript, true);
    InspectorTest.safeWrap(callback)();
};

InspectorTest.startDebuggerTestPromise = function(quiet)
{
    var cb;
    var p = new Promise(fullfill => cb = fullfill);
    InspectorTest.startDebuggerTest(cb, quiet);
    return p;
}

InspectorTest.completeDebuggerTest = function()
{
    Bindings.breakpointManager.setBreakpointsActive(true);
    InspectorTest.resumeExecution(InspectorTest.completeTest.bind(InspectorTest));
};

(function() {
    // FIXME: Until there is no window.onerror() for uncaught exceptions in promises
    // we use this hack to print the exceptions instead of just timing out.

    var origThen = Promise.prototype.then;
    var origCatch = Promise.prototype.catch;

    Promise.prototype.then = function()
    {
        var result = origThen.apply(this, arguments);
        origThen.call(result, undefined, onUncaughtPromiseReject.bind(null, new Error().stack));
        return result;
    }

    Promise.prototype.catch = function()
    {
        var result = origCatch.apply(this, arguments);
        origThen.call(result, undefined, onUncaughtPromiseReject.bind(null, new Error().stack));
        return result;
    }

    function onUncaughtPromiseReject(stack, e)
    {
        var message = (typeof e === "object" && e.stack) || e;
        InspectorTest.addResult("FAIL: Uncaught exception in promise: " + message + " " + stack);
        InspectorTest.completeDebuggerTest();
    }
})();

InspectorTest.runDebuggerTestSuite = function(testSuite)
{
    var testSuiteTests = testSuite.slice();

    function runner()
    {
        if (!testSuiteTests.length) {
            InspectorTest.completeDebuggerTest();
            return;
        }

        var nextTest = testSuiteTests.shift();
        InspectorTest.addResult("");
        InspectorTest.addResult("Running: " + /function\s([^(]*)/.exec(nextTest)[1]);
        InspectorTest.safeWrap(nextTest)(runner, runner);
    }

    InspectorTest.startDebuggerTest(runner);
};

InspectorTest.runTestFunction = function()
{
    InspectorTest.evaluateInPage("scheduleTestFunction()");
    InspectorTest.addResult("Set timer for test function.");
};

InspectorTest.runTestFunctionAndWaitUntilPaused = function(callback)
{
    InspectorTest.runTestFunction();
    InspectorTest.waitUntilPaused(callback);
};

InspectorTest.runTestFunctionAndWaitUntilPausedPromise = function()
{
    var cb;
    var p = new Promise(fullfill => cb = fullfill);
    InspectorTest.runTestFunctionAndWaitUntilPaused(cb);
    return p;
}

InspectorTest.runAsyncCallStacksTest = function(totalDebuggerStatements, maxAsyncCallStackDepth)
{
    var defaultMaxAsyncCallStackDepth = 8;

    InspectorTest.setQuiet(true);
    InspectorTest.startDebuggerTest(step1);

    function step1()
    {
        InspectorTest.DebuggerAgent.setAsyncCallStackDepth(maxAsyncCallStackDepth || defaultMaxAsyncCallStackDepth, step2);
    }

    function step2()
    {
        InspectorTest.runTestFunctionAndWaitUntilPaused(didPause);
    }

    var step = 0;
    var callStacksOutput = [];
    function didPause(callFrames, reason, breakpointIds, asyncStackTrace)
    {
        ++step;
        callStacksOutput.push(InspectorTest.captureStackTraceIntoString(callFrames, asyncStackTrace) + "\n");
        if (step < totalDebuggerStatements) {
            InspectorTest.resumeExecution(InspectorTest.waitUntilPaused.bind(InspectorTest, didPause));
        } else {
            InspectorTest.addResult("Captured call stacks in no particular order:");
            callStacksOutput.sort();
            InspectorTest.addResults(callStacksOutput);
            InspectorTest.completeDebuggerTest();
        }
    }
};

InspectorTest.dumpSourceFrameMessages = function(sourceFrame, dumpFullURL)
{
    var messages = [];
    for (var bucket of sourceFrame._rowMessageBuckets.values()) {
        for (var rowMessage of bucket._messages) {
            var message = rowMessage.message();
            messages.push(String.sprintf("  %d:%d [%s] %s", message.lineNumber(), message.columnNumber(), message.level(), message.text()));
        }
    }
    var name = dumpFullURL ? sourceFrame.uiSourceCode().url() : sourceFrame.uiSourceCode().displayName();
    InspectorTest.addResult("SourceFrame " + name + ": " + messages.length + " message(s)");
    InspectorTest.addResult(messages.join("\n"));
}

InspectorTest.waitUntilPausedNextTime = function(callback)
{
    InspectorTest._waitUntilPausedCallback = InspectorTest.safeWrap(callback);
};

InspectorTest.waitUntilPaused = function(callback)
{
    callback = InspectorTest.safeWrap(callback);

    if (InspectorTest._pausedScriptArguments)
        callback.apply(callback, InspectorTest._pausedScriptArguments);
    else
        InspectorTest._waitUntilPausedCallback = callback;
};

InspectorTest.waitUntilPausedPromise = function()
{
    return new Promise(resolve => InspectorTest.waitUntilPaused(resolve));
}

InspectorTest.waitUntilResumedNextTime = function(callback)
{
    InspectorTest._waitUntilResumedCallback = InspectorTest.safeWrap(callback);
};

InspectorTest.waitUntilResumed = function(callback)
{
    callback = InspectorTest.safeWrap(callback);

    if (!InspectorTest._pausedScriptArguments)
        callback();
    else
        InspectorTest._waitUntilResumedCallback = callback;
};

InspectorTest.resumeExecution = function(callback)
{
    if (UI.panels.sources.paused())
        UI.panels.sources._togglePause();
    InspectorTest.waitUntilResumed(callback);
};

InspectorTest.waitUntilPausedAndDumpStackAndResume = function(callback, options)
{
    InspectorTest.waitUntilPaused(paused);
    InspectorTest.addSniffer(Sources.SourcesPanel.prototype, "_updateDebuggerButtonsAndStatus", setStatus);

    var caption;
    var callFrames;
    var asyncStackTrace;

    function setStatus()
    {
        var statusElement = this.element.querySelector(".paused-message");
        caption = statusElement.deepTextContent();
        if (callFrames)
            step1();
    }

    function paused(frames, reason, breakpointIds, async)
    {
        callFrames = frames;
        asyncStackTrace = async;
        if (typeof caption === "string")
            step1();
    }

    function step1()
    {
        InspectorTest.captureStackTrace(callFrames, asyncStackTrace, options);
        InspectorTest.addResult(InspectorTest.clearSpecificInfoFromStackFrames(caption));
        InspectorTest.deprecatedRunAfterPendingDispatches(step2);
    }

    function step2()
    {
        InspectorTest.resumeExecution(InspectorTest.safeWrap(callback));
    }
};

InspectorTest.stepOver = function()
{
    Promise.resolve().then(function(){UI.panels.sources._stepOver()});
};

InspectorTest.stepInto = function()
{
    Promise.resolve().then(function(){UI.panels.sources._stepInto()});
};

InspectorTest.stepOut = function()
{
    Promise.resolve().then(function(){UI.panels.sources._stepOut()});
};

InspectorTest.togglePause = function()
{
    Promise.resolve().then(function(){UI.panels.sources._togglePause()});
};

InspectorTest.waitUntilPausedAndPerformSteppingActions = function(actions, callback)
{
    callback = InspectorTest.safeWrap(callback);
    InspectorTest.waitUntilPaused(didPause);

    function didPause(callFrames, reason, breakpointIds, asyncStackTrace)
    {
        var action = actions.shift();
        if (action === "Print") {
            InspectorTest.captureStackTrace(callFrames, asyncStackTrace);
            InspectorTest.addResult("");
            while (action === "Print")
                action = actions.shift();
        }

        if (!action) {
            callback();
            return;
        }

        InspectorTest.addResult("Executing " + action + "...");

        switch (action) {
        case "StepInto":
            InspectorTest.stepInto();
            break;
        case "StepOver":
            InspectorTest.stepOver();
            break;
        case "StepOut":
            InspectorTest.stepOut();
            break;
        case "Resume":
            InspectorTest.togglePause();
            break;
        default:
            InspectorTest.addResult("FAIL: Unknown action: " + action);
            callback();
            return;
        }

        InspectorTest.waitUntilResumed(actions.length ? InspectorTest.waitUntilPaused.bind(InspectorTest, didPause) : callback);
    }
};

InspectorTest.captureStackTrace = function(callFrames, asyncStackTrace, options)
{
    InspectorTest.addResult(InspectorTest.captureStackTraceIntoString(callFrames, asyncStackTrace, options));
};

InspectorTest.captureStackTraceIntoString = function(callFrames, asyncStackTrace, options)
{
    var results = [];
    options = options || {};
    function printCallFrames(callFrames, locationFunction, returnValueFunction)
    {
        var printed = 0;
        for (var i = 0; i < callFrames.length; i++) {
            var frame = callFrames[i];
            var location = locationFunction.call(frame);
            var script = location.script();
            var uiLocation = Bindings.debuggerWorkspaceBinding.rawLocationToUILocation(location);
            var isFramework = Bindings.blackboxManager.isBlackboxedRawLocation(location);
            if (options.dropFrameworkCallFrames && isFramework)
                continue;
            var url;
            var lineNumber;
            if (uiLocation && uiLocation.uiSourceCode.project().type() !== Workspace.projectTypes.Debugger) {
                url = uiLocation.uiSourceCode.name();
                lineNumber = uiLocation.lineNumber + 1;
            } else {
                url = Bindings.displayNameForURL(script.sourceURL);
                lineNumber = location.lineNumber + 1;
            }
            var s = (isFramework ? "  * " : "    ") + (printed++) + ") " + frame.functionName + " (" + url + (options.dropLineNumbers ? "" : ":" + lineNumber) + ")";
            results.push(s);
            if (options.printReturnValue && returnValueFunction && returnValueFunction.call(frame))
                results.push("       <return>: " + returnValueFunction.call(frame).description);
            if (frame.functionName === "scheduleTestFunction") {
                var remainingFrames = callFrames.length - 1 - i;
                if (remainingFrames)
                    results.push("    <... skipped remaining frames ...>");
                break;
            }
        }
        return printed;
    }

    function runtimeCallFramePosition()
    {
        return new SDK.DebuggerModel.Location(debuggerModel, this.scriptId, this.lineNumber, this.columnNumber);
    }

    results.push("Call stack:");
    printCallFrames(callFrames, SDK.DebuggerModel.CallFrame.prototype.location, SDK.DebuggerModel.CallFrame.prototype.returnValue);
    while (asyncStackTrace) {
        results.push("    [" + (asyncStackTrace.description || "Async Call") + "]");
        var debuggerModel = SDK.DebuggerModel.fromTarget(SDK.targetManager.mainTarget());
        var printed = printCallFrames(asyncStackTrace.callFrames, runtimeCallFramePosition);
        if (!printed)
            results.pop();
        asyncStackTrace = asyncStackTrace.parent;
    }
    return results.join("\n");
};

InspectorTest.dumpSourceFrameContents = function(sourceFrame)
{
    InspectorTest.addResult("==Source frame contents start==");
    var textEditor = sourceFrame._textEditor;
    for (var i = 0; i < textEditor.linesCount; ++i)
        InspectorTest.addResult(textEditor.line(i));
    InspectorTest.addResult("==Source frame contents end==");
};

InspectorTest._pausedScript = function(callFrames, reason, auxData, breakpointIds, asyncStackTrace)
{
    if (!InspectorTest._quiet)
        InspectorTest.addResult("Script execution paused.");
    var debuggerModel = SDK.DebuggerModel.fromTarget(this.target());
    InspectorTest._pausedScriptArguments = [SDK.DebuggerModel.CallFrame.fromPayloadArray(debuggerModel, callFrames), reason, breakpointIds, asyncStackTrace, auxData];
    if (InspectorTest._waitUntilPausedCallback) {
        var callback = InspectorTest._waitUntilPausedCallback;
        delete InspectorTest._waitUntilPausedCallback;
        setTimeout(() => callback.apply(callback, InspectorTest._pausedScriptArguments));
    }
};

InspectorTest._resumedScript = function()
{
    if (!InspectorTest._quiet)
        InspectorTest.addResult("Script execution resumed.");
    delete InspectorTest._pausedScriptArguments;
    if (InspectorTest._waitUntilResumedCallback) {
        var callback = InspectorTest._waitUntilResumedCallback;
        delete InspectorTest._waitUntilResumedCallback;
        callback();
    }
};

InspectorTest.showUISourceCode = function(uiSourceCode, callback)
{
    var panel = UI.panels.sources;
    panel.showUISourceCode(uiSourceCode);
    var sourceFrame = panel.visibleView;
    if (sourceFrame.loaded)
        callback(sourceFrame);
    else
        InspectorTest.addSniffer(sourceFrame, "onTextEditorContentSet", callback && callback.bind(null, sourceFrame));
};

InspectorTest.showUISourceCodePromise = function(uiSourceCode)
{
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    InspectorTest.showUISourceCode(uiSourceCode, fulfill);
    return promise;
}

InspectorTest.showScriptSource = function(scriptName, callback)
{
    InspectorTest.waitForScriptSource(scriptName, onScriptSource);

    function onScriptSource(uiSourceCode)
    {
        InspectorTest.showUISourceCode(uiSourceCode, callback);
    }
};

InspectorTest.waitForScriptSource = function(scriptName, callback)
{
    var panel = UI.panels.sources;
    var uiSourceCodes = panel._workspace.uiSourceCodes();
    for (var i = 0; i < uiSourceCodes.length; ++i) {
        if (uiSourceCodes[i].project().type() === Workspace.projectTypes.Service)
            continue;
        if (uiSourceCodes[i].name() === scriptName) {
            callback(uiSourceCodes[i]);
            return;
        }
    }

    InspectorTest.addSniffer(Sources.SourcesView.prototype, "_addUISourceCode", InspectorTest.waitForScriptSource.bind(InspectorTest, scriptName, callback));
};

InspectorTest.setBreakpoint = function(sourceFrame, lineNumber, condition, enabled)
{
    if (!sourceFrame._muted)
        sourceFrame._setBreakpoint(lineNumber, 0, condition, enabled);
};

InspectorTest.removeBreakpoint = function(sourceFrame, lineNumber)
{
    sourceFrame._breakpointManager.findBreakpoints(sourceFrame._uiSourceCode, lineNumber)[0].remove();
};

InspectorTest.createNewBreakpoint = function(sourceFrame, lineNumber, condition, enabled)
{
    var promise = new Promise(resolve => InspectorTest.addSniffer(sourceFrame.__proto__, "_breakpointWasSetForTest", resolve));
    sourceFrame._createNewBreakpoint(lineNumber, condition, enabled);
    return promise;
}

InspectorTest.toggleBreakpoint = function(sourceFrame, lineNumber, disableOnly)
{
    if (!sourceFrame._muted)
        sourceFrame._toggleBreakpoint(lineNumber, disableOnly);
};

InspectorTest.waitBreakpointSidebarPane = function(waitUntilResolved)
{
    return new Promise(resolve => InspectorTest.addSniffer(Sources.JavaScriptBreakpointsSidebarPane.prototype, "_didUpdateForTest", resolve)).then(checkIfReady);
    function checkIfReady()
    {
        if (!waitUntilResolved)
            return;
        for (var breakpoint of Bindings.breakpointManager._allBreakpoints()) {
            if (breakpoint._fakePrimaryLocation && breakpoint.enabled())
                return InspectorTest.waitBreakpointSidebarPane();
        }
    }
}

InspectorTest.breakpointsSidebarPaneContent = function()
{
    var paneElement = self.runtime.sharedInstance(Sources.JavaScriptBreakpointsSidebarPane).contentElement;
    var empty = paneElement.querySelector('.gray-info-message');
    if (empty)
        return InspectorTest.textContentWithLineBreaks(empty);
    var entries = Array.from(paneElement.querySelectorAll('.breakpoint-entry'));
    return entries.map(InspectorTest.textContentWithLineBreaks).join('\n');
}

InspectorTest.dumpBreakpointSidebarPane = function(title)
{
    InspectorTest.addResult("Breakpoint sidebar pane " + (title || ""));
    InspectorTest.addResult(InspectorTest.breakpointsSidebarPaneContent());
};

InspectorTest.dumpScopeVariablesSidebarPane = function()
{
    InspectorTest.addResult("Scope variables sidebar pane:");
    var sections = InspectorTest.scopeChainSections();
    for (var i = 0; i < sections.length; ++i) {
        var textContent = InspectorTest.textContentWithLineBreaks(sections[i].element);
        var text = InspectorTest.clearSpecificInfoFromStackFrames(textContent);
        if (text.length > 0)
            InspectorTest.addResult(text);
        if (!sections[i].objectTreeElement().expanded)
            InspectorTest.addResult("    <section collapsed>");
    }
};

InspectorTest.scopeChainSections = function()
{
    var children = self.runtime.sharedInstance(Sources.ScopeChainSidebarPane).contentElement.children;
    var sections = [];
    for (var i = 0; i < children.length; ++i)
        sections.push(children[i]._section);

    return sections;
}

InspectorTest.expandScopeVariablesSidebarPane = function(callback)
{
    // Expand all but the global scope. Expanding global scope takes for too long so we keep it collapsed.
    var sections = InspectorTest.scopeChainSections();
    for (var i = 0; i < sections.length - 1; ++i)
        sections[i].expand();
    InspectorTest.deprecatedRunAfterPendingDispatches(callback);
};

InspectorTest.expandProperties = function(properties, callback)
{
    var index = 0;
    function expandNextPath()
    {
        if (index === properties.length) {
            InspectorTest.safeWrap(callback)();
            return;
        }
        var parentTreeElement = properties[index++];
        var path = properties[index++];
        InspectorTest._expandProperty(parentTreeElement, path, 0, expandNextPath);
    }
    InspectorTest.deprecatedRunAfterPendingDispatches(expandNextPath);
};

InspectorTest._expandProperty = function(parentTreeElement, path, pathIndex, callback)
{
    if (pathIndex === path.length) {
        InspectorTest.addResult("Expanded property: " + path.join("."));
        callback();
        return;
    }
    var name = path[pathIndex++];
    var propertyTreeElement = InspectorTest._findChildPropertyTreeElement(parentTreeElement, name);
    if (!propertyTreeElement) {
       InspectorTest.addResult("Failed to expand property: " + path.slice(0, pathIndex).join("."));
       InspectorTest.completeDebuggerTest();
       return;
    }
    propertyTreeElement.expand();
    InspectorTest.deprecatedRunAfterPendingDispatches(InspectorTest._expandProperty.bind(InspectorTest, propertyTreeElement, path, pathIndex, callback));
};

InspectorTest._findChildPropertyTreeElement = function(parent, childName)
{
    var children = parent.children();
    for (var i = 0; i < children.length; i++) {
        var treeElement = children[i];
        var property = treeElement.property;
        if (property.name === childName)
            return treeElement;
    }
};

InspectorTest.setQuiet = function(quiet)
{
    InspectorTest._quiet = quiet;
};

InspectorTest.queryScripts = function(filter)
{
    var scripts = [];
    for (var scriptId in InspectorTest.debuggerModel._scripts) {
        var script = InspectorTest.debuggerModel._scripts[scriptId];
        if (!filter || filter(script))
            scripts.push(script);
    }
    return scripts;
};

InspectorTest.createScriptMock = function(url, startLine, startColumn, isContentScript, source, target, preRegisterCallback)
{
    target = target || SDK.targetManager.mainTarget();
    var debuggerModel = SDK.DebuggerModel.fromTarget(target);
    var scriptId = ++InspectorTest._lastScriptId + "";
    var lineCount = source.computeLineEndings().length;
    var endLine = startLine + lineCount - 1;
    var endColumn = lineCount === 1 ? startColumn + source.length : source.length - source.computeLineEndings()[lineCount - 2];
    var hasSourceURL = !!source.match(/\/\/#\ssourceURL=\s*(\S*?)\s*$/m) || !!source.match(/\/\/@\ssourceURL=\s*(\S*?)\s*$/m);
    var script = new SDK.Script(debuggerModel, scriptId, url, startLine, startColumn, endLine, endColumn, 0, "", isContentScript, false, undefined, hasSourceURL);
    script.requestContent = function()
    {
        var trimmedSource = SDK.Script._trimSourceURLComment(source);
        return Promise.resolve(trimmedSource);
    };
    if (preRegisterCallback)
        preRegisterCallback(script);
    debuggerModel._registerScript(script);
    return script;
};

InspectorTest._lastScriptId = 0;

InspectorTest.checkRawLocation = function(script, lineNumber, columnNumber, location)
{
    InspectorTest.assertEquals(script.scriptId, location.scriptId, "Incorrect scriptId");
    InspectorTest.assertEquals(lineNumber, location.lineNumber, "Incorrect lineNumber");
    InspectorTest.assertEquals(columnNumber, location.columnNumber, "Incorrect columnNumber");
};

InspectorTest.checkUILocation = function(uiSourceCode, lineNumber, columnNumber, location)
{
    InspectorTest.assertEquals(uiSourceCode, location.uiSourceCode, "Incorrect uiSourceCode, expected '" + (uiSourceCode ? uiSourceCode.url() : null) + "'," +
                                                                    " but got '" + (location.uiSourceCode ? location.uiSourceCode.url() : null) + "'");
    InspectorTest.assertEquals(lineNumber, location.lineNumber, "Incorrect lineNumber, expected '" + lineNumber + "', but got '" + location.lineNumber + "'");
    InspectorTest.assertEquals(columnNumber, location.columnNumber, "Incorrect columnNumber, expected '" + columnNumber + "', but got '" + location.columnNumber + "'");
};

InspectorTest.scriptFormatter = function()
{
    return self.runtime.allInstances(Sources.SourcesView.EditorAction).then(function(editorActions) {
        for (var i = 0; i < editorActions.length; ++i) {
            if (editorActions[i] instanceof Sources.ScriptFormatterEditorAction)
                return editorActions[i];
        }
        return null;
    });
};

InspectorTest.waitForExecutionContextInTarget = function(target, callback)
{
    if (target.runtimeModel.executionContexts().length) {
        callback(target.runtimeModel.executionContexts()[0]);
        return;
    }
    target.runtimeModel.addEventListener(SDK.RuntimeModel.Events.ExecutionContextCreated, contextCreated);

    function contextCreated()
    {
        target.runtimeModel.removeEventListener(SDK.RuntimeModel.Events.ExecutionContextCreated, contextCreated);
        callback(target.runtimeModel.executionContexts()[0]);
    }
}

InspectorTest.selectThread = function(target)
{
    var threadsPane = self.runtime.sharedInstance(Sources.ThreadsSidebarPane);
    threadsPane._list.selectItem(target.model(SDK.DebuggerModel));
}

InspectorTest.evaluateOnCurrentCallFrame = function(code)
{
    return new Promise(succ => InspectorTest.debuggerModel.evaluateOnSelectedCallFrame(code, "console", false, true, false, false, InspectorTest.safeWrap(succ)));
}

InspectorTest.prepareSourceFrameForBreakpointTest = function(sourceFrame)
{
    var symbol = Symbol('waitedDecorations');
    sourceFrame[symbol] = 0;
    InspectorTest.addSniffer(sourceFrame.__proto__, "_willAddInlineDecorationsForTest", () => sourceFrame[symbol]++, true);
    InspectorTest.addSniffer(sourceFrame.__proto__, "_didAddInlineDecorationsForTest", (updateWasScheduled) => {
        sourceFrame[symbol]--;
        if (!updateWasScheduled)
            sourceFrame._breakpointDecorationsUpdatedForTest();
    }, true);
    sourceFrame._waitingForPossibleLocationsForTest = () => !!sourceFrame[symbol];
}

InspectorTest.waitJavaScriptSourceFrameBreakpoints = function(sourceFrame, inline)
{
    if (!sourceFrame._waitingForPossibleLocationsForTest) {
        InspectorTest.addResult("Error: source frame should be prepared with InspectorTest.prepareSourceFrameForBreakpointTest function.");
        InspectorTest.completeTest();
        return;
    }
    return waitUpdate().then(checkIfReady);
    function waitUpdate()
    {
        return new Promise(resolve => InspectorTest.addSniffer(sourceFrame.__proto__, "_breakpointDecorationsUpdatedForTest", resolve));
    }
    function checkIfReady()
    {
        if (sourceFrame._waitingForPossibleLocationsForTest())
            return waitUpdate().then(checkIfReady);
        for (var breakpoint of Bindings.breakpointManager._allBreakpoints()) {
            if (breakpoint._fakePrimaryLocation && breakpoint.enabled())
                return waitUpdate().then(checkIfReady);
        }
        return Promise.resolve();
    }
}

InspectorTest.dumpJavaScriptSourceFrameBreakpoints = function(sourceFrame)
{
    var textEditor = sourceFrame._textEditor;
    for (var lineNumber = 0; lineNumber < textEditor.linesCount; ++lineNumber) {
        if (!textEditor.hasLineClass(lineNumber, "cm-breakpoint"))
            continue;
        var disabled = textEditor.hasLineClass(lineNumber, "cm-breakpoint-disabled");
        var conditional = textEditor.hasLineClass(lineNumber, "cm-breakpoint-conditional")
        InspectorTest.addResult("breakpoint at " + lineNumber + (disabled ? " disabled" : "") + (conditional ? " conditional" : ""));

        var range = new Common.TextRange(lineNumber, 0, lineNumber, textEditor.line(lineNumber).length);
        var bookmarks = textEditor.bookmarks(range, Sources.JavaScriptSourceFrame.BreakpointDecoration._bookmarkSymbol);
        bookmarks = bookmarks.filter(bookmark => !!bookmark.position());
        bookmarks.sort((bookmark1, bookmark2) => bookmark1.position().startColumn - bookmark2.position().startColumn);
        for (var bookmark of bookmarks) {
            var position = bookmark.position();
            var element = bookmark[Sources.JavaScriptSourceFrame.BreakpointDecoration._elementSymbolForTest];
            var disabled = element.classList.contains("cm-inline-disabled");
            var conditional = element.classList.contains("cm-inline-conditional");
            InspectorTest.addResult("  inline breakpoint at (" + position.startLine + ", " + position.startColumn + ")" + (disabled ? " disabled" : "") + (conditional ? " conditional" : ""));
        }
    }
}

InspectorTest.clickJavaScriptSourceFrameBreakpoint = function(sourceFrame, lineNumber, index, next)
{
    var textEditor = sourceFrame._textEditor;
    var lineLength = textEditor.line(lineNumber).length;
    var lineRange = new Common.TextRange(lineNumber, 0, lineNumber, lineLength);
    var bookmarks = textEditor.bookmarks(lineRange, Sources.JavaScriptSourceFrame.BreakpointDecoration._bookmarkSymbol);
    bookmarks.sort((bookmark1, bookmark2) => bookmark1.position().startColumn - bookmark2.position().startColumn);
    var bookmark = bookmarks[index];
    if (bookmark) {
        bookmark[Sources.JavaScriptSourceFrame.BreakpointDecoration._elementSymbolForTest].click();
    } else {
        InspectorTest.addResult(`Could not click on Javascript breakpoint - lineNumber: ${lineNumber}, index: ${index}`);
        next();
    }
}

};
