var initialize_SecurityTest = function() {

InspectorTest.preloadPanel("security");

InspectorTest.dumpSecurityPanelSidebarOrigins = function() {
    for (var key in WebInspector.SecurityPanelSidebarTree.OriginGroupName) {
        var originGroupName = WebInspector.SecurityPanelSidebarTree.OriginGroupName[key];
        var originGroup = WebInspector.SecurityPanel._instance()._sidebarTree._originGroups.get(originGroupName);
        if (originGroup.hidden)
            continue;
        InspectorTest.addResult("Group: " + originGroupName);
        var originTitles = originGroup.childrenListElement.getElementsByClassName("title");
        for (var originTitle of originTitles)
            InspectorTest.dumpDeepInnerHTML(originTitle);
    }
}

/**
 * @param {!WebInspector.NetworkRequest} request
 */
InspectorTest.dispatchRequestFinished = function(request) {
    InspectorTest.networkManager.dispatchEventToListeners(WebInspector.NetworkManager.Events.RequestFinished, request);
}

}
