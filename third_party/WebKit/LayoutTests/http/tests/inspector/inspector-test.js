var initialize_InspectorTest = function() {

var results = [];

function consoleOutputHook(messageType)
{
    InspectorTest.addResult(messageType + ": " + Array.prototype.slice.call(arguments, 1));
}

window._originalConsoleLog = console.log.bind(console);

console.log = consoleOutputHook.bind(InspectorTest, "log");
console.error = consoleOutputHook.bind(InspectorTest, "error");
console.info = consoleOutputHook.bind(InspectorTest, "info");
console.assert = function(condition, object)
{
    if (condition)
        return;
    var message = "Assertion failed: " + (typeof object !== "undefined" ? object : "");
    InspectorTest.addResult(new Error(message).stack);
}

InspectorTest.startDumpingProtocolMessages = function()
{
    InspectorBackendClass.Connection.prototype._dumpProtocolMessage = testRunner.logToStderr.bind(testRunner);
    InspectorBackendClass.Options.dumpInspectorProtocolMessages = 1;
}

InspectorTest.completeTest = function()
{
    InspectorTest.RuntimeAgent.evaluate("completeTest(\"" + escape(JSON.stringify(results)) + "\")", "test");
}

InspectorTest.flushResults = function()
{
    InspectorTest.RuntimeAgent.evaluate("flushResults(\"" + escape(JSON.stringify(results)) + "\")", "test");
    results = [];
}

InspectorTest.evaluateInPage = function(code, callback)
{
    callback = InspectorTest.safeWrap(callback);

    function mycallback(error, result, wasThrown)
    {
        if (!error)
            callback(InspectorTest.runtimeModel.createRemoteObject(result), wasThrown);
    }
    InspectorTest.RuntimeAgent.evaluate(code, "console", false, mycallback);
}

InspectorTest.evaluateInPageWithTimeout = function(code)
{
    // FIXME: we need a better way of waiting for chromium events to happen
    InspectorTest.evaluateInPage("setTimeout(unescape('" + escape(code) + "'), 1)");
}

var lastEvalId = 0;
var pendingEvalRequests = {};

InspectorTest.invokePageFunctionAsync = function(functionName, callback)
{
    var id = ++lastEvalId;
    pendingEvalRequests[id] = InspectorTest.safeWrap(callback);
    var asyncEvalWrapper = function(callId, functionName)
    {
        function evalCallback(result)
        {
            testRunner.evaluateInWebInspector(evalCallbackCallId, "InspectorTest.didInvokePageFunctionAsync(" + callId + ", " + JSON.stringify(result) + ");");
        }
        try {
            eval(functionName + "(" + evalCallback + ")");
        } catch(e) {
            console.error(e);
            evalCallback(String(e));
        }
    }
    InspectorTest.evaluateInPage("(" + asyncEvalWrapper.toString() + ")(" + id + ", unescape('" + escape(functionName) + "'))");
}

InspectorTest.didInvokePageFunctionAsync = function(callId, value)
{
    var callback = pendingEvalRequests[callId];
    if (!callback) {
        InspectorTest.addResult("Missing callback for async eval " + callId + ", perhaps callback invoked twice?");
        return;
    }
    delete pendingEvalRequests[callId];
    callback(value);
}

InspectorTest.check = function(passCondition, failureText)
{
    if (!passCondition)
        InspectorTest.addResult("FAIL: " + failureText);
}

InspectorTest.addResult = function(text)
{
    results.push(String(text));
}

InspectorTest.addResults = function(textArray)
{
    if (!textArray)
        return;
    for (var i = 0, size = textArray.length; i < size; ++i)
        InspectorTest.addResult(textArray[i]);
}

window.onerror = function (message, filename, lineno, colno, error)
{
    InspectorTest.addResult("Uncaught exception in inspector front-end: " + message + " [" + error.stack + "]");
    InspectorTest.completeTest();
}

InspectorTest.formatters = {};

InspectorTest.formatters.formatAsTypeName = function(value)
{
    return "<" + typeof value + ">";
}

InspectorTest.formatters.formatAsRecentTime = function(value)
{
    if (typeof value !== "object" || !(value instanceof Date))
        return InspectorTest.formatAsTypeName(value);
    var delta = Date.now() - value;
    return 0 <= delta && delta < 30 * 60 * 1000 ? "<plausible>" : value;
}

InspectorTest.formatters.formatAsURL = function(value)
{
    if (!value)
        return value;
    var lastIndex = value.lastIndexOf("inspector/");
    if (lastIndex < 0)
        return value;
    return ".../" + value.substr(lastIndex);
}

InspectorTest.addObject = function(object, customFormatters, prefix, firstLinePrefix)
{
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    InspectorTest.addResult(firstLinePrefix + "{");
    var propertyNames = Object.keys(object);
    propertyNames.sort();
    for (var i = 0; i < propertyNames.length; ++i) {
        var prop = propertyNames[i];
        if (!object.hasOwnProperty(prop))
            continue;
        var prefixWithName = "    " + prefix + prop + " : ";
        var propValue = object[prop];
        if (customFormatters && customFormatters[prop]) {
            var formatterName = customFormatters[prop];
            if (formatterName !== "skip") {
                var formatter = InspectorTest.formatters[formatterName];
                InspectorTest.addResult(prefixWithName + formatter(propValue));
            }
        } else
            InspectorTest.dump(propValue, customFormatters, "    " + prefix, prefixWithName);
    }
    InspectorTest.addResult(prefix + "}");
}

InspectorTest.addArray = function(array, customFormatters, prefix, firstLinePrefix)
{
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    InspectorTest.addResult(firstLinePrefix + "[");
    for (var i = 0; i < array.length; ++i)
        InspectorTest.dump(array[i], customFormatters, prefix + "    ");
    InspectorTest.addResult(prefix + "]");
}

InspectorTest.dumpDeepInnerHTML = function(element)
{
    function innerHTML(prefix, element)
    {
        var openTag = [];
        if (element.nodeType === Node.TEXT_NODE) {
            if (!element.parentElement || element.parentElement.nodeName !== "STYLE")
                InspectorTest.addResult(element.nodeValue);
            return;
        }
        openTag.push("<" + element.nodeName);
        var attrs = element.attributes;
        for (var i = 0; attrs && i < attrs.length; ++i) {
            openTag.push(attrs[i].name + "=" + attrs[i].value);
        }
        openTag.push(">");
        InspectorTest.addResult(prefix + openTag.join(" "));
        for (var child = element.firstChild; child; child = child.nextSibling)
            innerHTML(prefix + "    ", child);
        if (element.shadowRoot)
            innerHTML(prefix + "    ", element.shadowRoot);
        InspectorTest.addResult(prefix + "</" + element.nodeName + ">");
    }
    innerHTML("", element)
}

InspectorTest.dump = function(value, customFormatters, prefix, prefixWithName)
{
    prefixWithName = prefixWithName || prefix;
    if (prefixWithName && prefixWithName.length > 80) {
        InspectorTest.addResult(prefixWithName + "was skipped due to prefix length limit");
        return;
    }
    if (value === null)
        InspectorTest.addResult(prefixWithName + "null");
    else if (value && value.constructor && value.constructor.name === "Array")
        InspectorTest.addArray(value, customFormatters, prefix, prefixWithName);
    else if (typeof value === "object")
        InspectorTest.addObject(value, customFormatters, prefix, prefixWithName);
    else if (typeof value === "string")
        InspectorTest.addResult(prefixWithName + "\"" + value + "\"");
    else
        InspectorTest.addResult(prefixWithName + value);
}

InspectorTest.dumpDataGrid = function(dataGrid)
{
    InspectorTest.addResult(InspectorTest.dumpDataGridIntoString(dataGrid));
}

InspectorTest.dumpDataGridIntoString = function(dataGrid)
{
    var tableElement = dataGrid.element;
    var textRows = [];
    var textWidths = [];
    var rows = tableElement.getElementsByTagName("tr");
    for (var i = 0, row; row = rows[i]; ++i) {
        if (!row.offsetHeight || !row.textContent)
            continue;
        var textCols = [];
        var cols = row.getElementsByTagName("td");
        for (var j = 0, col; col = cols[j]; ++j) {
            if (!col.offsetHeight)
                continue;
            var index = textCols.length;
            var content = col.textContent || (col.firstChild && col.firstChild.title) || "";
            var text = padding(col) + content;
            textWidths[index] = Math.max(textWidths[index] || 0, text.length);
            textCols[index] = text;
        }
        if (textCols.length)
            textRows.push(textCols);
    }

    function padding(target)
    {
        var cell = target.enclosingNodeOrSelfWithNodeName("td");
        if (!cell.classList.contains("disclosure"))
            return "";
        var node = dataGrid.dataGridNodeFromNode(target);
        var spaces = (node ? node.depth : 0) * 2;
        return Array(spaces + 1).join(" ");
    }

    function alignText(text, width)
    {
        var spaces = width - text.length;
        return text + Array(spaces + 1).join(" ");;
    }

    var output = [];
    for (var i = 0; i < textRows.length; ++i) {
        var line = "";
        for (var j = 0; j < textRows[i].length; ++j) {
            if (j)
                line += " | ";
            line += alignText(textRows[i][j], textWidths[j]);
        }
        line += "|";
        output.push(line);
    }
    return output.join("\n");
}

InspectorTest.assertGreaterOrEqual = function(a, b, message)
{
    if (a < b)
        InspectorTest.addResult("FAILED: " + (message ? message + ": " : "") + a + " < " + b);
}

InspectorTest.navigate = function(url, callback)
{
    InspectorTest._pageLoadedCallback = InspectorTest.safeWrap(callback);
    InspectorTest.evaluateInPage("window.location.replace('" + url + "')");
}

InspectorTest.hardReloadPage = function(callback, scriptToEvaluateOnLoad, scriptPreprocessor)
{
    InspectorTest._innerReloadPage(true, callback, scriptToEvaluateOnLoad, scriptPreprocessor);
}

InspectorTest.reloadPage = function(callback, scriptToEvaluateOnLoad, scriptPreprocessor)
{
    InspectorTest._innerReloadPage(false, callback, scriptToEvaluateOnLoad, scriptPreprocessor);
}

InspectorTest._innerReloadPage = function(hardReload, callback, scriptToEvaluateOnLoad, scriptPreprocessor)
{
    InspectorTest._pageLoadedCallback = InspectorTest.safeWrap(callback);

    if (WebInspector.panels.network)
        WebInspector.panels.network._networkLogView.reset();
    InspectorTest.PageAgent.reload(hardReload, scriptToEvaluateOnLoad, scriptPreprocessor);
}

InspectorTest.pageLoaded = function()
{
    InspectorTest.addResult("Page reloaded.");
    if (InspectorTest._pageLoadedCallback) {
        var callback = InspectorTest._pageLoadedCallback;
        delete InspectorTest._pageLoadedCallback;
        callback();
    }
}

InspectorTest.runWhenPageLoads = function(callback)
{
    var oldCallback = InspectorTest._pageLoadedCallback;
    function chainedCallback()
    {
        if (oldCallback)
            oldCallback();
        callback();
    }
    InspectorTest._pageLoadedCallback = InspectorTest.safeWrap(chainedCallback);
}

InspectorTest.runAfterPendingDispatches = function(callback)
{
    var barrier = new CallbackBarrier();
    var targets = WebInspector.targetManager.targets();
    for (var i = 0; i < targets.length; ++i)
        targets[i]._connection.runAfterPendingDispatches(barrier.createCallback());
    barrier.callWhenDone(InspectorTest.safeWrap(callback));
}

InspectorTest.createKeyEvent = function(keyIdentifier, ctrlKey, altKey, shiftKey, metaKey)
{
    var evt = document.createEvent("KeyboardEvent");
    evt.initKeyboardEvent("keydown", true /* can bubble */, true /* can cancel */, null /* view */, keyIdentifier, "", ctrlKey, altKey, shiftKey, metaKey);
    return evt;
}

InspectorTest.runTestSuite = function(testSuite)
{
    var testSuiteTests = testSuite.slice();

    function runner()
    {
        if (!testSuiteTests.length) {
            InspectorTest.completeTest();
            return;
        }
        var nextTest = testSuiteTests.shift();
        InspectorTest.addResult("");
        InspectorTest.addResult("Running: " + /function\s([^(]*)/.exec(nextTest)[1]);
        InspectorTest.safeWrap(nextTest)(runner);
    }
    runner();
}

InspectorTest.assertEquals = function(expected, found, message)
{
    if (expected === found)
        return;

    var error;
    if (message)
        error = "Failure (" + message + "):";
    else
        error = "Failure:";
    throw new Error(error + " expected <" + expected + "> found <" + found + ">");
}

InspectorTest.assertTrue = function(found, message)
{
    InspectorTest.assertEquals(true, !!found, message);
}

InspectorTest.safeWrap = function(func, onexception)
{
    function result()
    {
        if (!func)
            return;
        var wrapThis = this;
        try {
            return func.apply(wrapThis, arguments);
        } catch(e) {
            InspectorTest.addResult("Exception while running: " + func + "\n" + (e.stack || e));
            if (onexception)
                InspectorTest.safeWrap(onexception)();
            else
                InspectorTest.completeTest();
        }
    }
    return result;
}

InspectorTest.addSniffer = function(receiver, methodName, override, opt_sticky)
{
    override = InspectorTest.safeWrap(override);

    var original = receiver[methodName];
    if (typeof original !== "function")
        throw ("Cannot find method to override: " + methodName);

    receiver[methodName] = function(var_args) {
        try {
            var result = original.apply(this, arguments);
        } finally {
            if (!opt_sticky)
                receiver[methodName] = original;
        }
        // In case of exception the override won't be called.
        try {
            Array.prototype.push.call(arguments, result);
            override.apply(this, arguments);
        } catch (e) {
            throw ("Exception in overriden method '" + methodName + "': " + e);
        }
        return result;
    };
}

InspectorTest.addConsoleSniffer = function(override, opt_sticky)
{
    InspectorTest.addSniffer(WebInspector.ConsoleModel.prototype, "addMessage", override, opt_sticky);
}

InspectorTest.override = function(receiver, methodName, override, opt_sticky)
{
    override = InspectorTest.safeWrap(override);

    var original = receiver[methodName];
    if (typeof original !== "function")
        throw ("Cannot find method to override: " + methodName);

    receiver[methodName] = function(var_args) {
        try {
            try {
                var result = override.apply(this, arguments);
            } catch (e) {
                throw ("Exception in overriden method '" + methodName + "': " + e);
            }
        } finally {
            if (!opt_sticky)
                receiver[methodName] = original;
        }
        return result;
    };

    return original;
}

InspectorTest.textContentWithLineBreaks = function(node)
{
    function padding(currentNode)
    {
        var result = 0;
        while (currentNode && currentNode !== node) {
            if (currentNode.nodeName === "OL")
                ++result;
            currentNode = currentNode.parentNode;
        }
        return Array(result * 4 + 1).join(" ");
    }

    var buffer = "";
    var currentNode = node;
    while (currentNode = currentNode.traverseNextNode(node)) {
        if (currentNode.nodeType === Node.TEXT_NODE) {
            buffer += currentNode.nodeValue;
        } else if (currentNode.nodeName === "LI" || currentNode.nodeName === "TR") {
            buffer += "\n" + padding(currentNode);
        } else if (currentNode.nodeName === "STYLE") {
            currentNode = currentNode.traverseNextNode(node);
            continue;
        } else if (currentNode.classList && currentNode.classList.contains("console-message")) {
            buffer += "\n\n";
        }
    }
    return buffer;
}

InspectorTest.textContentWithoutStyles = function(node)
{
    var buffer = "";
    var currentNode = node;
    while (currentNode = currentNode.traverseNextNode(node)) {
        if (currentNode.nodeType === Node.TEXT_NODE)
            buffer += currentNode.nodeValue;
        else if (currentNode.nodeName === "STYLE")
            currentNode = currentNode.traverseNextNode(node);
    }
    return buffer;
}

InspectorTest.clearSpecificInfoFromStackFrames = function(text)
{
    var buffer = text.replace(/\(file:\/\/\/(?:[^)]+\)|[\w\/:-]+)/g, "(...)");
    buffer = buffer.replace(/\(<anonymous>:[^)]+\)/g, "(...)");
    buffer = buffer.replace(/VM\d+/g, "VM");
    buffer = buffer.replace(/\s*at[^()]+\(native\)/g, "");
    return buffer.replace(/\s*at Object.InjectedScript.[^)]+\)/g, "");
}

