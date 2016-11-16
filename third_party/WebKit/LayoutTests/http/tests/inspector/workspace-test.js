var initialize_WorkspaceTest = function() {

InspectorTest.createWorkspace = function(ignoreEvents)
{
    if (InspectorTest.testFileSystemWorkspaceBinding)
        InspectorTest.testFileSystemWorkspaceBinding.dispose();
    Workspace.fileSystemMapping.resetForTesting();

    InspectorTest.testTargetManager = new SDK.TargetManager();
    InspectorTest.testWorkspace = new Workspace.Workspace();
    InspectorTest.testFileSystemWorkspaceBinding = new Bindings.FileSystemWorkspaceBinding(Workspace.isolatedFileSystemManager, InspectorTest.testWorkspace);
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
    var MockTarget = class extends SDK.Target {
        constructor(name, connectionFactory, callback) {
            super(InspectorTest.testTargetManager, name, capabilities, connectionFactory, null, callback);
            this._inspectedURL = InspectorTest.mainTarget.inspectedURL();
            this.consoleModel = new SDK.ConsoleModel(this);
            this.networkManager = new SDK.NetworkManager(this);
            this.runtimeModel = new SDK.RuntimeModel(this);
            this.securityOriginManager = SDK.SecurityOriginManager.fromTarget(this);
            this.resourceTreeModel = new SDK.ResourceTreeModel(this, this.networkManager, this.securityOriginManager);
            this.resourceTreeModel._cachedResourcesProcessed = true;
            this.resourceTreeModel._frameAttached("42", 0);
            this.debuggerModel = debuggerModelConstructor ? new debuggerModelConstructor(this) : new SDK.DebuggerModel(this);
            this._modelByConstructor.set(SDK.DebuggerModel, this.debuggerModel);
            this.domModel = new SDK.DOMModel(this);
            this.cssModel = new SDK.CSSModel(this, this.domModel);
        }

        _loadedWithCapabilities()
        {
        }
    };

    var target = new MockTarget("mock-target-" + id, (params) => new SDK.StubConnection(params));
    InspectorTest.testTargetManager.addTarget(target);
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

};
