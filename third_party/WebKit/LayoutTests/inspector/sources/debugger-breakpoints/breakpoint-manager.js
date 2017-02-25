var initialize_BreakpointManagerTest = function() {

InspectorTest.createWorkspace = function(ignoreEvents)
{
    if (InspectorTest.testFileSystemWorkspaceBinding)
        InspectorTest.testFileSystemWorkspaceBinding.dispose();
    Workspace.fileSystemMapping.resetForTesting();

    InspectorTest.testTargetManager = new SDK.TargetManager();
    InspectorTest.testWorkspace = new Workspace.Workspace();
    InspectorTest.testFileSystemWorkspaceBinding = new Persistence.FileSystemWorkspaceBinding(Workspace.isolatedFileSystemManager, InspectorTest.testWorkspace);
    InspectorTest.testNetworkProjectManager = new Bindings.NetworkProjectManager(InspectorTest.testTargetManager, InspectorTest.testWorkspace);
    InspectorTest.testDebuggerWorkspaceBinding = new Bindings.DebuggerWorkspaceBinding(InspectorTest.testTargetManager, InspectorTest.testWorkspace);
    InspectorTest.testCSSWorkspaceBinding = new Bindings.CSSWorkspaceBinding(InspectorTest.testTargetManager, InspectorTest.testWorkspace);

    InspectorTest.testTargetManager.observeTargets({
        targetAdded: function(target)
        {
            InspectorTest.testNetworkProject = Bindings.NetworkProject.forTarget(target);
        },

        targetRemoved: function(target)
        {
        }
    });

    if (ignoreEvents)
        return;
    InspectorTest.testWorkspace.addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, InspectorTest._defaultWorkspaceEventHandler);
    InspectorTest.testWorkspace.addEventListener(Workspace.Workspace.Events.UISourceCodeRemoved, InspectorTest._defaultWorkspaceEventHandler);
}

InspectorTest._mockTargetId = 1;
InspectorTest._pageCapabilities =
    SDK.Target.Capability.Browser | SDK.Target.Capability.DOM |
    SDK.Target.Capability.JS | SDK.Target.Capability.Log |
    SDK.Target.Capability.Network | SDK.Target.Capability.Worker;

InspectorTest.createMockTarget = function(id, debuggerModelConstructor, capabilities)
{
    capabilities = capabilities || InspectorTest._pageCapabilities;
    var target = InspectorTest.testTargetManager.createTarget("mock-target-" + id, "mock-target-" + id, capabilities & (~SDK.Target.Capability.JS), (params) => new SDK.StubConnection(params), null);
    target._capabilitiesMask = capabilities;
    target._inspectedURL = InspectorTest.mainTarget.inspectedURL();
    target.resourceTreeModel = target.model(SDK.ResourceTreeModel);
    target.resourceTreeModel._cachedResourcesProcessed = true;
    target.resourceTreeModel._frameAttached("42", 0);
    target.runtimeModel = /** @type {!SDK.RuntimeModel} */ (target.model(SDK.RuntimeModel));
    if (debuggerModelConstructor) {
        target.debuggerModel = new debuggerModelConstructor(target);
        target._modelByConstructor.set(SDK.DebuggerModel, target.debuggerModel);
        InspectorTest.testTargetManager.modelAdded(target, SDK.DebuggerModel, target.debuggerModel);
    } else {
        target.debuggerModel = target.model(SDK.DebuggerModel);
    }
    return target;
}

InspectorTest.createWorkspaceWithTarget = function(ignoreEvents)
{
    InspectorTest.createWorkspace(ignoreEvents);
    var target = InspectorTest.createMockTarget(InspectorTest._mockTargetId++);
    return target;
}

