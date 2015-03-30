var initialize_ResourceTest = function() {

InspectorTest.preloadPanel("resources");

InspectorTest.requestURLComparer = function(r1, r2)
{
    return r1.request.url.localeCompare(r2.request.url);
}

InspectorTest.runAfterCachedResourcesProcessed = function(callback)
{
    if (!InspectorTest.resourceTreeModel._cachedResourcesProcessed)
        InspectorTest.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.CachedResourcesLoaded, callback);
    else
        callback();
}

InspectorTest.runAfterResourcesAreFinished = function(resourceURLs, callback)
{
    var resourceURLsMap = resourceURLs.keySet();

    function checkResources()
    {
        for (url in resourceURLsMap) {
            var resource = InspectorTest.resourceMatchingURL(url);
            if (resource)
                delete resourceURLsMap[url];
        }
        if (!Object.keys(resourceURLsMap).length) {
            InspectorTest.resourceTreeModel.removeEventListener(WebInspector.ResourceTreeModel.EventTypes.ResourceAdded, checkResources);
            callback();
        }
    }
    checkResources();
    if (Object.keys(resourceURLsMap).length)
        InspectorTest.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.ResourceAdded, checkResources);
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
        WebInspector.panels.resources.showResource(resource, 1);
        var sourceFrame = WebInspector.panels.resources._resourceViewForResource(resource);
        if (sourceFrame.loaded)
            callbackWrapper(sourceFrame);
        else
            InspectorTest.addSniffer(sourceFrame, "onTextEditorContentLoaded", callbackWrapper.bind(null, sourceFrame));
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
    return WebInspector.DatabaseModel.fromTarget(InspectorTest.mainTarget);
}

InspectorTest.domStorageModel = function()
{
    return WebInspector.DOMStorageModel.fromTarget(InspectorTest.mainTarget);
}

InspectorTest.indexedDBModel = function()
{
    return WebInspector.IndexedDBModel.fromTarget(InspectorTest.mainTarget);
}

}