InspectorTest.hideInspectorView = function()
{
    WebInspector.inspectorView.element.setAttribute("style", "display:none !important");
}

InspectorTest.StringOutputStream = function(callback)
{
    this._callback = callback;
    this._buffer = "";
};

InspectorTest.StringOutputStream.prototype = {
    open: function(fileName, callback)
    {
        callback(true);
    },

    write: function(chunk, callback)
    {
        this._buffer += chunk;
        if (callback)
            callback(this);
    },

    close: function()
    {
        this._callback(this._buffer);
    }
};

InspectorTest.MockSetting = function(value)
{
    this._value = value;
};

InspectorTest.MockSetting.prototype = {
    get: function() {
        return this._value;
    },

    set: function(value) {
        this._value = value;
    }
};


/**
 * @constructor
 * @param {!string} dirPath
 * @param {!string} name
 * @param {!function(?WebInspector.TempFile)} callback
 */
InspectorTest.TempFileMock = function(dirPath, name)
{
    this._chunks = [];
    this._name = name;
    this._size = 0;
}

InspectorTest.TempFileMock.prototype = {
    /**
     * @param {!Array.<string>} chunks
     * @param {!function(boolean)} callback
     */
    write: function(chunks, callback)
    {
        for (var i = 0; i < chunks.length; ++i)
            this._size += chunks[i].length;
        this._chunks.push.apply(this._chunks, chunks);
        setTimeout(callback.bind(this, this._size), 1);
    },

    finishWriting: function() { },

    /**
     * @param {function(?string)} callback
     */
    read: function(callback)
    {
        this.readRange(undefined, undefined, callback);
    },

    /**
     * @param {number|undefined} startOffset
     * @param {number|undefined} endOffset
     * @param {function(?string)} callback
     */
    readRange: function(startOffset, endOffset, callback)
    {
        var blob = new Blob(this._chunks);
        blob = blob.slice(startOffset || 0, endOffset || blob.size);
        reader = new FileReader();
        var self = this;
        reader.onloadend = function()
        {
            callback(reader.result);
        }
        reader.readAsText(blob);
    },

    /**
     * @param {!WebInspector.OutputStream} outputStream
     * @param {!WebInspector.OutputStreamDelegate} delegate
     */
    writeToOutputSteam: function(outputStream, delegate)
    {
        var name = this._name;
        var text = this._chunks.join("");
        var chunkedReaderMock = {
            loadedSize: function()
            {
                return text.length;
            },

            fileSize: function()
            {
                return text.length;
            },

            fileName: function()
            {
                return name;
            },

            cancel: function() { }
        }
        delegate.onTransferStarted(chunkedReaderMock);
        outputStream.write(text);
        delegate.onChunkTransferred(chunkedReaderMock);
        outputStream.close();
        delegate.onTransferFinished(chunkedReaderMock);
    },

    remove: function() { }
}

