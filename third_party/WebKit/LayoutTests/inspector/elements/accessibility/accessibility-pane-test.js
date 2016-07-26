var initialize_AccessibilityTest = function() {

/**
 * @return {Promise}
 */
InspectorTest.showAccessibilityView = function()
{
    var sidebarPane = _getAccessibilitySidebarPane();
    if (sidebarPane) {
        sidebarPane.revealWidget();
        return InspectorTest.waitForAccessibilityNodeUpdate();
    } else {
        return _waitForViewsLoaded()
            .then(() => {
                return InspectorTest.showAccessibilityView();
            });
    }
}


/**
 * @return {Promise}
 */
function _waitForViewsLoaded()
{
    return new Promise(function(resolve, reject)
    {
        InspectorTest.addSniffer(WebInspector.ElementsPanel.prototype,
                                 "_sidebarViewsLoadedForTest",
                                 resolve);
        WebInspector.panels.elements._loadSidebarViews();
    });
}

function _getAccessibilitySidebarPane()
{
    var sidebarViews = WebInspector.panels.elements._elementsSidebarViews;
    var sidebarPane = sidebarViews.find((view) => { return view._title === "Accessibility"; });
    return sidebarPane;
}

/**
 * @param {string} idValue
 * @return {Promise}
 */
InspectorTest.selectNodeAndWaitForAccessibility = function(idValue)
{
    return Promise.all([InspectorTest.waitForAccessibilityNodeUpdateInARIAPane(),
                        InspectorTest.waitForAccessibilityNodeUpdate(),
                        new Promise(function(resolve) {
                            InspectorTest.selectNodeWithId(idValue, resolve);
                        })]);
}

InspectorTest.dumpSelectedElementAccessibilityNode = function()
{
    var sidebarPane = _getAccessibilitySidebarPane();

    if (!sidebarPane) {
        InspectorTest.addResult("No sidebarPane in dumpSelectedElementAccessibilityNode");
        InspectorTest.completeTest();
        return;
    }
    InspectorTest.dumpAccessibilityNode(sidebarPane._axNodeSubPane._axNode);
}

/**
 * @param {!AccessibilityAgent.AXNode} accessibilityNode
 */
InspectorTest.dumpAccessibilityNode = function(accessibilityNode)
{
    if (!accessibilityNode) {
        InspectorTest.addResult("<null>");
        InspectorTest.completeTest();
        return;
    }

    var builder = [];
    builder.push(accessibilityNode.role.value);
    builder.push(accessibilityNode.name ? '"' + accessibilityNode.name.value + '"'
                : "<undefined>");
    if ("properties" in accessibilityNode) {
        for (var property of accessibilityNode.properties) {
            if ("value" in property)
                builder.push(property.name + '="' + property.value.value + '"');
        }
    }
    InspectorTest.addResult(builder.join(" "));
}

/**
 * @param {string} attribute
 * @return {?WebInspector.ARIAAttributesTreeElement}
 */
InspectorTest.findARIAAttributeTreeElement = function(attribute)
{
    var sidebarPane = _getAccessibilitySidebarPane();

    if (!sidebarPane) {
        InspectorTest.addResult("Could not get Accessibility sidebar pane.");
        InspectorTest.completeTest();
        return;
    }

    var ariaSubPane = sidebarPane._ariaSubPane;
    var treeOutline = ariaSubPane._treeOutline;
    var childNodes = treeOutline._rootElement._children;
    for (var treeElement of childNodes) {
        if (treeElement._attribute.name === attribute)
            return treeElement;
    }
    return null;
}

/**
 * @return {Promise}
 */
InspectorTest.waitForAccessibilityNodeUpdate = function()
{
    return new Promise(function(resolve, reject)
    {
        InspectorTest.addSniffer(WebInspector.AccessibilitySidebarView.prototype, "_accessibilityNodeUpdatedForTest", resolve);
    });
}

/**
 * @return {Promise}
 */
InspectorTest.waitForAccessibilityNodeUpdateInARIAPane = function()
{
    return new Promise(function(resolve, reject)
    {
        InspectorTest.addSniffer(WebInspector.ARIAAttributesPane.prototype, "_gotNodeForTest", resolve);
    });
}
};
