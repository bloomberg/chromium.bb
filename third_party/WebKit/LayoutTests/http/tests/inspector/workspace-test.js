var initialize_WorkspaceTest = function() {

InspectorTest.createWorkspace = function(ignoreEvents)
{
    if (InspectorTest.testFileSystemWorkspaceBinding)
        InspectorTest.testFileSystemWorkspaceBinding.dispose();
    if (InspectorTest.testNetworkMapping)
        InspectorTest.testNetworkMapping.dispose();
    WebInspector.fileSystemMapping.resetForTesting();

    InspectorTest.testTargetManager = new WebInspector.TargetManager();
    InspectorTest.testWorkspace = new WebInspector.Workspace();
    InspectorTest.testFileSystemWorkspaceBinding = new WebInspector.FileSystemWorkspaceBinding(WebInspector.isolatedFileSystemManager, InspectorTest.testWorkspace);
    InspectorTest.testNetworkMapping = new WebInspector.NetworkMapping(InspectorTest.testTargetManager, InspectorTest.testWorkspace, InspectorTest.testFileSystemWorkspaceBinding, WebInspector.fileSystemMapping);
    InspectorTest.testNetworkProjectManager = new WebInspector.NetworkProjectManager(InspectorTest.testTargetManager, InspectorTest.testWorkspace, InspectorTest.testNetworkMapping);
    InspectorTest.testDebuggerWorkspaceBinding = new WebInspector.DebuggerWorkspaceBinding(InspectorTest.testTargetManager, InspectorTest.testWorkspace, InspectorTest.testNetworkMapping);
    InspectorTest.testCSSWorkspaceBinding = new WebInspector.CSSWorkspaceBinding(InspectorTest.testTargetManager, InspectorTest.testWorkspace, InspectorTest.testNetworkMapping);

    InspectorTest.testTargetManager.observeTargets({
        targetAdded: function(target)
        {
            InspectorTest.testNetworkProject = WebInspector.NetworkProject.forTarget(target);
        },

        targetRemoved: function(target)
        {
        }
    });

    if (ignoreEvents)
        return;
    InspectorTest.testWorkspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeAdded, InspectorTest._defaultWorkspaceEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeRemoved, InspectorTest._defaultWorkspaceEventHandler);
}

InspectorTest._mockTargetId = 1;

InspectorTest.createMockTarget = function(id, debuggerModelConstructor)
{
    var MockTarget = function(name, connection, callback)
    {
        WebInspector.Target.call(this, InspectorTest.testTargetManager, name, WebInspector.Target.Type.Page, connection, null, callback);
        this.consoleModel = new WebInspector.ConsoleModel(this);
        this.networkManager = new WebInspector.NetworkManager(this);
        this.resourceTreeModel = new WebInspector.ResourceTreeModel(this);
        this.resourceTreeModel._inspectedPageURL = InspectorTest.resourceTreeModel._inspectedPageURL;
        this.resourceTreeModel._cachedResourcesProcessed = true;
        this.resourceTreeModel._frameAttached("42", 0);
        this.runtimeModel = new WebInspector.RuntimeModel(this);
        this.debuggerModel = debuggerModelConstructor ? new debuggerModelConstructor(this) : new WebInspector.DebuggerModel(this);
        this._modelByConstructor.set(WebInspector.DebuggerModel, this.debuggerModel);
        this.domModel = new WebInspector.DOMModel(this);
        this.cssModel = new WebInspector.CSSModel(this);
    }

    MockTarget.prototype = {
        _loadedWithCapabilities: function()
        {
        },

        __proto__: WebInspector.Target.prototype
    }

    var target = new MockTarget("mock-target-" + id, new WebInspector.StubConnection());
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
    InspectorTest.testWorkspace.removeEventListener(WebInspector.Workspace.Events.UISourceCodeAdded, InspectorTest._defaultWorkspaceEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);

    function uiSourceCodeAdded(event)
    {
        if (projectType && event.data.project().type() !== projectType)
            return;
        if (!projectType && event.data.project().type() === WebInspector.projectTypes.Service)
            return;
        if (!(--InspectorTest.uiSourceCodeAddedEventsLeft)) {
            InspectorTest.testWorkspace.removeEventListener(WebInspector.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);
            InspectorTest.testWorkspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeAdded, InspectorTest._defaultWorkspaceEventHandler);
        }
        callback(event.data);
    }
}