InspectorTest.TempFileMock.create = function(dirPath, name)
{
    var tempFile = new InspectorTest.TempFileMock(dirPath, name);
    return Promise.resolve(tempFile);
}

InspectorTest.dumpLoadedModules = function(next)
{
    function moduleSorter(left, right)
    {
        return String.naturalOrderComparator(left._descriptor.name, right._descriptor.name);
    }

    InspectorTest.addResult("Loaded modules:");
    var modules = self.runtime._modules;
    modules.sort(moduleSorter);
    for (var i = 0; i < modules.length; ++i) {
        if (modules[i]._loaded)
            InspectorTest.addResult("    " + modules[i]._descriptor.name);
    }
    if (next)
        next();
}

InspectorTest.TimeoutMock = function()
{
    this._timeoutId = 0;
    this._timeoutIdToProcess = {};
    this._timeoutIdToMillis = {};
    this.setTimeout = this.setTimeout.bind(this);
    this.clearTimeout = this.clearTimeout.bind(this);
}
InspectorTest.TimeoutMock.prototype = {
    setTimeout: function(operation, timeout)
    {
        this._timeoutIdToProcess[++this._timeoutId] = operation;
        this._timeoutIdToMillis[this._timeoutId] = timeout;
        return this._timeoutId;
    },

    clearTimeout: function(timeoutId)
    {
        delete this._timeoutIdToProcess[timeoutId];
        delete this._timeoutIdToMillis[timeoutId];
    },

    activeTimersTimeouts: function()
    {
        return Object.values(this._timeoutIdToMillis);
    },

    fireAllTimers: function()
    {
        for (var timeoutId in this._timeoutIdToProcess)
            this._timeoutIdToProcess[timeoutId].call(window);
        this._timeoutIdToProcess = {};
        this._timeoutIdToMillis = {};
    }
}

