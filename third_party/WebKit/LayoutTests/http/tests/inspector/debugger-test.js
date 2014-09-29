var initialize_DebuggerTest = function() {

InspectorTest.startDebuggerTest = function(callback, quiet)
{
    console.assert(WebInspector.debuggerModel.debuggerEnabled(), "Debugger has to be enabled");
    if (quiet !== undefined)
        InspectorTest._quiet = quiet;
    WebInspector.inspectorView.showPanel("sources");

    InspectorTest.addSniffer(WebInspector.debuggerModel, "_pausedScript", InspectorTest._pausedScript, true);
    InspectorTest.addSniffer(WebInspector.debuggerModel, "_resumedScript", InspectorTest._resumedScript, true);
    InspectorTest.safeWrap(callback)();
};

InspectorTest.completeDebuggerTest = function()
{
    WebInspector.breakpointManager.setBreakpointsActive(true);
    InspectorTest.resumeExecution(InspectorTest.completeTest.bind(InspectorTest));
};

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
    InspectorTest.evaluateInConsole("setTimeout(testFunction, 0)");
    InspectorTest.addResult("Set timer for test function.");
};

InspectorTest.runTestFunctionAndWaitUntilPaused = function(callback)
{
    InspectorTest.runTestFunction();
    InspectorTest.waitUntilPaused(callback);
};