InspectorTest.waitForWorkspaceUISourceCodeAddedEvent = function(callback, count, projectType)
{
    InspectorTest.uiSourceCodeAddedEventsLeft = count || 1;
    InspectorTest.testWorkspace.removeEventListener(Workspace.Workspace.Events.UISourceCodeAdded, InspectorTest._defaultWorkspaceEventHandler);
    InspectorTest.testWorkspace.addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);

    function uiSourceCodeAdded(event)
    {
        if (projectType && event.data.project().type() !== projectType)
            return;
        if (!projectType && event.data.project().type() === Workspace.projectTypes.Service)
            return;
        if (!(--InspectorTest.uiSourceCodeAddedEventsLeft)) {
            InspectorTest.testWorkspace.removeEventListener(Workspace.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);
            InspectorTest.testWorkspace.addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, InspectorTest._defaultWorkspaceEventHandler);
        }
        callback(event.data);
    }
}

InspectorTest.waitForWorkspaceUISourceCodeRemovedEvent = function(callback, count)
{
    InspectorTest.uiSourceCodeRemovedEventsLeft = count || 1;
    InspectorTest.testWorkspace.removeEventListener(Workspace.Workspace.Events.UISourceCodeRemoved, InspectorTest._defaultWorkspaceEventHandler);
    InspectorTest.testWorkspace.addEventListener(Workspace.Workspace.Events.UISourceCodeRemoved, uiSourceCodeRemoved);

    function uiSourceCodeRemoved(event)
    {
        if (event.data.project().type() === Workspace.projectTypes.Service)
            return;
        if (!(--InspectorTest.uiSourceCodeRemovedEventsLeft)) {
            InspectorTest.testWorkspace.removeEventListener(Workspace.Workspace.Events.UISourceCodeRemoved, uiSourceCodeRemoved);
            InspectorTest.testWorkspace.addEventListener(Workspace.Workspace.Events.UISourceCodeRemoved, InspectorTest._defaultWorkspaceEventHandler);
        }
        callback(event.data);
    }
}

InspectorTest.addMockUISourceCodeToWorkspace = function(url, type, content)
{
    var mockContentProvider = Common.StaticContentProvider.fromString(url, type, content);
    InspectorTest.testNetworkProject.addFile(mockContentProvider, null, false);
}

InspectorTest.addMockUISourceCodeViaNetwork = function(url, type, content, target)
{
    var mockContentProvider = Common.StaticContentProvider.fromString(url, type, content);
    InspectorTest.testNetworkProject.addFile(mockContentProvider, target.resourceTreeModel.mainFrame, false);
}

InspectorTest._defaultWorkspaceEventHandler = function(event)
{
    var uiSourceCode = event.data;
    if (uiSourceCode.project().type() === Workspace.projectTypes.Service)
        return;
    InspectorTest.addResult(`Workspace event: ${event.type.toString()}: ${uiSourceCode.url()}.`);
}

InspectorTest.uiSourceCodeURL = function(uiSourceCode)
{
    return uiSourceCode.url().replace(/.*LayoutTests/, "LayoutTests");
}

InspectorTest.dumpUISourceCode = function(uiSourceCode, callback)
{
    InspectorTest.addResult("UISourceCode: " + InspectorTest.uiSourceCodeURL(uiSourceCode));
    if (uiSourceCode.contentType() === Common.resourceTypes.Script || uiSourceCode.contentType() === Common.resourceTypes.Document)
        InspectorTest.addResult("UISourceCode is content script: " + (uiSourceCode.project().type() === Workspace.projectTypes.ContentScripts));
    uiSourceCode.requestContent().then(didRequestContent);

    function didRequestContent(content, contentEncoded)
    {
        InspectorTest.addResult("Highlighter type: " + Bindings.NetworkProject.uiSourceCodeMimeType(uiSourceCode));
        InspectorTest.addResult("UISourceCode content: " + content);
        callback();
    }
}

