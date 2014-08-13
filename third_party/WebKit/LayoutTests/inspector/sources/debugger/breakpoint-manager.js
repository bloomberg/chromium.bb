var initialize_BreakpointManagerTest = function() {

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
            if (!InspectorTest.uiSourceCodes[uiSourceCode.url])
                return null;
            return new WebInspector.DebuggerModel.Location(target, uiSourceCode.url, lineNumber, 0);
        },

        isIdentity: function()
        {
            return true;
        }
    };
    target.defaultMapping = defaultMapping;
}

InspectorTest.createMockTarget = function(id)
{
    var target = {
        id: function()
        {
            return id;
        },

        addEventListener: function() { },
        removeEventListener: function() { },
        dispose: function() { },
        isDetached: function() { return !!this.detached; }
    };
    target._modelByConstructor = new Map();
    InspectorTest.initializeDefaultMappingOnTarget(target);
    target.debuggerModel = new InspectorTest.DebuggerModelMock(target, target.defaultMapping, InspectorTest.testDebuggerWorkspaceBinding);
    target.debuggerModel._target = target;
    return target;
}

InspectorTest.dumpTarget = function(targetAware)
{
    return InspectorTest.dumpTargetIds ?  "target " + targetAware.target().id() + " " : "";
}

InspectorTest.DebuggerModelMock = function(target, sourceMapping, debuggerWorkspaceBinding)
{
    WebInspector.SDKModel.call(this, WebInspector.DebuggerModel, target);
    this._breakpointResolvedEventTarget = new WebInspector.Object();
    this._scripts = {};
    this._sourceMapping = sourceMapping;
    this._breakpoints = {};
    this._debuggerWorkspaceBinding = InspectorTest.testDebuggerWorkspaceBinding;
}

InspectorTest.DebuggerModelMock.prototype = {
    target: function()
    {
        return this._target;
    },

    debuggerEnabled: function()
    {
        return true;
    },

    scriptsForSourceURL: function(url)
    {
        var script = this._scriptForURL(url);
        return script ? [script] : [];
    },

    _addScript: function(scriptId, url)
    {
        var script = new WebInspector.Script(this._target, scriptId, url);
        this._scripts[scriptId] = script;
        this._debuggerWorkspaceBinding._targetToData.get(this._target)._parsedScriptSource({data: script});
    },

    _registerScript: function(script)
    {
        this._scripts[script.scriptId] = script;
        this._debuggerWorkspaceBinding._targetToData.get(this._target)._parsedScriptSource({data: script});
    },

    _scriptForURL: function(url)
    {
        for (var scriptId in this._scripts) {
            var script = this._scripts[scriptId];
            if (script.sourceURL === url)
                return script;
        }
    },

    _scheduleSetBeakpointCallback: function(callback, breakpointId, locations)
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
    },

    createRawLocation: function(script, line, column)
    {
        return new WebInspector.DebuggerModel.Location(this.target(), script.scriptId, line, column);
    },

    setBreakpointByURL: function(url, lineNumber, columnNumber, condition, callback)
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
            var shiftedLocation = new WebInspector.DebuggerModel.Location(this._target, url, lineNumber + 10, columnNumber);
            this._scheduleSetBeakpointCallback(callback, breakpointId, [shiftedLocation]);
            return;
        }

        var locations = [];
        var script = this._scriptForURL(url);
        if (script) {
            var location = new WebInspector.DebuggerModel.Location(this._target, script.scriptId, lineNumber, 0);
            locations.push(location);
        }

        this._scheduleSetBeakpointCallback(callback, breakpointId, locations);
    },

    removeBreakpoint: function(breakpointId, callback)
    {
        InspectorTest.addResult("    " + InspectorTest.dumpTarget(this) + "debuggerModel.removeBreakpoint(" + breakpointId + ")");
        delete this._breakpoints[breakpointId];
        if (callback)
            setTimeout(callback, 0);
    },

    setBreakpointsActive: function() { },

    scriptForId: function(scriptId)
    {
        return this._scripts[scriptId];
    },

    reset: function()
    {
        InspectorTest.addResult("  Resetting debugger.");
        this._scripts = {};
        this._debuggerWorkspaceBinding._reset(this._target);
    },

    pushSourceMapping: function(sourceMapping)
    {
        for (var scriptId in this._scripts)
            this._debuggerWorkspaceBinding.pushSourceMapping(this._scripts[scriptId], sourceMapping);
    },

    disableSourceMapping: function(sourceMapping)
    {
        sourceMapping._disabled = true;
        for (var scriptId in this._scripts)
            this._debuggerWorkspaceBinding.updateLocations(this._scripts[scriptId]);
    },

    addBreakpointListener: function(breakpointId, listener, thisObject)
    {
        this._breakpointResolvedEventTarget.addEventListener(breakpointId, listener, thisObject)
    },

    removeBreakpointListener: function(breakpointId, listener, thisObject)
    {
        this._breakpointResolvedEventTarget.removeEventListener(breakpointId, listener, thisObject);
    },

    _breakpointResolved: function(breakpointId, location)
    {
        this._breakpointResolvedEventTarget.dispatchEventToListeners(breakpointId, location);
    },

    __proto__: WebInspector.Object.prototype
}

InspectorTest.setupLiveLocationSniffers = function()
{
    InspectorTest.addSniffer(WebInspector.DebuggerWorkspaceBinding.prototype, "createLiveLocation", function(rawLocation)
    {
        InspectorTest.addResult("    Location created: " + InspectorTest.dumpTarget(rawLocation) + rawLocation.scriptId + ":" + rawLocation.lineNumber);
    }, true);
    InspectorTest.addSniffer(WebInspector.DebuggerWorkspaceBinding.Location.prototype, "dispose", function()
    {
        InspectorTest.addResult("    Location disposed: " + InspectorTest.dumpTarget(this._rawLocation) + this._rawLocation.scriptId + ":" + this._rawLocation.lineNumber);
    }, true);
}