InspectorTest.runAsyncCallStacksTest = function(totalDebuggerStatements, maxAsyncCallStackDepth)
{
    InspectorTest.setQuiet(true);
    InspectorTest.startDebuggerTest(step1);

    function step1()
    {
        DebuggerAgent.setAsyncCallStackDepth(maxAsyncCallStackDepth, step2);
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
    if (WebInspector.panels.sources.paused())
        WebInspector.panels.sources.togglePause();
    InspectorTest.waitUntilResumed(callback);
};

InspectorTest.waitUntilPausedAndDumpStackAndResume = function(callback, options)
{
    InspectorTest.waitUntilPaused(paused);
    InspectorTest.addSniffer(WebInspector.CallStackSidebarPane.prototype, "setStatus", setStatus);

    var caption;
    var callFrames;
    var asyncStackTrace;

    function setStatus(status)
    {
        if (typeof status === "string")
            caption = status;
        else
            caption = status.textContent;
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
        InspectorTest.addResult(caption);
        InspectorTest.runAfterPendingDispatches(step2);
    }

    function step2()
    {
        InspectorTest.resumeExecution(InspectorTest.safeWrap(callback));
    }
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
            callback()
            return;
        }

        InspectorTest.addResult("Executing " + action + "...");

        switch (action) {
        case "StepInto":
            WebInspector.panels.sources._stepIntoButton.element.click();
            break;
        case "StepOver":
            WebInspector.panels.sources._stepOverButton.element.click();
            break;
        case "StepOut":
            WebInspector.panels.sources._stepOutButton.element.click();
            break;
        case "Resume":
            WebInspector.panels.sources.togglePause();
            break;
        default:
            InspectorTest.addResult("FAIL: Unknown action: " + action);
            callback()
            return;
        }

        InspectorTest.waitUntilResumed(InspectorTest.waitUntilPaused.bind(InspectorTest, didPause));
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

    function printCallFrames(callFrames)
    {
        var printed = 0;
        for (var i = 0; i < callFrames.length; i++) {
            var frame = callFrames[i];
            var script = WebInspector.debuggerModel.scriptForId(frame.location().scriptId);
            var isFramework = WebInspector.BlackboxSupport.isBlackboxedURL(script.sourceURL);
            if (options.dropFrameworkCallFrames && isFramework)
                continue;
            var url;
            var lineNumber;
            if (script) {
                url = WebInspector.displayNameForURL(script.sourceURL);
                lineNumber = frame.location().lineNumber + 1;
            } else {
                url = "(internal script)";
                lineNumber = "(line number)";
            }
            var s = (isFramework ? "  * " : "    ") + (printed++) + ") " + frame.functionName + " (" + url + (options.dropLineNumbers ? "" : ":" + lineNumber) + ")";
            results.push(s);
            if (options.printReturnValue && frame.returnValue())
                results.push("       <return>: " + frame.returnValue().description);
        }
        return printed;
    }

    results.push("Call stack:");
    printCallFrames(callFrames);

    while (asyncStackTrace) {
        results.push("    [" + (asyncStackTrace.description || "Async Call") + "]");
        var printed = printCallFrames(WebInspector.DebuggerModel.CallFrame.fromPayloadArray(WebInspector.targetManager.mainTarget(), asyncStackTrace.callFrames));
        if (!printed)
            results.pop();
        if (asyncStackTrace.callFrames.peekLast().functionName === "testFunction")
            break;
        asyncStackTrace = asyncStackTrace.asyncStackTrace;
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
    InspectorTest._pausedScriptArguments = [WebInspector.DebuggerModel.CallFrame.fromPayloadArray(WebInspector.targetManager.mainTarget(), callFrames), reason, breakpointIds, asyncStackTrace, auxData];
    if (InspectorTest._waitUntilPausedCallback) {
        var callback = InspectorTest._waitUntilPausedCallback;
        delete InspectorTest._waitUntilPausedCallback;
        callback.apply(callback, InspectorTest._pausedScriptArguments);
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
    var panel = WebInspector.inspectorView.showPanel("sources");
    panel.showUISourceCode(uiSourceCode);
    var sourceFrame = panel.visibleView;
    if (sourceFrame.loaded)
        callback(sourceFrame);
    else
        InspectorTest.addSniffer(sourceFrame, "onTextEditorContentLoaded", callback && callback.bind(null, sourceFrame));
};

InspectorTest.showScriptSource = function(scriptName, callback)
{
    InspectorTest.waitForScriptSource(scriptName, function(uiSourceCode) { InspectorTest.showUISourceCode(uiSourceCode, callback); });
};

InspectorTest.waitForScriptSource = function(scriptName, callback)
{
    var panel = WebInspector.inspectorView.showPanel("sources");
    var uiSourceCodes = panel._workspace.uiSourceCodes();
    for (var i = 0; i < uiSourceCodes.length; ++i) {
        if (uiSourceCodes[i].name() === scriptName) {
            callback(uiSourceCodes[i]);
            return;
        }
    }

    InspectorTest.addSniffer(WebInspector.SourcesView.prototype, "_addUISourceCode", InspectorTest.waitForScriptSource.bind(InspectorTest, scriptName, callback));
};

InspectorTest.dumpNavigatorView = function(navigatorView, id, prefix)
{
    InspectorTest.addResult(prefix + "Dumping ScriptsNavigator " + id + " tab:");
    dumpNavigatorTreeOutline(prefix, navigatorView._scriptsTree);

    function dumpNavigatorTreeElement(prefix, treeElement)
    {
        InspectorTest.addResult(prefix + treeElement.titleText);
        for (var i = 0; i < treeElement.children.length; ++i)
            dumpNavigatorTreeElement(prefix + "  ", treeElement.children[i]);
    }

    function dumpNavigatorTreeOutline(prefix, treeOutline)
    {
        for (var i = 0; i < treeOutline.children.length; ++i)
            dumpNavigatorTreeElement(prefix + "  ", treeOutline.children[i]);
    }
};

InspectorTest.setBreakpoint = function(sourceFrame, lineNumber, condition, enabled)
{
    if (!sourceFrame._muted)
        sourceFrame._setBreakpoint(lineNumber, 0, condition, enabled);
};

InspectorTest.removeBreakpoint = function(sourceFrame, lineNumber)
{
    sourceFrame._breakpointManager.findBreakpointOnLine(sourceFrame._uiSourceCode, lineNumber).remove();
};

InspectorTest.dumpBreakpointSidebarPane = function(title)
{
    var paneElement = WebInspector.panels.sources.sidebarPanes.jsBreakpoints.listElement;
    InspectorTest.addResult("Breakpoint sidebar pane " + (title || ""));
    InspectorTest.addResult(InspectorTest.textContentWithLineBreaks(paneElement));
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
    InspectorTest.runAfterPendingDispatches(expandNextPath);
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
    InspectorTest.runAfterPendingDispatches(InspectorTest._expandProperty.bind(InspectorTest, propertyTreeElement, path, pathIndex, callback));
};

InspectorTest._findChildPropertyTreeElement = function(parent, childName)
{
    var children = parent.children;
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
    for (var scriptId in WebInspector.debuggerModel._scripts) {
        var script = WebInspector.debuggerModel._scripts[scriptId];
        if (!filter || filter(script))
            scripts.push(script);
    }
    return scripts;
};

InspectorTest.createScriptMock = function(url, startLine, startColumn, isContentScript, source, target, preRegisterCallback)
{
    target = target || WebInspector.targetManager.mainTarget();
    var scriptId = ++InspectorTest._lastScriptId;
    var lineCount = source.lineEndings().length;
    var endLine = startLine + lineCount - 1;
    var endColumn = lineCount === 1 ? startColumn + source.length : source.length - source.lineEndings()[lineCount - 2];
    var hasSourceURL = !!source.match(/\/\/#\ssourceURL=\s*(\S*?)\s*$/m) || !!source.match(/\/\/@\ssourceURL=\s*(\S*?)\s*$/m);
    var script = new WebInspector.Script(target, scriptId, url, startLine, startColumn, endLine, endColumn, isContentScript, null, hasSourceURL);
    script.requestContent = function(callback)
    {
        var trimmedSource = WebInspector.Script._trimSourceURLComment(source);
        callback(trimmedSource, false, "text/javascript");
    };
    if (preRegisterCallback)
        preRegisterCallback(script);
    target.debuggerModel._registerScript(script);
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
    InspectorTest.assertEquals(uiSourceCode, location.uiSourceCode, "Incorrect uiSourceCode, expected '" + (uiSourceCode ? uiSourceCode.originURL() : null) + "'," +
                                                                    " but got '" + (location.uiSourceCode ? location.uiSourceCode.originURL() : null) + "'");
    InspectorTest.assertEquals(lineNumber, location.lineNumber, "Incorrect lineNumber, expected '" + lineNumber + "', but got '" + location.lineNumber + "'");
    InspectorTest.assertEquals(columnNumber, location.columnNumber, "Incorrect columnNumber, expected '" + columnNumber + "', but got '" + location.columnNumber + "'");
};

InspectorTest.scriptFormatter = function()
{
    return self.runtime.instancesPromise(WebInspector.SourcesView.EditorAction).then(function(editorActions) {
        for (var i = 0; i < editorActions.length; ++i) {
            if (editorActions[i] instanceof WebInspector.ScriptFormatterEditorAction)
                return editorActions[i];
        }
        return null;
    });
};

};
