var initialize_SecurityTest = function() {

InspectorTest.preloadPanel("security");

InspectorTest.dumpSecurityPanelSidebarOrigins = function() {
    for (var key in WebInspector.SecurityPanelSidebarTree.OriginGroupName) {
        var originGroupName = WebInspector.SecurityPanelSidebarTree.OriginGroupName[key];
        var originTitles = WebInspector.SecurityPanel._instance()._sidebarTree._originGroups.get(originGroupName).childrenListElement.getElementsByClassName("title");
        if (originTitles.length > 0) {
            InspectorTest.addResult("Group: " + originGroupName);
            for (var originTitle of originTitles)
                InspectorTest.dumpDeepInnerHTML(originTitle);
        }
    }
}

/**
 * @param {!WebInspector.NetworkRequest} request
 */
InspectorTest.dispatchRequestFinished = function(request) {
    InspectorTest.networkManager.dispatchEventToListeners(WebInspector.NetworkManager.EventTypes.RequestFinished, request);
}

}
