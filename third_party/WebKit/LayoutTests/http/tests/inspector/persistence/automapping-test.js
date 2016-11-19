var initialize_AutomappingTest = function() {

InspectorTest.addFiles = function(testFileSystem, files)
{
    for (var filePath in files) {
        var file = files[filePath];
        testFileSystem.addFile(filePath, file.content, file.time ? file.time.getTime() : 0);
    }
}

var timeOverrides;
var originalRequestMetadata;
InspectorTest.overrideNetworkModificationTime = function(urlToTime)
{
    if (!timeOverrides) {
        timeOverrides = new Map();
        originalRequestMetadata = InspectorTest.override(Bindings.ContentProviderBasedProject.prototype, "requestMetadata", overrideTime, true);
    }
    for (var url in urlToTime)
        timeOverrides.set(url, urlToTime[url]);

    function overrideTime(uiSourceCode)
    {
        if (!timeOverrides.has(uiSourceCode.url()))
            return originalRequestMetadata.call(this, uiSourceCode);
        var override = timeOverrides.get(uiSourceCode.url());
        return originalRequestMetadata.call(this, uiSourceCode).then(onOriginalMetadata.bind(null, override));
    }

    function onOriginalMetadata(timeOverride, metadata)
    {
        if (!timeOverride && !metadata)
            return null;
        return new Workspace.UISourceCodeMetadata(timeOverride, metadata ? metadata.contentSize : null);
    }
}

InspectorTest.AutomappingTest = function(workspace)
{
    this._workspace = workspace;
    this._networkProject = new Bindings.ContentProviderBasedProject(this._workspace, "AUTOMAPPING", Workspace.projectTypes.Network, "simple website");
    if (workspace !== Workspace.workspace)
        new Persistence.FileSystemWorkspaceBinding(Workspace.isolatedFileSystemManager, this._workspace);
    this._failedBindingsCount = 0;
    this._automapping = new Persistence.Automapping(this._workspace, this._onBindingAdded.bind(this), this._onBindingRemoved.bind(this));
    InspectorTest.addSniffer(this._automapping, "_onBindingFailedForTest", this._onBindingFailed.bind(this), true);
    InspectorTest.addSniffer(this._automapping, "_onSweepHappenedForTest", this._onSweepHappened.bind(this), true);
}

InspectorTest.AutomappingTest.prototype = {
    removeResources: function(urls)
    {
        for (var url of urls)
            this._networkProject.removeFile(url);
    },

    addNetworkResources: function(assets)
    {
        for (var url in assets) {
            var asset = assets[url];
            var contentType = asset.contentType || Common.resourceTypes.Script;
            var contentProvider = new Common.StaticContentProvider(url, contentType, Promise.resolve(asset.content));
            var metadata = typeof asset.content === "string" || asset.time ? new Workspace.UISourceCodeMetadata(asset.time, asset.content.length) : null;
            var uiSourceCode = this._networkProject.createUISourceCode(url, contentType);
            this._networkProject.addUISourceCodeWithProvider(uiSourceCode, contentProvider, metadata);
        }
    },

    waitUntilMappingIsStabilized: function(callback)
    {
        this._stabilizedCallback = callback;
        this._checkStabilized();
    },

    _onSweepHappened: function()
    {
        this._failedBindingsCount = 0;
        this._checkStabilized();
    },

    _onBindingAdded: function(binding)
    {
        InspectorTest.addResult("Binding created: " + binding);
        this._checkStabilized();
    },

    _onBindingFailed: function()
    {
        ++this._failedBindingsCount;
        this._checkStabilized();
    },

    _onBindingRemoved: function(binding)
    {
        InspectorTest.addResult("Binding removed: " + binding);
        this._checkStabilized();
    },

    _checkStabilized: function()
    {
        if (!this._stabilizedCallback || this._automapping._sweepThrottler._process)
            return;
        var networkUISourceCodes = this._workspace.uiSourceCodesForProjectType(Workspace.projectTypes.Network);
        var stabilized = this._failedBindingsCount + this._automapping._bindings.size === networkUISourceCodes.length;
        if (stabilized) {
            InspectorTest.addResult("Mapping has stabilized.");
            var callback = this._stabilizedCallback;
            delete this._stabilizedCallback;
            callback.call(null);
        }
    }
}

}
