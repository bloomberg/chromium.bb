var initialize_WorkspaceTest = function() {

InspectorTest.createWorkspace = function(ignoreEvents)
{
    WebInspector.settings.createSetting("fileSystemMapping", {}).set({});
    InspectorTest.testFileSystemMapping = new WebInspector.FileSystemMapping();
    InspectorTest.testExcludedFolderManager = new WebInspector.ExcludedFolderManager();
    InspectorTest.testFileSystemMapping._fileSystemMappingSetting = new InspectorTest.MockSetting({});
    InspectorTest.testFileSystemMapping._excludedFoldersSetting = new InspectorTest.MockSetting({});

    InspectorTest.testTargetManager = new WebInspector.TargetManager();
    InspectorTest.testWorkspace = new WebInspector.Workspace(InspectorTest.testFileSystemMapping);
    InspectorTest.testNetworkMapping = new WebInspector.NetworkMapping(InspectorTest.testWorkspace, InspectorTest.testFileSystemMapping);
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
        WebInspector.Target.call(this, name, WebInspector.Target.Type.Page, connection, null, callback);
        this.debuggerModel = debuggerModelConstructor ? new debuggerModelConstructor(this) : new WebInspector.DebuggerModel(this);
        this.resourceTreeModel = WebInspector.targetManager.mainTarget().resourceTreeModel;
        this.domModel = new WebInspector.DOMModel(this);
        this.cssModel = new WebInspector.CSSStyleModel(this);
        this.runtimeModel = new WebInspector.RuntimeModel(this);
        this.consoleModel = new WebInspector.ConsoleModel(this);
    }

    MockTarget.prototype = {
        _loadedWithCapabilities: function()
        {
        },

        __proto__: WebInspector.Target.prototype
    }

    var target = new MockTarget("mock-target-" + id, new InspectorBackendClass.StubConnection());
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
    var mockContentProvider = new WebInspector.StaticContentProvider(type, content);
    InspectorTest.testNetworkProject.addFileForURL(url, mockContentProvider);
}

InspectorTest._defaultWorkspaceEventHandler = function(event)
{
    var uiSourceCode = event.data;
    var networkURL = WebInspector.networkMapping.networkURL(uiSourceCode);
    if (uiSourceCode.project().type() === WebInspector.projectTypes.Debugger && !networkURL)
        return;
    if (uiSourceCode.project().type() === WebInspector.projectTypes.Service)
        return;
    InspectorTest.addResult("Workspace event: " + event.type + ": " + uiSourceCode.uri() + ".");
}

InspectorTest.uiSourceCodeURL = function(uiSourceCode)
{
    return uiSourceCode.originURL().replace(/.*LayoutTests/, "LayoutTests");
}

InspectorTest.dumpUISourceCode = function(uiSourceCode, callback)
{
    InspectorTest.addResult("UISourceCode: " + InspectorTest.uiSourceCodeURL(uiSourceCode));
    if (uiSourceCode.contentType() === WebInspector.resourceTypes.Script || uiSourceCode.contentType() === WebInspector.resourceTypes.Document)
        InspectorTest.addResult("UISourceCode is content script: " + (uiSourceCode.project().type() === WebInspector.projectTypes.ContentScripts));
    uiSourceCode.requestContent(didRequestContent);

    function didRequestContent(content, contentEncoded)
    {
        InspectorTest.addResult("Highlighter type: " + WebInspector.SourcesView.uiSourceCodeHighlighterType(uiSourceCode));
        InspectorTest.addResult("UISourceCode content: " + content);
        callback();
    }
}

};