WebInspector.targetManager.observeTargets({
    targetAdded: function(target)
    {
        if (InspectorTest.CSSAgent)
            return;
        InspectorTest.CSSAgent = target.cssAgent();
        InspectorTest.CanvasAgent = target.canvasAgent();
        InspectorTest.ConsoleAgent = target.consoleAgent();
        InspectorTest.DOMAgent = target.domAgent();
        InspectorTest.DOMDebuggerAgent = target.domdebuggerAgent();
        InspectorTest.DebuggerAgent = target.debuggerAgent();
        InspectorTest.EmulationAgent = target.emulationAgent();
        InspectorTest.HeapProfilerAgent = target.heapProfilerAgent();
        InspectorTest.InspectorAgent = target.inspectorAgent();
        InspectorTest.LayerTreeAgent = target.layerTreeAgent();
        InspectorTest.NetworkAgent = target.networkAgent();
        InspectorTest.PageAgent = target.pageAgent();
        InspectorTest.ProfilerAgent = target.profilerAgent();
        InspectorTest.RuntimeAgent = target.runtimeAgent();
        InspectorTest.WorkerAgent = target.workerAgent();

        InspectorTest.consoleModel = target.consoleModel;
        InspectorTest.networkManager = target.networkManager;
        InspectorTest.resourceTreeModel = target.resourceTreeModel;
        InspectorTest.networkLog = target.networkLog;
        InspectorTest.debuggerModel = target.debuggerModel;
        InspectorTest.runtimeModel = target.runtimeModel;
        InspectorTest.domModel = target.domModel;
        InspectorTest.cssModel = target.cssModel;
        InspectorTest.workerManager = target.workerManager;
        InspectorTest.powerProfiler = target.powerProfiler;
        InspectorTest.databaseModel = target.databaseModel;
        InspectorTest.domStorageModel = target.domStorageModel;
        InspectorTest.cpuProfilerModel = target.cpuProfilerModel;
        InspectorTest.heapProfilerModel = target.heapProfilerModel;
        InspectorTest.indexedDBModel = target.indexedDBModel;
        InspectorTest.layerTreeModel = target.layerTreeModel;
        InspectorTest.animationModel = target.animationModel;
        InspectorTest.serviceWorkerCacheModel = target.serviceWorkerCacheModel;
        InspectorTest.tracingManager = target.tracingManager;
        InspectorTest.mainTarget = target;
    },

    targetRemoved: function(target) { }
});