InspectorTest.waitForWorkspaceUISourceCodeRemovedEvent = function(callback, count)
{
    InspectorTest.uiSourceCodeRemovedEventsLeft = count || 1;
    InspectorTest.testWorkspace.removeEventListener(WebInspector.Workspace.Events.UISourceCodeRemoved, InspectorTest._defaultWorkspaceEventHandler);
    InspectorTest.testWorkspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeRemoved, uiSourceCodeRemoved);

    function uiSourceCodeRemoved(event)
    {
        if (event.data.project().type() === WebInspector.projectTypes.Service)
            return;
        if (!(--InspectorTest.uiSourceCodeRemovedEventsLeft)) {
            InspectorTest.testWorkspace.removeEventListener(WebInspector.Workspace.Events.UISourceCodeRemoved, uiSourceCodeRemoved);
            InspectorTest.testWorkspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeRemoved, InspectorTest._defaultWorkspaceEventHandler);
        }
        callback(event.data);
    }
}

InspectorTest.addMockUISourceCodeToWorkspace = function(url, type, content)
{
    var mockContentProvider = new WebInspector.StaticContentProvider(url, type, Promise.resolve(content));
    InspectorTest.testNetworkProject.addFile(mockContentProvider, null, false);
}

InspectorTest.addMockUISourceCodeViaNetwork = function(url, type, content, target)
{
    var mockContentProvider = new WebInspector.StaticContentProvider(url, type, Promise.resolve(content));
    InspectorTest.testNetworkProject.addFile(mockContentProvider, target.resourceTreeModel.mainFrame, false);
}

InspectorTest._defaultWorkspaceEventHandler = function(event)
{
    var uiSourceCode = event.data;
    var networkURL = WebInspector.networkMapping.networkURL(uiSourceCode);
    if (uiSourceCode.project().type() === WebInspector.projectTypes.Debugger && !networkURL)
        return;
    if (uiSourceCode.project().type() === WebInspector.projectTypes.Service)
        return;
    InspectorTest.addResult("Workspace event: " + event.type + ": " + uiSourceCode.url() + ".");
}

InspectorTest.uiSourceCodeURL = function(uiSourceCode)
{
    return uiSourceCode.url().replace(/.*LayoutTests/, "LayoutTests");
}

InspectorTest.dumpUISourceCode = function(uiSourceCode, callback)
{
    InspectorTest.addResult("UISourceCode: " + InspectorTest.uiSourceCodeURL(uiSourceCode));
    if (uiSourceCode.contentType() === WebInspector.resourceTypes.Script || uiSourceCode.contentType() === WebInspector.resourceTypes.Document)
        InspectorTest.addResult("UISourceCode is content script: " + (uiSourceCode.project().type() === WebInspector.projectTypes.ContentScripts));
    uiSourceCode.requestContent().then(didRequestContent);

    function didRequestContent(content, contentEncoded)
    {
        InspectorTest.addResult("Highlighter type: " + WebInspector.NetworkProject.uiSourceCodeMimeType(uiSourceCode));
        InspectorTest.addResult("UISourceCode content: " + content);
        callback();
    }
}

InspectorTest.fileSystemUISourceCodes = function()
{
    var uiSourceCodes = [];
    var fileSystemProjects = WebInspector.workspace.projectsForType(WebInspector.projectTypes.FileSystem);
    for (var project of fileSystemProjects)
        uiSourceCodes = uiSourceCodes.concat(project.uiSourceCodes());
    return uiSourceCodes;
}

InspectorTest.refreshFileSystemProjects = function(callback)
{
    var barrier = new CallbackBarrier();
    var projects = WebInspector.workspace.projects();
    for (var i = 0; i < projects.length; ++i)
        projects[i].refresh("/", barrier.createCallback());
    barrier.callWhenDone(callback);
}

InspectorTest.waitForGivenUISourceCode = function(name, callback)
{
    var uiSourceCodes = WebInspector.workspace.uiSourceCodes();
    for (var i = 0; i < uiSourceCodes.length; ++i) {
        if (uiSourceCodes[i].name() === name) {
            setImmediate(callback);
            return;
        }
    }
    WebInspector.workspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);

    function uiSourceCodeAdded(event)
    {
        if (event.data.name() === name) {
            WebInspector.workspace.removeEventListener(WebInspector.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);
            setImmediate(callback);
        }
    }
}

};
