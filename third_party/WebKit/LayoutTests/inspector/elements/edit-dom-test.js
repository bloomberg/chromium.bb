function initialize_EditDOMTests()
{

InspectorTest.doAddAttribute = function(testName, dataNodeId, attributeText, next)
{
    InspectorTest.domActionTestForNodeId(testName, dataNodeId, testBody, next);

    function testBody(node, done)
    {
        var editorElement = InspectorTest.editNodePart(node, "webkit-html-attribute");
        editorElement.dispatchEvent(InspectorTest.createKeyEvent("U+0009")); // Tab

        InspectorTest.runAfterPendingDispatches(testContinuation);

        function testContinuation()
        {
            var editorElement = WebInspector.panels.elements._treeOutlines[0]._shadowRoot.getSelection().anchorNode.parentElement;
            editorElement.textContent = attributeText;
            editorElement.dispatchEvent(InspectorTest.createKeyEvent("Enter"));
            InspectorTest.addSniffer(WebInspector.ElementsTreeUpdater.prototype, "_updateModifiedNodes", done);
        }
    }
}

InspectorTest.domActionTestForNodeId = function(testName, dataNodeId, testBody, next)
{
    function callback(testNode, continuation)
    {
        InspectorTest.selectNodeWithId(dataNodeId, continuation);
    }
    InspectorTest.domActionTest(testName, callback, testBody, next);
}

InspectorTest.domActionTest = function(testName, dataNodeSelectionCallback, testBody, next)
{
    var testNode = InspectorTest.expandedNodeWithId(testName);
    InspectorTest.addResult("==== before ====");
    InspectorTest.dumpElementsTree(testNode);

    dataNodeSelectionCallback(testNode, step0);

    function step0(node)
    {
        InspectorTest.runAfterPendingDispatches(step1.bind(null, node));
    }

    function step1(node)
    {
        testBody(node, step2);
    }

    function step2()
    {
        InspectorTest.addResult("==== after ====");
        InspectorTest.dumpElementsTree(testNode);
        next();
    }
}

InspectorTest.editNodePart = function(node, className)
{
    var treeElement = InspectorTest.firstElementsTreeOutline().findTreeElement(node);
    var textElement = treeElement.listItemElement.getElementsByClassName(className)[0];
    if (!textElement && treeElement.childrenListElement)
        textElement = treeElement.childrenListElement.getElementsByClassName(className)[0];
    treeElement._startEditingTarget(textElement);
    return textElement;
}

InspectorTest.editNodePartAndRun = function(node, className, newValue, step2, useSniffer)
{
    var editorElement = InspectorTest.editNodePart(node, className);
    editorElement.textContent = newValue;
    editorElement.dispatchEvent(InspectorTest.createKeyEvent("Enter"));
    if (useSniffer)
        InspectorTest.addSniffer(WebInspector.ElementsTreeUpdater.prototype, "_updateModifiedNodes", step2);
    else
        InspectorTest.runAfterPendingDispatches(step2);
}

}