InspectorTest._panelsToPreload = [];

InspectorTest.preloadPanel = function(panelName)
{
    InspectorTest._panelsToPreload.push(panelName);
}

};  // initialize_InspectorTest

var initializeCallId = 0;
var runTestCallId = 1;
var evalCallbackCallId = 2;
var frontendReopeningCount = 0;

function reopenFrontend()
{
    closeFrontend(openFrontendAndIncrement);
}

function closeFrontend(callback)
{
    // Do this asynchronously to allow InspectorBackendDispatcher to send response
    // back to the frontend before it's destroyed.
    // FIXME: we need a better way of waiting for chromium events to happen
    setTimeout(function() {
        testRunner.closeWebInspector();
        callback();
    }, 1);
}

function openFrontendAndIncrement()
{
    frontendReopeningCount++;
    testRunner.showWebInspector();
    setTimeout(runTest, 1);
}

function runAfterIframeIsLoaded()
{
    if (window.testRunner)
        testRunner.waitUntilDone();
    function step()
    {
        if (!window.iframeLoaded)
            setTimeout(step, 100);
        else
            runTest();
    }
    setTimeout(step, 100);
}

function runTest(enableWatchDogWhileDebugging)
{
    if (!window.testRunner)
        return;

    testRunner.dumpAsText();
    testRunner.waitUntilDone();

    function initializeFrontend(initializationFunctions)
    {
        if (window.InspectorTest) {
            InspectorTest.pageLoaded();
            return;
        }

        InspectorTest = {};

        for (var i = 0; i < initializationFunctions.length; ++i) {
            try {
                initializationFunctions[i]();
            } catch (e) {
                console.error("Exception in test initialization: " + e + " " + e.stack);
                InspectorTest.completeTest();
            }
        }
    }

    function runTestInFrontend(testFunction)
    {
        if (InspectorTest.wasAlreadyExecuted)
            return;

        InspectorTest.wasAlreadyExecuted = true;

        // 1. Preload panels.
        var lastLoadedPanel;

        var promises = [];
        for (var i = 0; i < InspectorTest._panelsToPreload.length; ++i) {
            lastLoadedPanel = InspectorTest._panelsToPreload[i];
            promises.push(WebInspector.inspectorView.panel(lastLoadedPanel));
        }

        var testPath = WebInspector.settings.testPath.get();

        if (testPath.indexOf("layers/") !== -1)
            Runtime.experiments.setEnabled("layersPanel", true);
        if (testPath.indexOf("shadow-host-display-modes") !== -1)
            Runtime.experiments.setEnabled("composedShadowDOM", true);

        // 2. Show initial panel based on test path.
        var initialPanelByFolder = {
            "audits": "audits",
            "console": "console",
            "elements": "elements",
            "editor": "sources",
            "layers": "layers",
            "profiler": "profiles",
            "resource-tree": "resources",
            "search": "sources",
            "sources": "sources",
            "timeline": "timeline",
            "tracing": "timeline",
        }
        var initialPanelShown = false;
        for (var folder in initialPanelByFolder) {
            if (testPath.indexOf(folder + "/") !== -1) {
                lastLoadedPanel = initialPanelByFolder[folder];
                promises.push(WebInspector.inspectorView.panel(lastLoadedPanel));
                break;
            }
        }

        // 3. Run test function.
        Promise.all(promises).then(function() {
            if (lastLoadedPanel)
                WebInspector.inspectorView.showInitialPanelForTest(lastLoadedPanel);
            testFunction();
        }).catch(function(e) {
            console.error(e);
            InspectorTest.completeTest();
        });
    }

    var initializationFunctions = [ String(initialize_InspectorTest) ];
    for (var name in window) {
        if (name.indexOf("initialize_") === 0 && typeof window[name] === "function" && name !== "initialize_InspectorTest")
            initializationFunctions.push(window[name].toString());
    }
    var toEvaluate = "(" + initializeFrontend + ")(" + "[" + initializationFunctions + "]" + ");";
    testRunner.evaluateInWebInspector(initializeCallId, toEvaluate);

    if (window.debugTest)
        test = "function() { " + test.toString() + "; window.test = test; InspectorTest.addResult = window._originalConsoleLog; InspectorTest.completeTest = function() {}; InspectorTest.debugTest = true; }";
    toEvaluate = "(" + runTestInFrontend + ")(" + test + ");";
    testRunner.evaluateInWebInspector(runTestCallId, toEvaluate);

    if (enableWatchDogWhileDebugging) {
        function watchDog()
        {
            console.log("Internal watchdog triggered at 20 seconds. Test timed out.");
            closeInspectorAndNotifyDone();
        }
        window._watchDogTimer = setTimeout(watchDog, 20000);
    }
}

