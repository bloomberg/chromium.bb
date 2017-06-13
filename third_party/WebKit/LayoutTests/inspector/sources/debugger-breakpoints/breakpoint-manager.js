var initialize_BreakpointManagerTest = function() {

InspectorTest.createWorkspace = function()
{
    InspectorTest.testTargetManager = new SDK.TargetManager();
    InspectorTest.testWorkspace = new Workspace.Workspace();
    InspectorTest.testNetworkProjectManager = new Bindings.NetworkProjectManager(InspectorTest.testTargetManager, InspectorTest.testWorkspace);
    InspectorTest.testResourceMapping = new Bindings.ResourceMapping(InspectorTest.testTargetManager, InspectorTest.testWorkspace);
    InspectorTest.testDebuggerWorkspaceBinding = new Bindings.DebuggerWorkspaceBinding(InspectorTest.testTargetManager, InspectorTest.testWorkspace);
    // Override ResourceMapping so that CSSWorkspaceBinding and DebuggerWorkspaceBinding refer to the correct one.
    Bindings.resourceMapping = InspectorTest.testResourceMapping;
}

function resourceMappingModelInfoForTarget(target) {
    var resourceTreeModel = target.model(SDK.ResourceTreeModel);
    var binding = resourceTreeModel ? InspectorTest.testResourceMapping._modelToInfo.get(resourceTreeModel) : null;
    return binding;
}

InspectorTest.createMockTarget = function(id)
{
    var capabilities = SDK.Target.Capability.AllForTests;
    var target = InspectorTest.testTargetManager.createTarget("mock-target-id-" + id, "mock-target-" + id, capabilities & (~SDK.Target.Capability.JS), (params) => new SDK.StubConnection(params), null);
    InspectorTest.testNetworkProject = Bindings.NetworkProject.forTarget(target);
    InspectorTest.testResourceMappingModelInfo = resourceMappingModelInfoForTarget(target);
    target._capabilitiesMask = capabilities;
    target._inspectedURL = InspectorTest.mainTarget.inspectedURL();
    target.resourceTreeModel = target.model(SDK.ResourceTreeModel);
    target.resourceTreeModel._cachedResourcesProcessed = true;
    target.resourceTreeModel._frameAttached("42", 0);
    target.runtimeModel = /** @type {!SDK.RuntimeModel} */ (target.model(SDK.RuntimeModel));
    target.debuggerModel = new InspectorTest.DebuggerModelMock(target);
    target._modelByConstructor.set(SDK.DebuggerModel, target.debuggerModel);
    InspectorTest.testTargetManager.modelAdded(target, SDK.DebuggerModel, target.debuggerModel);
    return target;
}

InspectorTest.uiSourceCodes = {};

InspectorTest.initializeDefaultMappingOnTarget = function(target)
{
    var defaultMapping = {
        rawLocationToUILocation: function(rawLocation)
        {
            return InspectorTest.uiSourceCodes[rawLocation.scriptId].uiLocation(rawLocation.lineNumber, 0);
        },

        uiLocationToRawLocation: function(uiSourceCode, lineNumber)
        {
            if (!InspectorTest.uiSourceCodes[uiSourceCode.url()])
                return null;
            return new SDK.DebuggerModel.Location(target.debuggerModel, uiSourceCode.url(), lineNumber, 0);
        },

        isIdentity: function()
        {
            return true;
        }
    };
    target.defaultMapping = defaultMapping;
}

InspectorTest.DebuggerModelMock = class extends SDK.SDKModel {
    sourceMapManager() {
        return this._sourceMapManager;
    }

    constructor(target)
    {
        super(target);
        this._sourceMapManager = new SDK.SourceMapManager();
        this._target = target;
        this._breakpointResolvedEventTarget = new Common.Object();
        this._scripts = {};
        this._breakpoints = {};
        this._debuggerWorkspaceBinding = InspectorTest.testDebuggerWorkspaceBinding;
    }

    target()
    {
        return this._target;
    }

    runtimeModel()
    {
        return this._target.runtimeModel;
    }

    setBeforePausedCallback(callback) { }

    debuggerEnabled()
    {
        return true;
    }

    scriptsForSourceURL(url)
    {
        var script = this._scriptForURL(url);
        return script ? [script] : [];
    }

    _addScript(scriptId, url)
    {
        var script = new SDK.Script(this, scriptId, url);
        this._scripts[scriptId] = script;
        this._debuggerWorkspaceBinding._debuggerModelToData.get(this)._parsedScriptSource({data: script});
    }

    _scriptForURL(url)
    {
        for (var scriptId in this._scripts) {
            var script = this._scripts[scriptId];
            if (script.sourceURL === url)
                return script;
        }
    }

    _scheduleSetBeakpointCallback(callback, breakpointId, locations)
    {
        setTimeout(innerCallback.bind(this), 0);

        function innerCallback()
        {
            if (callback)
                callback(breakpointId, locations);
            if (window.setBreakpointCallback) {
                var savedCallback = window.setBreakpointCallback;
                delete window.setBreakpointCallback;
                savedCallback();
            }
        }
    }

    createRawLocation(script, line, column)
    {
        return new SDK.DebuggerModel.Location(this, script.scriptId, line, column);
    }

    createRawLocationByURL(url, line, column)
    {
        var script = this._scriptForURL(url);
        if (!script)
            return null;
        return new SDK.DebuggerModel.Location(this, script.scriptId, line, column);
    }

    setBreakpointByURL(url, lineNumber, columnNumber, condition, callback)
    {
        InspectorTest.addResult("    debuggerModel.setBreakpoint(" + [url, lineNumber, condition].join(":") + ")");

        var breakpointId = url + ":" + lineNumber;
        if (this._breakpoints[breakpointId]) {
            this._scheduleSetBeakpointCallback(callback, null);
            return;
        }
        this._breakpoints[breakpointId] = true;

        if (lineNumber >= 2000) {
            this._scheduleSetBeakpointCallback(callback, breakpointId, []);
            return;
        }
        if (lineNumber >= 1000) {
            var shiftedLocation = new SDK.DebuggerModel.Location(this, url, lineNumber + 10, columnNumber);
            this._scheduleSetBeakpointCallback(callback, breakpointId, [shiftedLocation]);
            return;
        }

        var locations = [];
        var script = this._scriptForURL(url);
        if (script) {
            var location = new SDK.DebuggerModel.Location(this, script.scriptId, lineNumber, 0);
            locations.push(location);
        }

        this._scheduleSetBeakpointCallback(callback, breakpointId, locations);
    }

    async removeBreakpoint(breakpointId)
    {
        InspectorTest.addResult("    debuggerModel.removeBreakpoint(" + breakpointId + ")");
        delete this._breakpoints[breakpointId];
    }

    setBreakpointsActive() { }

    scriptForId(scriptId)
    {
        return this._scripts[scriptId];
    }

    reset()
    {
        InspectorTest.addResult("  Resetting debugger.");
        this._scripts = {};
        this._debuggerWorkspaceBinding._reset(this);
    }

    addBreakpointListener(breakpointId, listener, thisObject)
    {
        this._breakpointResolvedEventTarget.addEventListener(breakpointId, listener, thisObject)
    }

    removeBreakpointListener(breakpointId, listener, thisObject)
    {
        this._breakpointResolvedEventTarget.removeEventListener(breakpointId, listener, thisObject);
    }

    _breakpointResolved(breakpointId, location)
    {
        this._breakpointResolvedEventTarget.dispatchEventToListeners(breakpointId, location);
    }
};

InspectorTest.setupLiveLocationSniffers = function()
{
    InspectorTest.addSniffer(Bindings.DebuggerWorkspaceBinding.prototype, "createLiveLocation", function(rawLocation)
    {
        InspectorTest.addResult("    Location created: " + rawLocation.scriptId + ":" + rawLocation.lineNumber);
    }, true);
    InspectorTest.addSniffer(Bindings.DebuggerWorkspaceBinding.Location.prototype, "dispose", function()
    {
        InspectorTest.addResult("    Location disposed: " + this._rawLocation.scriptId + ":" + this._rawLocation.lineNumber);
    }, true);
}

InspectorTest.addScript = function(target, breakpointManager, url)
{
    target.debuggerModel._addScript(url, url);
    InspectorTest.addResult("  Adding script: " + url);
    var uiSourceCodes = breakpointManager._workspace.uiSourceCodesForProjectType(Workspace.projectTypes.Debugger);
    for (var i = 0; i < uiSourceCodes.length; ++i) {
        var uiSourceCode = uiSourceCodes[i];
        if (uiSourceCode.url() === url) {
            InspectorTest.uiSourceCodes[url] = uiSourceCode;
            return uiSourceCode;
        }
    }
}

InspectorTest.addUISourceCode = function(target, breakpointManager, url, doNotSetSourceMapping, doNotAddScript)
{
    if (!doNotAddScript)
        InspectorTest.addScript(target, breakpointManager, url);
    InspectorTest.addResult("  Adding UISourceCode: " + url);

    // Add resource to get UISourceCode.
    var resourceMappingModelInfo = resourceMappingModelInfoForTarget(target);
    if (resourceMappingModelInfo._bindings.has(url)) {
        resourceMappingModelInfo._bindings.get(url).dispose();
        resourceMappingModelInfo._bindings.delete(url);
    }
    var resource = new SDK.Resource(target, null, url, url, '', '', Common.resourceTypes.Document, 'text/html', null, null);
    resourceMappingModelInfo._resourceAdded({data: resource});
    uiSourceCode = InspectorTest.testWorkspace.uiSourceCodeForURL(url);

    InspectorTest.uiSourceCodes[url] = uiSourceCode;
    if (!doNotSetSourceMapping) {
        breakpointManager._debuggerWorkspaceBinding.updateLocations(target.debuggerModel.scriptForId(url));
    }
    return uiSourceCode;
}

InspectorTest.createBreakpointManager = function(targetManager, debuggerWorkspaceBinding, persistentBreakpoints)
{
    InspectorTest._pendingBreakpointUpdates = 0;
    InspectorTest.addSniffer(Bindings.BreakpointManager.ModelBreakpoint.prototype, "_updateInDebugger", updateInDebugger, true);
    InspectorTest.addSniffer(Bindings.BreakpointManager.ModelBreakpoint.prototype, "_didUpdateInDebugger", didUpdateInDebugger, true);

    function updateInDebugger()
    {
        InspectorTest._pendingBreakpointUpdates++;
    }

    function didUpdateInDebugger()
    {
        InspectorTest._pendingBreakpointUpdates--;
        InspectorTest._notifyAfterBreakpointUpdate();
    }

    persistentBreakpoints = persistentBreakpoints || [];
    var setting = {
        get: function() { return persistentBreakpoints; },
        set: function(breakpoints) { persistentBreakpoints = breakpoints; }
    };

    function breakpointAdded(event)
    {
        var breakpoint = event.data.breakpoint;
        var uiLocation = event.data.uiLocation;
        InspectorTest.addResult("    breakpointAdded(" + [uiLocation.uiSourceCode.url(), uiLocation.lineNumber, uiLocation.columnNumber, breakpoint.condition(), breakpoint.enabled()].join(", ") + ")");
    }

    function breakpointRemoved(event)
    {
        var uiLocation = event.data.uiLocation;
        InspectorTest.addResult("    breakpointRemoved(" + [uiLocation.uiSourceCode.url(), uiLocation.lineNumber, uiLocation.columnNumber].join(", ") + ")");
    }
    var targets = targetManager.targets();
    var mappingForManager;
    for (var i = 0; i < targets.length; ++i) {
        InspectorTest.initializeDefaultMappingOnTarget(targets[i]);
        if (!mappingForManager)
            mappingForManager = targets[i].defaultMapping;
    }

    var breakpointManager = new Bindings.BreakpointManager(setting, debuggerWorkspaceBinding._workspace, targetManager, debuggerWorkspaceBinding);
    breakpointManager.defaultMapping = mappingForManager;
    breakpointManager.addEventListener(Bindings.BreakpointManager.Events.BreakpointAdded, breakpointAdded);
    breakpointManager.addEventListener(Bindings.BreakpointManager.Events.BreakpointRemoved, breakpointRemoved);
    InspectorTest.addResult("  Created breakpoints manager");
    InspectorTest.dumpBreakpointStorage(breakpointManager);
    return breakpointManager;
}

InspectorTest.setBreakpoint = function(breakpointManager, uiSourceCode, lineNumber, columnNumber, condition, enabled, setBreakpointCallback)
{
    InspectorTest.addResult("  Setting breakpoint at " + uiSourceCode.url() + ":" + lineNumber + ":" + columnNumber + " enabled:" + enabled + " condition:" + condition);
    if (setBreakpointCallback)
        window.setBreakpointCallback = setBreakpointCallback;
    return breakpointManager.setBreakpoint(uiSourceCode, lineNumber, columnNumber, condition, enabled);
}

InspectorTest.removeBreakpoint = function(breakpointManager, uiSourceCode, lineNumber, columnNumber)
{
    InspectorTest.addResult("  Removing breakpoint at " + uiSourceCode.url() + ":" + lineNumber + ":" + columnNumber);
    breakpointManager.findBreakpoint(uiSourceCode, lineNumber, columnNumber).remove();
}

InspectorTest.dumpBreakpointStorage = function(breakpointManager)
{
    var breakpoints = breakpointManager._storage._setting.get();
    InspectorTest.addResult("  Dumping Storage");
    for (var i = 0; i < breakpoints.length; ++i)
        InspectorTest.addResult("    " + breakpoints[i].url + ":" + breakpoints[i].lineNumber  + " enabled:" + breakpoints[i].enabled + " condition:" + breakpoints[i].condition);
}

InspectorTest.dumpBreakpointLocations = function(breakpointManager)
{
    var allBreakpointLocations = breakpointManager.allBreakpointLocations();
    InspectorTest.addResult("  Dumping Breakpoint Locations");
    var lastUISourceCode = null;
    var locations = [];

    function dumpLocations(uiSourceCode, locations)
    {
        locations.sort(function(a, b) {
            return a.lineNumber - b.lineNumber;
        });
        InspectorTest.addResult("    UISourceCode (url='" + uiSourceCode.url() + "', uri='" + uiSourceCode.url() + "')");
        for (var i = 0; i < locations.length; ++i)
            InspectorTest.addResult("      Location: (" + locations[i].lineNumber + ", " + locations[i].columnNumber + ")");
    }

    for (var i = 0; i < allBreakpointLocations.length; ++i) {
        var uiLocation = allBreakpointLocations[i].uiLocation;
        var uiSourceCode = uiLocation.uiSourceCode;
        if (lastUISourceCode && lastUISourceCode != uiSourceCode) {
            dumpLocations(uiSourceCode, locations);
            locations = [];
        }
        lastUISourceCode = uiSourceCode;
        locations.push(uiLocation);
    }
    if (lastUISourceCode)
        dumpLocations(lastUISourceCode, locations);
}

InspectorTest.resetBreakpointManager = function(breakpointManager, next)
{
    InspectorTest.addResult("  Resetting breakpoint manager");
    breakpointManager.removeAllBreakpoints();
    breakpointManager.removeProvisionalBreakpointsForTest();
    InspectorTest.uiSourceCodes = {};
    next();
}

InspectorTest.runAfterPendingBreakpointUpdates = function(breakpointManager, callback)
{
    InspectorTest._pendingBreakpointUpdatesCallback = callback;
    InspectorTest._notifyAfterBreakpointUpdate();
}

InspectorTest._notifyAfterBreakpointUpdate = function()
{
    if (!InspectorTest._pendingBreakpointUpdates && InspectorTest._pendingBreakpointUpdatesCallback) {
        var callback = InspectorTest._pendingBreakpointUpdatesCallback;
        delete InspectorTest._pendingBreakpointUpdatesCallback;
        callback();
    }
}

InspectorTest.finishBreakpointTest = function(breakpointManager, next)
{
    InspectorTest.runAfterPendingBreakpointUpdates(breakpointManager, dump);

    function dump()
    {
        InspectorTest.dumpBreakpointLocations(breakpointManager);
        InspectorTest.dumpBreakpointStorage(breakpointManager);
        InspectorTest.runAfterPendingBreakpointUpdates(breakpointManager, reset);
    }

    function reset()
    {
        InspectorTest.resetBreakpointManager(breakpointManager, didReset);
    }

    function didReset()
    {
        InspectorTest.runAfterPendingBreakpointUpdates(breakpointManager, finish);
    }

    function finish()
    {
        InspectorTest.dumpBreakpointLocations(breakpointManager);
        next();
    }
}

}
