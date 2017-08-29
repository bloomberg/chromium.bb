var initialize_HeapSnapshotTest = function() {

InspectorTest.preloadPanel("heap_profiler");

InspectorTest.createHeapSnapshotMockFactories = function() {

InspectorTest.createJSHeapSnapshotMockObject = function()
{
    return {
        _rootNodeIndex: 0,
        _nodeTypeOffset: 0,
        _nodeNameOffset: 1,
        _nodeEdgeCountOffset: 2,
        _nodeFieldCount: 3,
        _edgeFieldsCount: 3,
        _edgeTypeOffset: 0,
        _edgeNameOffset: 1,
        _edgeToNodeOffset: 2,
        _nodeTypes: ["hidden", "object"],
        _edgeTypes: ["element", "property", "shortcut"],
        _edgeShortcutType: -1,
        _edgeHiddenType: -1,
        _edgeElementType: 0,
        _realNodesLength: 18,
        // Represents the following graph:
        //   (numbers in parentheses indicate node offset)
        //
        //         -> A (3) --ac- C (9) -ce- E(15)
        //       a/   |          /
        //  "" (0)    1    - bc -
        //       b\   v   /
        //         -> B (6) -bd- D (12)
        //
        nodes: new Uint32Array([
            0, 0, 2,    //  0: root
            1, 1, 2,    //  3: A
            1, 2, 2,    //  6: B
            1, 3, 1,    //  9: C
            1, 4, 0,    // 12: D
            1, 5, 0]),  // 15: E
        containmentEdges: new Uint32Array([
            2,  6, 3,   //  0: shortcut 'a' to node 'A'
            1,  7, 6,   //  3: property 'b' to node 'B'
            0,  1, 6,   //  6: element '1' to node 'B'
            1,  8, 9,   //  9: property 'ac' to node 'C'
            1,  9, 9,   // 12: property 'bc' to node 'C'
            1, 10, 12,  // 15: property 'bd' to node 'D'
            1, 11, 15]),// 18: property 'ce' to node 'E'
        strings: ["", "A", "B", "C", "D", "E", "a", "b", "ac", "bc", "bd", "ce"],
        _firstEdgeIndexes: new Uint32Array([0, 6, 12, 18, 21, 21, 21]),
        createNode: HeapSnapshotWorker.JSHeapSnapshot.prototype.createNode,
        createEdge: HeapSnapshotWorker.JSHeapSnapshot.prototype.createEdge,
        createRetainingEdge: HeapSnapshotWorker.JSHeapSnapshot.prototype.createRetainingEdge
    };
};

InspectorTest.createHeapSnapshotMockRaw = function()
{
    // Effectively the same graph as in createJSHeapSnapshotMockObject,
    // but having full set of fields.
    //
    // A triple in parentheses indicates node index, self size and
    // retained size.
    //
    //          --- A (7,2,2) --ac- C (21,4,10) -ce- E(35,6,6)
    //         /    |                /
    //  "" (0,0,20) 1       --bc-----
    //         \    v      /
    //          --- B (14,3,8) --bd- D (28,5,5)
    //
    return {
        snapshot: {
            meta: {
                node_fields: ["type", "name", "id", "self_size", "retained_size", "dominator", "edge_count"],
                node_types: [["hidden", "object"], "", "", "", "", "", ""],
                edge_fields: ["type", "name_or_index", "to_node"],
                edge_types: [["element", "property", "shortcut"], "", ""]
            },
            node_count: 6,
            edge_count: 7},
        nodes: [
            0, 0, 1, 0, 20,  0, 2,  // root (0)
            1, 1, 2, 2,  2,  0, 2,  // A (7)
            1, 2, 3, 3,  8,  0, 2,  // B (14)
            1, 3, 4, 4, 10,  0, 1,  // C (21)
            1, 4, 5, 5,  5, 14, 0,  // D (28)
            1, 5, 6, 6,  6, 21, 0], // E (35)
        edges: [
            // root node edges
            1,  6,  7, // property 'a' to node 'A'
            1,  7, 14, // property 'b' to node 'B'

            // A node edges
            0,  1, 14, // element 1 to node 'B'
            1,  8, 21, // property 'ac' to node 'C'

            // B node edges
            1,  9, 21, // property 'bc' to node 'C'
            1, 10, 28, // property 'bd' to node 'D'

            // C node edges
            1, 11, 35], // property 'ce' to node 'E'
        strings: ["", "A", "B", "C", "D", "E", "a", "b", "ac", "bc", "bd", "ce"]
    };
};

InspectorTest._postprocessHeapSnapshotMock = function(mock)
{
    mock.nodes = new Uint32Array(mock.nodes);
    mock.edges = new Uint32Array(mock.edges);
    return mock;
};

InspectorTest.createHeapSnapshotMock = function()
{
    return InspectorTest._postprocessHeapSnapshotMock(InspectorTest.createHeapSnapshotMockRaw());
};

InspectorTest.createHeapSnapshotMockWithDOM = function()
{
    return InspectorTest._postprocessHeapSnapshotMock({
        snapshot: {
            meta: {
                node_fields: ["type", "name", "id", "edge_count"],
                node_types: [["hidden", "object", "synthetic"], "", "", ""],
                edge_fields: ["type", "name_or_index", "to_node"],
                edge_types: [["element", "hidden", "internal"], "", ""]
            },
            node_count: 13,
            edge_count: 13
        },
        nodes: [
            // A tree with Window objects.
            //
            //    |----->Window--->A
            //    |             \
            //    |----->Window--->B--->C
            //    |        |     \
            //  (root)   hidden   --->D--internal / "native"-->N
            //    |          \        |
            //    |----->E    H    internal
            //    |                   v
            //    |----->F--->G       M
            //
            /* (root) */    2,  0,  1, 4,
            /* Window */    1, 11,  2, 2,
            /* Window */    1, 11,  3, 3,
            /* E */         2,  5,  4, 0,
            /* F */         2,  6,  5, 1,
            /* A */         1,  1,  6, 0,
            /* B */         1,  2,  7, 1,
            /* D */         1,  4,  8, 2,
            /* H */         1,  8,  9, 0,
            /* G */         1,  7, 10, 0,
            /* C */         1,  3, 11, 0,
            /* N */         1, 10, 12, 0,
            /* M */         1,  9, 13, 0
            ],
        edges: [
            /* from (root) */  0,  1,  4, 0, 2,  8, 0, 3, 12, 0, 4, 16,
            /* from Window */  0,  1, 20, 0, 2, 24,
            /* from Window */  0,  1, 24, 0, 2, 28, 1, 3, 32,
            /* from F */       0,  1, 36,
            /* from B */       0,  1, 40,
            /* from D */       2, 12, 44, 2, 1, 48
            ],
        strings: ["", "A", "B", "C", "D", "E", "F", "G", "H", "M", "N", "Window", "native"]
    });
};

InspectorTest.HeapNode = function(name, selfSize, type, id)
{
    this._type = type || InspectorTest.HeapNode.Type.object;
    this._name = name;
    this._selfSize = selfSize || 0;
    this._builder = null;
    this._edges = {};
    this._edgesCount = 0;
    this._id = id;
}

InspectorTest.HeapNode.Type = {
    "hidden": "hidden",
    "array": "array",
    "string": "string",
    "object": "object",
    "code": "code",
    "closure": "closure",
    "regexp": "regexp",
    "number": "number",
    "native": "native",
    "synthetic": "synthetic"
};

InspectorTest.HeapNode.prototype = {
    linkNode: function(node, type, nameOrIndex)
    {
        if (!this._builder)
            throw new Error("parent node is not connected to a snapshot");

        if (!node._builder)
            node._setBuilder(this._builder);

        if (nameOrIndex === undefined)
            nameOrIndex = this._edgesCount;
        ++this._edgesCount;

        if (nameOrIndex in this._edges)
            throw new Error("Can't add edge with the same nameOrIndex. nameOrIndex: " + nameOrIndex + " oldNodeName: " + this._edges[nameOrIndex]._name + " newNodeName: " + node._name);
        this._edges[nameOrIndex] = new InspectorTest.HeapEdge(node, type, nameOrIndex);
    },

    _setBuilder: function(builder)
    {
        if (this._builder)
            throw new Error("node reusing is prohibited");

        this._builder = builder;
        this._ordinal = this._builder._registerNode(this);
    },

    _serialize: function(rawSnapshot)
    {
        rawSnapshot.nodes.push(this._builder.lookupNodeType(this._type));
        rawSnapshot.nodes.push(this._builder.lookupOrAddString(this._name));
        // JS engine snapshot impementation generates monotonicaly increasing odd id for JS objects,
        // and even ids based on a hash for native DOMObject groups.
        rawSnapshot.nodes.push(this._id || this._ordinal * 2 + 1);
        rawSnapshot.nodes.push(this._selfSize);
        rawSnapshot.nodes.push(0);                               // retained_size
        rawSnapshot.nodes.push(0);                               // dominator
        rawSnapshot.nodes.push(Object.keys(this._edges).length); // edge_count

        for (var i in this._edges)
            this._edges[i]._serialize(rawSnapshot);
    }
}

InspectorTest.HeapEdge = function(targetNode, type, nameOrIndex)
{
    this._targetNode = targetNode;
    this._type = type;
    this._nameOrIndex = nameOrIndex;
}

InspectorTest.HeapEdge.prototype = {
    _serialize: function(rawSnapshot)
    {
        if (!this._targetNode._builder)
            throw new Error("Inconsistent state of node: " + this._name + " no builder assigned");
        var builder = this._targetNode._builder;
        rawSnapshot.edges.push(builder.lookupEdgeType(this._type));
        rawSnapshot.edges.push(typeof this._nameOrIndex === "string" ? builder.lookupOrAddString(this._nameOrIndex) : this._nameOrIndex);
        rawSnapshot.edges.push(this._targetNode._ordinal * builder.nodeFieldsCount); // index
    }
}

InspectorTest.HeapEdge.Type = {
    "context": "context",
    "element": "element",
    "property": "property",
    "internal": "internal",
    "hidden": "hidden",
    "shortcut": "shortcut",
    "weak": "weak"
};

InspectorTest.HeapSnapshotBuilder = function()
{
    this._nodes = [];
    this._string2id = {};
    this._strings = [];
    this.nodeFieldsCount = 7;

    this._nodeTypesMap = {};
    this._nodeTypesArray = [];
    for (var nodeType in InspectorTest.HeapNode.Type) {
        this._nodeTypesMap[nodeType] = this._nodeTypesArray.length
        this._nodeTypesArray.push(nodeType);
    }

    this._edgeTypesMap = {};
    this._edgeTypesArray = [];
    for (var edgeType in InspectorTest.HeapEdge.Type) {
        this._edgeTypesMap[edgeType] = this._edgeTypesArray.length
        this._edgeTypesArray.push(edgeType);
    }

    this.rootNode = new InspectorTest.HeapNode("root", 0, "object");
    this.rootNode._setBuilder(this);
}

InspectorTest.HeapSnapshotBuilder.prototype = {
    generateSnapshot: function()
    {
        var rawSnapshot = {
            "snapshot": {
                "meta": {
                    "node_fields": ["type","name","id","self_size","retained_size","dominator","edge_count"],
                    "node_types": [
                        this._nodeTypesArray,
                        "string",
                        "number",
                        "number",
                        "number",
                        "number",
                        "number"
                    ],
                    "edge_fields": ["type","name_or_index","to_node"],
                    "edge_types": [
                        this._edgeTypesArray,
                        "string_or_number",
                        "node"
                    ]
                }
            },
            "nodes": [],
            "edges":[],
            "strings": []
        };

        for (var i = 0; i < this._nodes.length; ++i)
            this._nodes[i]._serialize(rawSnapshot);

        rawSnapshot.strings = this._strings.slice();

        var meta = rawSnapshot.snapshot.meta;
        rawSnapshot.snapshot.edge_count = rawSnapshot.edges.length / meta.edge_fields.length;
        rawSnapshot.snapshot.node_count = rawSnapshot.nodes.length / meta.node_fields.length;

        return rawSnapshot;
    },

    createJSHeapSnapshot: function()
    {
        var parsedSnapshot = InspectorTest._postprocessHeapSnapshotMock(this.generateSnapshot());
        return new HeapSnapshotWorker.JSHeapSnapshot(parsedSnapshot, new HeapSnapshotWorker.HeapSnapshotProgress());
    },

    _registerNode: function(node)
    {
        this._nodes.push(node);
        return this._nodes.length - 1;
    },

    lookupNodeType: function(typeName)
    {
        if (typeName === undefined)
            throw new Error("wrong node type: " + typeName);
        if (!typeName in this._nodeTypesMap)
            throw new Error("wrong node type name: " + typeName);
        return this._nodeTypesMap[typeName];
    },

    lookupEdgeType: function(typeName)
    {
        if (!typeName in this._edgeTypesMap)
            throw new Error("wrong edge type name: " + typeName);
        return this._edgeTypesMap[typeName];
    },

    lookupOrAddString: function(string)
    {
        if (string in this._string2id)
            return this._string2id[string];
        this._string2id[string] = this._strings.length;
        this._strings.push(string);
        return this._strings.length - 1;
    }
}

InspectorTest.createHeapSnapshot = function(instanceCount, firstId)
{
    // Mocking results of running the following code:
    //
    // function A() { this.a = this; }
    // function B() { this.a = new A(); }
    // for (var i = 0; i < instanceCount; ++i) window[i] = new B();
    //
    // Set A and B object sizes to pseudo random numbers. It is used in sorting tests.

    var seed = 881669;
    function pseudoRandom(limit) {
        seed = ((seed * 355109 + 153763) >> 2) & 0xffff;
        return seed % limit;
    }

    var builder = new InspectorTest.HeapSnapshotBuilder();
    var rootNode = builder.rootNode;

    var gcRootsNode = new InspectorTest.HeapNode("(GC roots)", 0, InspectorTest.HeapNode.Type.synthetic);
    rootNode.linkNode(gcRootsNode, InspectorTest.HeapEdge.Type.element);

    var windowNode = new InspectorTest.HeapNode("Window", 20);
    rootNode.linkNode(windowNode, InspectorTest.HeapEdge.Type.shortcut);
    gcRootsNode.linkNode(windowNode, InspectorTest.HeapEdge.Type.element);

    for (var i = 0; i < instanceCount; ++i) {
        var sizeOfB = pseudoRandom(20) + 1;
        var nodeB = new InspectorTest.HeapNode("B", sizeOfB, undefined, firstId++);
        windowNode.linkNode(nodeB, InspectorTest.HeapEdge.Type.element);
        var sizeOfA = pseudoRandom(50) + 1;
        var nodeA = new InspectorTest.HeapNode("A", sizeOfA, undefined, firstId++);
        nodeB.linkNode(nodeA, InspectorTest.HeapEdge.Type.property, "a");
        nodeA.linkNode(nodeA, InspectorTest.HeapEdge.Type.property, "a");
    }
    return builder.generateSnapshot();
}

}

InspectorTest.createHeapSnapshotMockFactories();


InspectorTest.startProfilerTest = function(callback)
{
    InspectorTest.addResult("Profiler was enabled.");
    // We mock out HeapProfilerAgent -- as DRT runs in single-process mode, Inspector
    // and test share the same heap. Taking a snapshot takes too long for a test,
    // so we provide synthetic snapshots.
    InspectorTest._panelReset = InspectorTest.override(UI.panels.heap_profiler, "_reset", function(){}, true);
    InspectorTest.addSniffer(Profiler.HeapSnapshotView.prototype, "show", InspectorTest._snapshotViewShown, true);

    // Reduce the number of populated nodes to speed up testing.
    Profiler.HeapSnapshotContainmentDataGrid.prototype.defaultPopulateCount = function() { return 10; };
    Profiler.HeapSnapshotConstructorsDataGrid.prototype.defaultPopulateCount = function() { return 10; };
    Profiler.HeapSnapshotDiffDataGrid.prototype.defaultPopulateCount = function() { return 5; };
    InspectorTest.addResult("Detailed heap profiles were enabled.");
    InspectorTest.safeWrap(callback)();
};

InspectorTest.completeProfilerTest = function()
{
    // There is no way to disable detailed heap profiles.
    InspectorTest.addResult("");
    InspectorTest.addResult("Profiler was disabled.");
    InspectorTest.completeTest();
};

InspectorTest.runHeapSnapshotTestSuite = function(testSuite)
{
    var testSuiteTests = testSuite.slice();
    var completeTestStack;

    function runner()
    {
        if (!testSuiteTests.length) {
            if (completeTestStack)
                InspectorTest.addResult("FAIL: test already completed at " + completeTestStack);
            InspectorTest.completeProfilerTest();
            completeTestStack = new Error().stack;
            return;
        }

        var nextTest = testSuiteTests.shift();
        InspectorTest.addResult("");
        InspectorTest.addResult("Running: " + /function\s([^(]*)/.exec(nextTest)[1]);
        InspectorTest._panelReset.call(UI.panels.heap_profiler);
        InspectorTest.safeWrap(nextTest)(runner, runner);
    }

    InspectorTest.startProfilerTest(runner);
};

InspectorTest.assertColumnContentsEqual = function(reference, actual)
{
    var length = Math.min(reference.length, actual.length);
    for (var i = 0; i < length; ++i)
        InspectorTest.assertEquals(reference[i], actual[i], "row " + i);
    if (reference.length > length)
        InspectorTest.addResult("extra rows in reference array:\n" + reference.slice(length).join("\n"));
    else if (actual.length > length)
        InspectorTest.addResult("extra rows in actual array:\n" + actual.slice(length).join("\n"));
};

InspectorTest.checkArrayIsSorted = function(contents, sortType, sortOrder)
{
    function simpleComparator(a, b)
    {
        return a < b ? -1 : (a > b ? 1 : 0);
    }
    function parseSize(size)
    {
        // Remove thousands separator.
        return parseInt(size.replace(/[\xa0,]/g, ""), 10);
    }
    var extractor = {
        text: function (data) { data; },
        number: function (data) { return parseInt(data, 10); },
        size: parseSize,
        name: function (data) { return data; },
        id: function (data) { return parseInt(data, 10); }
    }[sortType];

    if (!extractor) {
        InspectorTest.addResult("Invalid sort type: " + sortType);
        return;
    }

    var acceptableComparisonResult;
    if (sortOrder === DataGrid.DataGrid.Order.Ascending) {
        acceptableComparisonResult = -1;
    } else if (sortOrder === DataGrid.DataGrid.Order.Descending) {
        acceptableComparisonResult = 1;
    } else {
        InspectorTest.addResult("Invalid sort order: " + sortOrder);
        return;
    }

    for (var i = 0; i < contents.length - 1; ++i) {
        var a = extractor(contents[i]);
        var b = extractor(contents[i + 1]);
        var result = simpleComparator(a, b);
        if (result !== 0 && result !== acceptableComparisonResult) {
            InspectorTest.addResult("Elements " + i + " and " + (i + 1) + " are out of order: " + a + " " + b + " (" + sortOrder + ")");
        }
    }
};

InspectorTest.clickColumn = function(column, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var cell = this._currentGrid()._headerTableHeaders[column.id];
    var event = { target: { enclosingNodeOrSelfWithNodeName: function() { return cell; } } };

    function sortingComplete()
    {
        InspectorTest._currentGrid().removeEventListener(Profiler.HeapSnapshotSortableDataGrid.Events.SortingComplete, sortingComplete, this);
        InspectorTest.assertEquals(column.id, this._currentGrid().sortColumnId(), "unexpected sorting");
        column.sort = this._currentGrid().sortOrder();
        function callCallback()
        {
            callback(column);
        }
        setTimeout(callCallback, 0);
    }
    InspectorTest._currentGrid().addEventListener(Profiler.HeapSnapshotSortableDataGrid.Events.SortingComplete, sortingComplete, this);
    this._currentGrid()._clickInHeaderCell(event);
};

InspectorTest.clickRowAndGetRetainers = function(row, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var event = {
        target: {
            enclosingNodeOrSelfWithNodeName: function() { return row._element; },
            selectedNode: row
        }
    };
    this._currentGrid()._mouseDownInDataTable(event);
    var rootNode = InspectorTest.currentProfileView()._retainmentDataGrid.rootNode();
    rootNode.once(Profiler.HeapSnapshotGridNode.Events.PopulateComplete).then(() => callback(rootNode));
};

InspectorTest.clickShowMoreButton = function(buttonName, row, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var parent = row.parent;
    parent.once(Profiler.HeapSnapshotGridNode.Events.PopulateComplete).then(() => setTimeout(() => callback(parent), 0));
    row[buttonName].click();
};

InspectorTest.columnContents = function(column, row)
{
    // Make sure invisible nodes are removed from the view port.
    this._currentGrid().updateVisibleNodes();
    var columnOrdinal = InspectorTest.viewColumns().indexOf(column);
    var result = [];
    var parent = row || this._currentGrid().rootNode();
    for (var node = parent.children[0]; node; node = node.traverseNextNode(true, parent, true)) {
        if (!node.selectable)
            continue;
        var content = node.element().children[columnOrdinal];
        // Do not inlcude percents
        if (content.firstElementChild)
            content = content.firstElementChild;
        result.push(content.textContent);
    }
    return result;
};

InspectorTest.countDataRows = function(row, filter)
{
    var result = 0;
    filter = filter || function(node) { return node.selectable; };
    for (var node = row.children[0]; node; node = node.traverseNextNode(true, row, true)) {
        if (filter(node))
            ++result;
    }
    return result;
};

InspectorTest.expandRow = function(row, callback)
{
    callback = InspectorTest.safeWrap(callback);
    row.once(Profiler.HeapSnapshotGridNode.Events.PopulateComplete).then(() => setTimeout(() => callback(row), 0));
    (function expand()
    {
        if (row.hasChildren())
            row.expand();
        else
            setTimeout(expand, 0);
    })();
};

InspectorTest.findAndExpandGCRoots = function(callback)
{
    InspectorTest.findAndExpandRow("(GC roots)", callback);
};

InspectorTest.findAndExpandWindow = function(callback)
{
    InspectorTest.findAndExpandRow("Window", callback);
};

InspectorTest.findAndExpandRow = function(name, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var row = InspectorTest.findRow(name);
    InspectorTest.assertEquals(true, !!row, '"' + name + '" row');
    InspectorTest.expandRow(row, callback);
};

InspectorTest.findButtonsNode = function(row, startNode)
{
    var result = 0;
    for (var node = startNode || row.children[0]; node; node = node.traverseNextNode(true, row, true)) {
        if (!node.selectable && node.showNext)
            return node;
    }
    return null;
};

InspectorTest.findRow = function(name, parent)
{
    function matcher(x)
    {
        return x._name === name;
    };
    return InspectorTest.findMatchingRow(matcher, parent);
};

InspectorTest.findMatchingRow = function(matcher, parent)
{
    parent = parent || this._currentGrid().rootNode();
    for (var node = parent.children[0]; node; node = node.traverseNextNode(true, parent, true)) {
        if (matcher(node))
            return node;
    }
    return null;
};

InspectorTest.switchToView = function(title, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var view = UI.panels.heap_profiler.visibleView;
    view._changePerspectiveAndWait(title).then(callback);
    // Increase the grid container height so the viewport don't limit the number of nodes.
    InspectorTest._currentGrid().scrollContainer.style.height = "10000px";
};

InspectorTest.takeAndOpenSnapshot = function(generator, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var snapshot = generator();
    var profileType = Profiler.ProfileTypeRegistry.instance.heapSnapshotProfileType;
    function pushGeneratedSnapshot(reportProgress)
    {
        var profile = profileType.profileBeingRecorded();
        if (reportProgress) {
            profileType._reportHeapSnapshotProgress({data: {done: 50, total: 100, finished: false}});
            profileType._reportHeapSnapshotProgress({data: {done: 100, total: 100, finished: true}});
        }
        snapshot.snapshot.typeId = "HEAP";
        profileType._addHeapSnapshotChunk({data: JSON.stringify(snapshot)});
        return Promise.resolve();
    }
    InspectorTest.override(InspectorTest.HeapProfilerAgent, "takeHeapSnapshot", pushGeneratedSnapshot);
    InspectorTest._takeAndOpenSnapshotCallback = callback;
    profileType._takeHeapSnapshot();
};

InspectorTest.viewColumns = function()
{
    return InspectorTest._currentGrid()._columnsArray;
};

InspectorTest.currentProfileView = function()
{
    return UI.panels.heap_profiler.visibleView;
};

InspectorTest._currentGrid = function()
{
    return this.currentProfileView()._dataGrid;
};

InspectorTest._snapshotViewShown = function()
{
    if (InspectorTest._takeAndOpenSnapshotCallback) {
        var callback = InspectorTest._takeAndOpenSnapshotCallback;
        InspectorTest._takeAndOpenSnapshotCallback = null;
        var dataGrid = this._dataGrid;
        function sortingComplete()
        {
            dataGrid.removeEventListener(Profiler.HeapSnapshotSortableDataGrid.Events.SortingComplete, sortingComplete, null);
            callback();
        }
        dataGrid.addEventListener(Profiler.HeapSnapshotSortableDataGrid.Events.SortingComplete, sortingComplete, null);
    }
};

};