InspectorTest.addScript = function(target, breakpointManager, url)
{
    target.debuggerModel._addScript(url, url);
    InspectorTest.addResult("  Adding script: " + url);
    var uiSourceCodes = breakpointManager._workspace.uiSourceCodesForProjectType(WebInspector.projectTypes.Debugger);
    for (var i = 0; i < uiSourceCodes.length; ++i) {
        var uiSourceCode = uiSourceCodes[i];
        if (uiSourceCode.url === url) {
            breakpointManager._debuggerWorkspaceBinding.setSourceMapping(target, uiSourceCode, breakpointManager.defaultMapping);
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
    var contentProvider = new WebInspector.StaticContentProvider(WebInspector.resourceTypes.Script, "");
    var binding = breakpointManager._debuggerWorkspaceBinding;
    var uiSourceCode = binding._networkWorkspaceBinding.addFileForURL(url, contentProvider);
    InspectorTest.uiSourceCodes[url] = uiSourceCode;
    if (!doNotSetSourceMapping) {
        breakpointManager._debuggerWorkspaceBinding.setSourceMapping(target, uiSourceCode, breakpointManager.defaultMapping);
        breakpointManager._debuggerWorkspaceBinding.updateLocations(target.debuggerModel.scriptForId(url));
    }
    return uiSourceCode;
}

InspectorTest.createBreakpointManager = function(targetManager, debuggerWorkspaceBinding, persistentBreakpoints)
{
    InspectorTest._pendingBreakpointUpdates = 0;
    InspectorTest.addSniffer(WebInspector.BreakpointManager.TargetBreakpoint.prototype, "_updateInDebugger", updateInDebugger, true);
    InspectorTest.addSniffer(WebInspector.BreakpointManager.TargetBreakpoint.prototype, "_didUpdateInDebugger", didUpdateInDebugger, true);

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
        InspectorTest.addResult("    breakpointAdded(" + [uiLocation.uiSourceCode.originURL(), uiLocation.lineNumber, uiLocation.columnNumber, breakpoint.condition(), breakpoint.enabled()].join(", ") + ")");
    }

    function breakpointRemoved(event)
    {
        var uiLocation = event.data.uiLocation;
        InspectorTest.addResult("    breakpointRemoved(" + [uiLocation.uiSourceCode.originURL(), uiLocation.lineNumber, uiLocation.columnNumber].join(", ") + ")");
    }
    var targets = targetManager.targets();
    var mappingForManager;
    for (var i = 0; i < targets.length; ++i) {
        InspectorTest.initializeDefaultMappingOnTarget(targets[i]);
        if (!mappingForManager)
            mappingForManager = targets[i].defaultMapping;
        var model = new InspectorTest.DebuggerModelMock(targets[i], targets[i].defaultMapping, debuggerWorkspaceBinding);
        targets[i].debuggerModel = model;
    }

    var breakpointManager = new WebInspector.BreakpointManager(setting, debuggerWorkspaceBinding._workspace, targetManager, debuggerWorkspaceBinding);
    breakpointManager.defaultMapping = mappingForManager;
    breakpointManager.addEventListener(WebInspector.BreakpointManager.Events.BreakpointAdded, breakpointAdded);
    breakpointManager.addEventListener(WebInspector.BreakpointManager.Events.BreakpointRemoved, breakpointRemoved);
    InspectorTest.addResult("  Created breakpoints manager");
    InspectorTest.dumpBreakpointStorage(breakpointManager);
    return breakpointManager;
}

InspectorTest.setBreakpoint = function(breakpointManager, uiSourceCode, lineNumber, columnNumber, condition, enabled, setBreakpointCallback)
{
    InspectorTest.addResult("  Setting breakpoint at " + uiSourceCode.originURL() + ":" + lineNumber + ":" + columnNumber + " enabled:" + enabled + " condition:" + condition);
    if (setBreakpointCallback)
        window.setBreakpointCallback = setBreakpointCallback;
    return breakpointManager.setBreakpoint(uiSourceCode, lineNumber, columnNumber, condition, enabled);
}

InspectorTest.removeBreakpoint = function(breakpointManager, uiSourceCode, lineNumber, columnNumber)
{
    InspectorTest.addResult("  Removing breakpoint at " + uiSourceCode.originURL() + ":" + lineNumber + ":" + columnNumber);
    breakpointManager.findBreakpoint(uiSourceCode, lineNumber, columnNumber).remove();
}

InspectorTest.dumpBreakpointStorage = function(breakpointManager)
{
    var breakpoints = breakpointManager._storage._setting.get();
    InspectorTest.addResult("  Dumping Storage");
    for (var i = 0; i < breakpoints.length; ++i)
        InspectorTest.addResult("    " + breakpoints[i].sourceFileId + ":" + breakpoints[i].lineNumber  + " enabled:" + breakpoints[i].enabled + " condition:" + breakpoints[i].condition);
}

InspectorTest.dumpBreakpointLocations = function(breakpointManager)
{
    var allBreakpointLocations = breakpointManager.allBreakpointLocations();
    InspectorTest.addResult("  Dumping Breakpoint Locations");
    var lastUISourceCode = null;
    var locations = [];

    function dumpLocations(uiSourceCode, locations)
    {
        InspectorTest.addResult("    UISourceCode (url='" + uiSourceCode.url + "', uri='" + uiSourceCode.uri() + "')");
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