InspectorTest.fileSystemUISourceCodes = function()
{
    var uiSourceCodes = [];
    var fileSystemProjects = Workspace.workspace.projectsForType(Workspace.projectTypes.FileSystem);
    for (var project of fileSystemProjects)
        uiSourceCodes = uiSourceCodes.concat(project.uiSourceCodes());
    return uiSourceCodes;
}

InspectorTest.refreshFileSystemProjects = function(callback)
{
    var barrier = new CallbackBarrier();
    var projects = Workspace.workspace.projects();
    for (var i = 0; i < projects.length; ++i)
        projects[i].refresh("/", barrier.createCallback());
    barrier.callWhenDone(callback);
}

InspectorTest.waitForGivenUISourceCode = function(name, callback)
{
    var uiSourceCodes = Workspace.workspace.uiSourceCodes();
    for (var i = 0; i < uiSourceCodes.length; ++i) {
        if (uiSourceCodes[i].name() === name) {
            setImmediate(callback);
            return;
        }
    }
    Workspace.workspace.addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);

    function uiSourceCodeAdded(event)
    {
        if (event.data.name() === name) {
            Workspace.workspace.removeEventListener(Workspace.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);
            setImmediate(callback);
        }
    }
}

InspectorTest.uiSourceCodes = {};

InspectorTest.dumpTargetIds = false;

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

InspectorTest.dumpTarget = function(targetAware)
{
    return InspectorTest.dumpTargetIds ?  "target " + targetAware.target().id() + " " : "";
}

InspectorTest.DebuggerModelMock = class extends SDK.SDKModel {
    constructor(target)
    {
        super(target);
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

    setBeforePausedCallback(callback) { }

    _targetDisposed() { }

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

    _registerScript(script)
    {
        this._scripts[script.scriptId] = script;
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

    setBreakpointByURL(url, lineNumber, columnNumber, condition, callback)
    {
        InspectorTest.addResult("    " + InspectorTest.dumpTarget(this) + "debuggerModel.setBreakpoint(" + [url, lineNumber, condition].join(":") + ")");

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

    removeBreakpoint(breakpointId, callback)
    {
        InspectorTest.addResult("    " + InspectorTest.dumpTarget(this) + "debuggerModel.removeBreakpoint(" + breakpointId + ")");
        delete this._breakpoints[breakpointId];
        if (callback)
            setTimeout(callback, 0);
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

    pushSourceMapping(sourceMapping)
    {
        for (var scriptId in this._scripts)
            this._debuggerWorkspaceBinding.pushSourceMapping(this._scripts[scriptId], sourceMapping);
    }

    disableSourceMapping(sourceMapping)
    {
        sourceMapping._disabled = true;
        for (var scriptId in this._scripts)
            this._debuggerWorkspaceBinding.updateLocations(this._scripts[scriptId]);
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
        InspectorTest.addResult("    Location created: " + InspectorTest.dumpTarget(rawLocation) + rawLocation.scriptId + ":" + rawLocation.lineNumber);
    }, true);
    InspectorTest.addSniffer(Bindings.DebuggerWorkspaceBinding.Location.prototype, "dispose", function()
    {
        InspectorTest.addResult("    Location disposed: " + InspectorTest.dumpTarget(this._rawLocation) + this._rawLocation.scriptId + ":" + this._rawLocation.lineNumber);
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
            breakpointManager._debuggerWorkspaceBinding.setSourceMapping(target.debuggerModel, uiSourceCode, breakpointManager.defaultMapping);
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
    var contentProvider = Common.StaticContentProvider.fromString(url, Common.resourceTypes.Script, "");
    var binding = breakpointManager._debuggerWorkspaceBinding;
    var uiSourceCode = InspectorTest.testNetworkProject.addFile(contentProvider, null);
    InspectorTest.uiSourceCodes[url] = uiSourceCode;
    if (!doNotSetSourceMapping) {
        breakpointManager._debuggerWorkspaceBinding.setSourceMapping(target.debuggerModel, uiSourceCode, breakpointManager.defaultMapping);
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