function runTestAfterDisplay(enableWatchDogWhileDebugging)
{
    if (!window.testRunner)
        return;

    testRunner.waitUntilDone();
    requestAnimationFrame(runTest.bind(this, enableWatchDogWhileDebugging));
}

function completeTest(results)
{
    flushResults(results);
    closeInspectorAndNotifyDone();
}

function flushResults(results)
{
    results = (JSON && JSON.parse ? JSON.parse : eval)(unescape(results));
    for (var i = 0; i < results.length; ++i)
        _output(results[i]);
}

function closeInspectorAndNotifyDone()
{
    if (window._watchDogTimer)
        clearTimeout(window._watchDogTimer);

    testRunner.closeWebInspector();
    setTimeout(function() {
        testRunner.notifyDone();
    }, 0);
}

var outputElement;

function createOutputElement()
{
    outputElement = document.createElement("div");
    // Support for svg - add to document, not body, check for style.
    if (outputElement.style) {
        outputElement.style.whiteSpace = "pre";
        outputElement.style.height = "10px";
        outputElement.style.overflow = "hidden";
    }
    document.documentElement.appendChild(outputElement);
}

function output(text)
{
    _output("[page] " + text);
}

function _output(result)
{
    if (!outputElement)
        createOutputElement();
    outputElement.appendChild(document.createTextNode(result));
    outputElement.appendChild(document.createElement("br"));
}
