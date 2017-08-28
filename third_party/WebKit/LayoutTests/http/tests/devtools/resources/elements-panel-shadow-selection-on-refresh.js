var initialize_ElementPanelShadowSelection = function() {

InspectorTest.selectReloadAndDump = function(next, node)
{
    InspectorTest.selectNode(node).then(onSelected);
    var reloaded = false;
    var selected = false;

    function onSelected()
    {
        InspectorTest.reloadPage(onReloaded);
        InspectorTest.addSniffer(Elements.ElementsPanel.prototype, "_lastSelectedNodeSelectedForTest", onReSelected);
    }

    function onReloaded()
    {
        reloaded = true;
        maybeDumpSelectedNode();
    }

    function onReSelected()
    {
        selected = true;
        maybeDumpSelectedNode();
    }

    function maybeDumpSelectedNode()
    {
        if (!reloaded || !selected)
            return;
        var selectedElement = InspectorTest.firstElementsTreeOutline().selectedTreeElement;
        var nodeName = selectedElement ? selectedElement.node().nodeNameInCorrectCase() : "null";
        InspectorTest.addResult("Selected node: '" + nodeName + "'");
        next();
    }
}

}

