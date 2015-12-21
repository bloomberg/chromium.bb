function initialize_DataGridTest()
{

InspectorTest.preloadModule("ui_lazy");

InspectorTest.dumpDataGrid = function(root, descentIntoCollapsed, prefix)
{
    if (!prefix)
        prefix = "";
    var suffix = root.selected ? " <- selected" : "";
    if (root.data.id)
        InspectorTest.addResult(prefix + root.data.id + suffix);
    if (!descentIntoCollapsed && !root.expanded)
        return;
    for (var child of root.children)
        InspectorTest.dumpDataGrid(child, descentIntoCollapsed, prefix + " ");
}

InspectorTest.validateDataGrid = function(root)
{
    var children = root.children;
    for (var i = 0; i < children.length; ++i) {
        var child = children[i];
        if (child.parent !== root)
            throw "Wrong parent for child " + child.data.id + " of " + root.data.id;
        if (child.nextSibling !== (i + 1 === children.length ? null : children[i + 1]))
            throw "Wrong child.nextSibling for " + child.data.id + " (" + i + " of " + children.length + ") ";
        if (child.previousSibling !== (i ? children[i - 1] : null))
            throw "Wrong child.previousSibling for " + child.data.id + " (" + i + " of " + children.length + ") ";
        if (child.parent && !child.parent._isRoot && child.depth !== root.depth + 1)
            throw "Wrong depth for " + child.data.id + " expected " + (root.depth + 1) + " but got " + child.depth;
        InspectorTest.validateDataGrid(child);
    }
    var selectedNode = root.dataGrid.selectedNode;
    if (!root.parent && selectedNode) {
        for (var node = selectedNode; node && node !== root; node = node.parent) { }
        if (!node)
            throw "Selected node (" + selectedNode.data.id + ") is not within the DataGrid";
    }
}

InspectorTest.dumpAndValidateDataGrid = function(root)
{
    InspectorTest.dumpDataGrid(root);
    InspectorTest.validateDataGrid(root);
}

}
