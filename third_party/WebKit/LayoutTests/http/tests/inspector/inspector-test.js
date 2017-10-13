/**
 * These test helper APIs are being migrated to
 * third_party/WebKit/Source/devtools/front_end/test_runner
 * See crbug.com/667560
 */

if (window.GCController)
    GCController.collectAll();

var initialize_InspectorTest = function() {
Protocol.InspectorBackend.Options.suppressRequestErrors = true;
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

InspectorTest.completeTest = function()
{
    InspectorTest.RuntimeAgent.evaluate("completeTest(\"" + escape(JSON.stringify(results)) + "\")", "test");
}

InspectorTest.flushResults = function()
{
    InspectorTest.RuntimeAgent.evaluate("flushResults(\"" + escape(JSON.stringify(results)) + "\")", "test");
    results = [];
}

/**
 * TestRunner.evaluateInPage inserts sourceURL by inspecting the call stack.
 */
InspectorTest.evaluateInPage = async function(code, callback)
{
    var response = await InspectorTest.RuntimeAgent.invoke_evaluate({
        expression: code,
        objectGroup: "console"
    });
    if (!response[Protocol.Error])
        InspectorTest.safeWrap(callback)(InspectorTest.runtimeModel.createRemoteObject(response.result), response.exceptionDetails);
}

InspectorTest.addResult = function(text)
{
    results.push(String(text));
}

window.onerror = function (message, filename, lineno, colno, error)
{
    InspectorTest.addResult("Uncaught exception in inspector front-end: " + message + " [" + error.stack + "]");
    InspectorTest.completeTest();
}

TestRunner.formatters.formatAsURL = function(value)
{
    if (!value)
        return value;
    var lastIndex = value.lastIndexOf("inspector/");
    if (lastIndex < 0)
        return value;
    return ".../" + value.substr(lastIndex);
}

InspectorTest.navigate = function(url, callback)
{
    InspectorTest._pageLoadedCallback = InspectorTest.safeWrap(callback);
    InspectorTest.evaluateInPage("window.location.replace('" + url + "')");
}

InspectorTest.navigatePromise = function(url)
{
    var fulfill;
    var promise = new Promise(callback => fulfill = callback);
    InspectorTest.navigate(url, fulfill);
    return promise;
}

InspectorTest.hardReloadPage = function(callback, scriptToEvaluateOnLoad)
{
    InspectorTest._innerReloadPage(true, callback, scriptToEvaluateOnLoad);
}

InspectorTest.reloadPage = function(callback, scriptToEvaluateOnLoad)
{
    InspectorTest._innerReloadPage(false, callback, scriptToEvaluateOnLoad);
}

InspectorTest.reloadPagePromise = function(scriptToEvaluateOnLoad)
{
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    InspectorTest.reloadPage(fulfill, scriptToEvaluateOnLoad);
    return promise;
}

InspectorTest._innerReloadPage = function(hardReload, callback, scriptToEvaluateOnLoad)
{
    InspectorTest._pageLoadedCallback = InspectorTest.safeWrap(callback);

    if (UI.panels.network)
        NetworkLog.networkLog.reset();
    InspectorTest.resourceTreeModel.reloadPage(hardReload, scriptToEvaluateOnLoad);
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

InspectorTest.addConsoleSniffer = function(override, opt_sticky)
{
    InspectorTest.addSniffer(ConsoleModel.ConsoleModel.prototype, "addMessage", override, opt_sticky);
}

SDK.targetManager.observeTargets({
    targetAdded: function(target)
    {
        if (TestRunner.CSSAgent)
            return;
        TestRunner._setupTestHelpers(target);
        InspectorTest.consoleModel = ConsoleModel.consoleModel;
        InspectorTest.networkLog = NetworkLog.networkLog;
        InspectorTest = Object.assign({}, self.TestRunner, InspectorTest);
    },

    targetRemoved: function(target) { }
});

InspectorTest._panelsToPreload = [];

InspectorTest.preloadPanel = function(panelName)
{
    InspectorTest._panelsToPreload.push(panelName);
}

InspectorTest._modulesToPreload = [];

InspectorTest.preloadModule = function(moduleName)
{
    InspectorTest._modulesToPreload.push(moduleName);
}

self.AccessibilityTestRunner = InspectorTest;
self.ApplicationTestRunner = InspectorTest;
self.AuditsTestRunner = InspectorTest;
self.BindingsTestRunner = InspectorTest;
self.ConsoleTestRunner = InspectorTest;
self.CoverageTestRunner = InspectorTest;
self.DataGridTestRunner = InspectorTest;
self.DeviceModeTestRunner = InspectorTest;
self.ElementsTestRunner = InspectorTest;
self.ExtensionsTestRunner = InspectorTest;
self.LayersTestRunner = InspectorTest;
self.NetworkTestRunner = InspectorTest;
self.PerformanceTestRunner = InspectorTest;
self.ProfilerTestRunner = InspectorTest;
self.SASSTestRunner = InspectorTest;
self.SecurityTestRunner = InspectorTest;
self.SDKTestRunner = InspectorTest;
self.SourcesTestRunner = InspectorTest;
self.TestRunner = Object.assign(self.TestRunner, InspectorTest);

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
    testRunner.showWebInspector(JSON.stringify({testPath: '"' + location.href + '"'}).replace(/\"/g,"%22"));
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

function runTest(pixelTest, enableWatchDogWhileDebugging)
{
    if (!window.testRunner)
        return;

    if (pixelTest)
        testRunner.dumpAsTextWithPixelResults();
    else
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

        var modulePromise = Promise.resolve();

        for (let moduleName of InspectorTest._modulesToPreload)
            modulePromise = modulePromise.then(() => TestRunner.loadModule(moduleName));

        var promises = [modulePromise];

        for (var i = 0; i < InspectorTest._panelsToPreload.length; ++i) {
            lastLoadedPanel = InspectorTest._panelsToPreload[i];
            promises.push(UI.inspectorView.panel(lastLoadedPanel));
        }

        var testPath = Common.settings.createSetting("testPath", "").get();

        // 2. Show initial panel based on test path.
        var initialPanelByFolder = {
            "animation": "elements",
            "audits": "audits",
            "console": "console",
            "elements": "elements",
            "editor": "sources",
            "layers": "layers",
            "network": "network",
            "profiler": "heap_profiler",
            "resource-tree": "resources",
            "search": "sources",
            "security": "security",
            "service-workers": "resources",
            "sources": "sources",
            "timeline": "timeline",
            "tracing": "timeline",
        }
        var initialPanelShown = false;
        for (var folder in initialPanelByFolder) {
            if (testPath.indexOf(folder + "/") !== -1) {
                lastLoadedPanel = initialPanelByFolder[folder];
                promises.push(UI.inspectorView.panel(lastLoadedPanel));
                break;
            }
        }

        // 3. Run test function.
        Promise.all(promises).then(() => {
            if (lastLoadedPanel)
                UI.inspectorView.showPanel(lastLoadedPanel).then(testFunction);
            else
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
        test = "function() { Protocol.InspectorBackend.Options.suppressRequestErrors = false; window.test = " + test.toString() + "; InspectorTest.addResult = window._originalConsoleLog; InspectorTest.completeTest = function() {}; TestRunner.addResult = InspectorTest.addResult; TestRunner.completeTest = InspectorTest.completeTest; }";
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
