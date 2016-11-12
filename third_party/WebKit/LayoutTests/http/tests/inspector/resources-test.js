var initialize_ResourceTest = function() {

InspectorTest.preloadPanel("sources");
InspectorTest.preloadPanel("resources");

InspectorTest.requestURLComparer = function(r1, r2)
{
    return r1.request.url.localeCompare(r2.request.url);
}

InspectorTest.runAfterCachedResourcesProcessed = function(callback)
{
    if (!InspectorTest.resourceTreeModel._cachedResourcesProcessed)
        InspectorTest.resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.CachedResourcesLoaded, callback);
    else
        callback();
}

InspectorTest.runAfterResourcesAreFinished = function(resourceURLs, callback)
{
    var resourceURLsMap = new Set(resourceURLs);

    function checkResources()
    {
        for (var url of resourceURLsMap) {
            var resource = InspectorTest.resourceMatchingURL(url);
            if (resource)
                resourceURLsMap.delete(url);
        }
        if (!resourceURLsMap.size) {
            InspectorTest.resourceTreeModel.removeEventListener(SDK.ResourceTreeModel.Events.ResourceAdded, checkResources);
            callback();
        }
    }
    checkResources();
    if (resourceURLsMap.size)
        InspectorTest.resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.ResourceAdded, checkResources);
}

InspectorTest.showResource = function(resourceURL, callback)
{
    var reported = false;
    function callbackWrapper(sourceFrame)
    {
        if (reported)
            return;
        callback(sourceFrame);
        reported = true;
    }

    function showResourceCallback()
    {
        var resource = InspectorTest.resourceMatchingURL(resourceURL);
        if (!resource)
            return;
        UI.panels.resources.showResource(resource, 1);
        var sourceFrame = UI.panels.resources._resourceViewForResource(resource);
        if (sourceFrame.loaded)
            callbackWrapper(sourceFrame);
        else
            InspectorTest.addSniffer(sourceFrame, "onTextEditorContentSet", callbackWrapper.bind(null, sourceFrame));
    }
    InspectorTest.runAfterResourcesAreFinished([resourceURL], showResourceCallback);
}

InspectorTest.resourceMatchingURL = function(resourceURL)
{
    var result = null;
    InspectorTest.resourceTreeModel.forAllResources(visit);
    function visit(resource)
    {
        if (resource.url.indexOf(resourceURL) !== -1) {
            result = resource;
            return true;
        }
    }
    return result;
}

InspectorTest.databaseModel = function()
{
    return Resources.DatabaseModel.fromTarget(InspectorTest.mainTarget);
}

InspectorTest.domStorageModel = function()
{
    return Resources.DOMStorageModel.fromTarget(InspectorTest.mainTarget);
}

InspectorTest.indexedDBModel = function()
{
    return Resources.IndexedDBModel.fromTarget(InspectorTest.mainTarget);
}

}
