function initialize_DataGridTest()
{

InspectorTest.preloadModule("ui_lazy");

InspectorTest.dumpDataGrid = function(root, descentIntoCollapsed, prefix)
{
    if (!prefix)
        prefix = "";
    if (root.data.id)
        InspectorTest.addResult(prefix + root.data.id);
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
}

InspectorTest.dumpAndValidateDataGrid = function(root)
{
    InspectorTest.dumpDataGrid(root);
    InspectorTest.validateDataGrid(root);
}

}
