var initialize_AccessibilityTest = function() {

InspectorTest.accessibilitySidebarPane = function()
{
    return self.runtime.sharedInstance(Accessibility.AccessibilitySidebarView);
}

/**
 * @param {string} idValue
 * @return {Promise}
 */
InspectorTest.selectNodeAndWaitForAccessibility = function(idValue)
{
    return new Promise((resolve) => {
        InspectorTest.selectNodeWithId(idValue, function() {
            self.runtime.sharedInstance(Accessibility.AccessibilitySidebarView).doUpdate().then(resolve);
        });
    });
}

InspectorTest.dumpSelectedElementAccessibilityNode = function()
{
    var sidebarPane = InspectorTest.accessibilitySidebarPane();

    if (!sidebarPane) {
        InspectorTest.addResult("No sidebarPane in dumpSelectedElementAccessibilityNode");
        InspectorTest.completeTest();
        return;
    }
    InspectorTest.dumpAccessibilityNode(sidebarPane._axNodeSubPane._axNode);
}

/**
 * @param {!Accessibility.AccessibilityNode} accessibilityNode
 */
InspectorTest.dumpAccessibilityNode = function(accessibilityNode)
{
    if (!accessibilityNode) {
        InspectorTest.addResult("<null>");
        InspectorTest.completeTest();
        return;
    }

    var builder = [];
    builder.push(accessibilityNode.role().value);
    builder.push(accessibilityNode.name() ? '"' + accessibilityNode.name().value + '"'
                : "<undefined>");
    if (accessibilityNode.properties()) {
        for (var property of accessibilityNode.properties()) {
            if ("value" in property)
                builder.push(property.name + '="' + property.value.value + '"');
        }
    }
    InspectorTest.addResult(builder.join(" "));
}

/**
 * @param {string} attribute
 * @return {?Accessibility.ARIAAttributesTreeElement}
 */
InspectorTest.findARIAAttributeTreeElement = function(attribute)
{
    var sidebarPane = InspectorTest.accessibilitySidebarPane();

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

};
