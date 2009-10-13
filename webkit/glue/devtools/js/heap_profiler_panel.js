// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Heap profiler panel implementation.
 */

WebInspector.ProfilesPanel.prototype.__oldReset = WebInspector.ProfilesPanel.prototype.reset;
WebInspector.ProfilesPanel.prototype.reset = function() {
    if (this._snapshots) {
        var snapshotsLength = this._snapshots.length;
        for (var i = 0; i < snapshotsLength; ++i) {
            var snapshot = this._snapshots[i];
            delete snapshot._snapshotView;
        }
    }
    this._snapshots = [];

    this.__oldReset();
}


WebInspector.ProfilesPanel.prototype.addSnapshot = function(snapshot) {
    this._snapshots.push(snapshot);
    snapshot.list = this._snapshots;
    snapshot.listIndex = this._snapshots.length - 1;

    var snapshotsTreeElement = new WebInspector.HeapSnapshotSidebarTreeElement(snapshot);
    snapshotsTreeElement.small = false;
    snapshot._snapshotsTreeElement = snapshotsTreeElement;

    this.snapshotsListTreeElement.appendChild(snapshotsTreeElement);

    this.dispatchEventToListeners("snapshot added");
}


WebInspector.ProfilesPanel.prototype.showSnapshot = function(snapshot) {
    if (!snapshot)
        return;

    if (this.visibleView)
        this.visibleView.hide();
    var view = this.snapshotViewForSnapshot(snapshot);
    view.show(this.profileViews);
    this.visibleView = view;

    this.profileViewStatusBarItemsContainer.removeChildren();
    var statusBarItems = view.statusBarItems;
    for (var i = 0; i < statusBarItems.length; ++i)
        this.profileViewStatusBarItemsContainer.appendChild(statusBarItems[i]);
}


WebInspector.ProfilesPanel.prototype.__oldShowView = WebInspector.ProfilesPanel.prototype.showView;
WebInspector.ProfilesPanel.prototype.showView = function(view) {
    if ('snapshot' in view)
        this.showSnapshot(view.snapshot);
    else
        this.__oldShowView(view);
}


WebInspector.ProfilesPanel.prototype.snapshotViewForSnapshot = function(snapshot) {
    if (!snapshot)
        return null;
    if (!snapshot._snapshotView)
        snapshot._snapshotView = new WebInspector.HeapSnapshotView(this, snapshot);
    return snapshot._snapshotView;
}


// TODO(mnaganov): remove this hack after Alexander's change landed in WebKit.
if ("getProfileType" in WebInspector.ProfilesPanel.prototype) {
    WebInspector.ProfilesPanel.prototype.__defineGetter__("profilesListTreeElement",
        function() { return this.getProfileType(WebInspector.ProfilesPanel.CPUProfileType.TYPE_ID).treeElement; });

    WebInspector.ProfilesPanel.prototype.__defineGetter__("snapshotsListTreeElement",
        function() { return this.getProfileType(WebInspector.ProfilesPanel.HeapSnapshotProfileType.TYPE_ID).treeElement; });                                                          
}


WebInspector.HeapSnapshotView = function(parent, snapshot)
{
    WebInspector.View.call(this);

    this.element.addStyleClass("heap-snapshot-view");

    this.parent = parent;
    this.parent.addEventListener("snapshot added", this._updateBaseOptions, this);

    this.showCountAsPercent = true;
    this.showSizeAsPercent = true;
    this.showCountDeltaAsPercent = true;
    this.showSizeDeltaAsPercent = true;

    this.summaryBar = new WebInspector.SummaryBar(this.categories);
    this.summaryBar.element.id = "heap-snapshot-summary";
    this.summaryBar.calculator = new WebInspector.HeapSummaryCalculator(snapshot.used);
    this.element.appendChild(this.summaryBar.element);

    var columns = { "cons": { title: WebInspector.UIString("Constructor"), disclosure: true, sortable: true },
                    "count": { title: WebInspector.UIString("Count"), width: "54px", sortable: true },
                    "size": { title: WebInspector.UIString("Size"), width: "72px", sort: "descending", sortable: true },
                    "countDelta": { title: WebInspector.UIString("\xb1 Count"), width: "72px", sortable: true },
                    "sizeDelta": { title: WebInspector.UIString("\xb1 Size"), width: "72px", sortable: true } };

    this.dataGrid = new WebInspector.DataGrid(columns);
    this.dataGrid.addEventListener("sorting changed", this._sortData, this);
    this.dataGrid.element.addEventListener("mousedown", this._mouseDownInDataGrid.bind(this), true);
    this.element.appendChild(this.dataGrid.element);

    this.snapshot = snapshot;

    this.baseSelectElement = document.createElement("select");
    this.baseSelectElement.className = "status-bar-item";
    this.baseSelectElement.addEventListener("change", this._changeBase.bind(this), false);
    this._updateBaseOptions();
    if (this.snapshot.listIndex > 0)
        this.baseSelectElement.selectedIndex = this.snapshot.listIndex - 1;
    else
        this.baseSelectElement.selectedIndex = this.snapshot.listIndex;
    this._resetDataGridList();

    this.percentButton = new WebInspector.StatusBarButton("", "percent-time-status-bar-item status-bar-item");
    this.percentButton.addEventListener("click", this._percentClicked.bind(this), false);

    this.refresh();

    this._updatePercentButton();
};

WebInspector.HeapSnapshotView.prototype = {

    get categories()
    {
        return {code: {title: WebInspector.UIString("Code"), color: {r: 255, g: 121, b: 0}}, data: {title: WebInspector.UIString("Objects and Data"), color: {r: 47, g: 102, b: 236}}, other: {title: WebInspector.UIString("Other"), color: {r: 186, g: 186, b: 186}}};
    },

    get statusBarItems()
    {
        return [this.baseSelectElement, this.percentButton.element];
    },

    get snapshot()
    {
        return this._snapshot;
    },

    set snapshot(snapshot)
    {
        this._snapshot = snapshot;
    },

    show: function(parentElement)
    {
        WebInspector.View.prototype.show.call(this, parentElement);
        this.dataGrid.updateWidths();
    },

    resize: function()
    {
        if (this.dataGrid)
            this.dataGrid.updateWidths();
    },

    refresh: function()
    {
        this.dataGrid.removeChildren();

        var children = this.snapshotDataGridList.children;
        var count = children.length;
        for (var index = 0; index < count; ++index)
            this.dataGrid.appendChild(children[index]);

        this._updateSummaryGraph();
    },

    refreshShowAsPercents: function()
    {
        this._updatePercentButton();
        this.refreshVisibleData();
    },

    refreshVisibleData: function()
    {
        var child = this.dataGrid.children[0];
        while (child) {
            child.refresh();
            child = child.traverseNextNode(false, null, true);
        }
        this._updateSummaryGraph();
    },

    _changeBase: function() {
        if (this.baseSnapshot === this.snapshot.list[this.baseSelectElement.selectedIndex])
            return;

        this._resetDataGridList();
        this.refresh();
    },

    _createSnapshotDataGridList: function()
    {
        if (this._snapshotDataGridList)
          delete this._snapshotDataGridList;

        this._snapshotDataGridList = new WebInspector.HeapSnapshotDataGridList(this, this.baseSnapshot.entries, this.snapshot.entries);
        return this._snapshotDataGridList;
    },

    _mouseDownInDataGrid: function(event)
    {
        if (event.detail < 2)
            return;

        var cell = event.target.enclosingNodeOrSelfWithNodeName("td");
        if (!cell || (!cell.hasStyleClass("count-column") && !cell.hasStyleClass("size-column") && !cell.hasStyleClass("countDelta-column") && !cell.hasStyleClass("sizeDelta-column")))
            return;

        if (cell.hasStyleClass("count-column"))
            this.showCountAsPercent = !this.showCountAsPercent;
        else if (cell.hasStyleClass("size-column"))
            this.showSizeAsPercent = !this.showSizeAsPercent;
        else if (cell.hasStyleClass("countDelta-column"))
            this.showCountDeltaAsPercent = !this.showCountDeltaAsPercent;
        else if (cell.hasStyleClass("sizeDelta-column"))
            this.showSizeDeltaAsPercent = !this.showSizeDeltaAsPercent;

        this.refreshShowAsPercents();

        event.preventDefault();
        event.stopPropagation();
    },

    get _isShowingAsPercent()
    {
        return this.showCountAsPercent && this.showSizeAsPercent && this.showCountDeltaAsPercent && this.showSizeDeltaAsPercent;
    },

    _percentClicked: function(event)
    {
        var currentState = this._isShowingAsPercent;
        this.showCountAsPercent = !currentState;
        this.showSizeAsPercent = !currentState;
        this.showCountDeltaAsPercent = !currentState;
        this.showSizeDeltaAsPercent = !currentState;
        this.refreshShowAsPercents();
    },

    _resetDataGridList: function()
    {
        this.baseSnapshot = this.snapshot.list[this.baseSelectElement.selectedIndex];
        var lastComparator = WebInspector.HeapSnapshotDataGridList.propertyComparator("size", false);
        if (this.snapshotDataGridList) {
            lastComparator = this.snapshotDataGridList.lastComparator;
        }
        this.snapshotDataGridList = this._createSnapshotDataGridList();
        this.snapshotDataGridList.sort(lastComparator, true);
    },

    _sortData: function()
    {
        var sortAscending = this.dataGrid.sortOrder === "ascending";
        var sortColumnIdentifier = this.dataGrid.sortColumnIdentifier;
        var sortProperty = {
                "cons": "constructorName",
                "count": "count",
                "size": "size",
                "countDelta": this.showCountDeltaAsPercent ? "countDeltaPercent" : "countDelta",
                "sizeDelta": this.showSizeDeltaAsPercent ? "sizeDeltaPercent" : "sizeDelta"
        }[sortColumnIdentifier];

        this.snapshotDataGridList.sort(WebInspector.HeapSnapshotDataGridList.propertyComparator(sortProperty, sortAscending));

        this.refresh();
    },

    _updateBaseOptions: function()
    {
        // We're assuming that snapshots can only be added.
        if (this.baseSelectElement.length == this.snapshot.list.length)
            return;

        for (var i = this.baseSelectElement.length, n = this.snapshot.list.length; i < n; ++i) {
            var baseOption = document.createElement("option");
            baseOption.label = WebInspector.UIString("Compared to %s", this.snapshot.list[i].title);
            this.baseSelectElement.appendChild(baseOption);
        }
    },

    _updatePercentButton: function()
    {
        if (this._isShowingAsPercent) {
            this.percentButton.title = WebInspector.UIString("Show absolute counts and sizes.");
            this.percentButton.toggled = true;
        } else {
            this.percentButton.title = WebInspector.UIString("Show counts and sizes as percentages.");
            this.percentButton.toggled = false;
        }
    },

    _updateSummaryGraph: function()
    {
        this.summaryBar.calculator.showAsPercent = this._isShowingAsPercent;
        this.summaryBar.update(this.snapshot.lowlevels);
    }
};

WebInspector.HeapSnapshotView.prototype.__proto__ = WebInspector.View.prototype;

WebInspector.HeapSummaryCalculator = function(total)
{
    this.total = total;
}

WebInspector.HeapSummaryCalculator.prototype = {
    computeSummaryValues: function(lowLevels)
    {
        function highFromLow(type) {
            if (type == "CODE_TYPE" || type == "SHARED_FUNCTION_INFO_TYPE" || type == "SCRIPT_TYPE") return "code";
            if (type == "STRING_TYPE" || type == "HEAP_NUMBER_TYPE" || type.match(/^JS_/) || type.match(/_ARRAY_TYPE$/)) return "data";
            return "other";
        }
        var highLevels = {data: 0, code: 0, other: 0};
        for (var item in lowLevels) {
            var highItem = highFromLow(item);
            highLevels[highItem] += lowLevels[item].size;
        }
        return {categoryValues: highLevels};
    },

    formatValue: function(value)
    {
        if (this.showAsPercent)
            return WebInspector.UIString("%.2f%%", value / this.total * 100.0);
        else
            return Number.bytesToString(value);
    },

    get showAsPercent()
    {
        return this._showAsPercent;
    },

    set showAsPercent(x)
    {
        this._showAsPercent = x;
    }
}

WebInspector.HeapSnapshotSidebarTreeElement = function(snapshot)
{
    this.snapshot = snapshot;
    this.snapshot.title = WebInspector.UIString("Snapshot %d", this.snapshot.number);

    WebInspector.SidebarTreeElement.call(this, "heap-snapshot-sidebar-tree-item", "", "", snapshot, false);

    this.refreshTitles();
};

WebInspector.HeapSnapshotSidebarTreeElement.prototype = {
    onselect: function()
    {
        WebInspector.panels.profiles.showSnapshot(this.snapshot);
    },

    get mainTitle()
    {
        if (this._mainTitle)
            return this._mainTitle;
        return this.snapshot.title;
    },

    set mainTitle(x)
    {
        this._mainTitle = x;
        this.refreshTitles();
    },

    get subtitle()
    {
        if (this._subTitle)
            return this._subTitle;
        return WebInspector.UIString("Used %s of %s (%.0f%%)", Number.bytesToString(this.snapshot.used, null, false), Number.bytesToString(this.snapshot.capacity, null, false), this.snapshot.used / this.snapshot.capacity * 100.0);
    },

    set subtitle(x)
    {
        this._subTitle = x;
        this.refreshTitles();
    }
};

WebInspector.HeapSnapshotSidebarTreeElement.prototype.__proto__ = WebInspector.SidebarTreeElement.prototype;

WebInspector.HeapSnapshotDataGridNodeWithRetainers = function(owningTree)
{
    this.tree = owningTree;

    WebInspector.DataGridNode.call(this, null, this._hasRetainers);

    this.addEventListener("populate", this._populate, this);
};

WebInspector.HeapSnapshotDataGridNodeWithRetainers.prototype = {
    isEmptySet: function(set)
    {
        for (var x in set)
            return false;
        return true;
    },

    get _hasRetainers()
    {
        return !this.isEmptySet(this.retainers);
    },

    get _parent()
    {
        // For top-level nodes, return owning tree as a parent, not data grid.
        return this.parent !== this.dataGrid ? this.parent : this.tree;
    },

    _populate: function(event)
    {
        for (var retainer in this.retainers) {
            this.appendChild(new WebInspector.HeapSnapshotDataGridRetainerNode(this.snapshotView, null, this.retainers[retainer], this.tree));
        }

        if (this._parent) {
            var currentComparator = this._parent.lastComparator;
            if (currentComparator)
                this.sort(currentComparator, true);
        }

        this.removeEventListener("populate", this._populate, this);
    },

    sort: function(comparator, force) {
        if (!force && this.lastComparator === comparator)
            return;

        this.children.sort(comparator);
        var childCount = this.children.length;           
        for (var childIndex = 0; childIndex < childCount; ++childIndex)
            this.children[childIndex]._recalculateSiblings(childIndex);            
        for (var i = 0; i < this.children.length; ++i) {
            var child = this.children[i];
            if (!force && (!child.expanded || child.lastComparator === comparator))
                continue;
            child.sort(comparator, force);
        }
        this.lastComparator = comparator;
    },

    getTotalCount: function() {
        if (!this._count) {
            this._count = 0;
            for (var i = 0, n = this.children.length; i < n; ++i) {
                this._count += this.children[i].count;
            }
        }
        return this._count;
    },

    getTotalSize: function() {
        if (!this._size) {
            this._size = 0;
            for (var i = 0, n = this.children.length; i < n; ++i) {
                this._size += this.children[i].size;
            }
        }
        return this._size;
    },

    get countPercent()
    {
        return this.count / this._parent.getTotalCount() * 100.0;
    },

    get sizePercent()
    {
        return this.size / this._parent.getTotalSize() * 100.0;
    }
};

WebInspector.HeapSnapshotDataGridNodeWithRetainers.prototype.__proto__ = WebInspector.DataGridNode.prototype;

WebInspector.HeapSnapshotDataGridNode = function(snapshotView, baseEntry, snapshotEntry, owningTree)
{
    this.snapshotView = snapshotView;

    if (!snapshotEntry)
        snapshotEntry = { cons: baseEntry.cons, count: 0, size: 0, retainers: {} };
    this.constructorName = snapshotEntry.cons;
    this.count = snapshotEntry.count;
    this.size = snapshotEntry.size;
    this.retainers = snapshotEntry.retainers;

    if (!baseEntry)
        baseEntry = { count: 0, size: 0, retainers: {} };
    this.baseCount = baseEntry.count;
    this.countDelta = this.count - this.baseCount;
    this.baseSize = baseEntry.size;
    this.sizeDelta = this.size - this.baseSize;
    this.baseRetainers = baseEntry.retainers;

    WebInspector.HeapSnapshotDataGridNodeWithRetainers.call(this, owningTree);
};

WebInspector.HeapSnapshotDataGridNode.prototype = {
    get data()
    {
        var data = {};

        data["cons"] = this.constructorName;

        if (this.snapshotView.showCountAsPercent)
            data["count"] = WebInspector.UIString("%.2f%%", this.countPercent);
        else
            data["count"] = this.count;

        if (this.snapshotView.showSizeAsPercent)
            data["size"] = WebInspector.UIString("%.2f%%", this.sizePercent);
        else
            data["size"] = Number.bytesToString(this.size);

        function signForDelta(delta) {
            if (delta == 0)
                return "";
            if (delta > 0)
                return "+";
            else
                // Math minus sign, same width as plus.
                return "\u2212";
        }

        function showDeltaAsPercent(value) {
            if (value === Number.POSITIVE_INFINITY)
                return WebInspector.UIString("new");
            else if (value === Number.NEGATIVE_INFINITY)
                return WebInspector.UIString("deleted");
            if (value > 1000.0)
                return WebInspector.UIString("%s >1000%%", signForDelta(value));
            return WebInspector.UIString("%s%.2f%%", signForDelta(value), Math.abs(value));
        }

        if (this.snapshotView.showCountDeltaAsPercent)
            data["countDelta"] = showDeltaAsPercent(this.countDeltaPercent);
        else
            data["countDelta"] = WebInspector.UIString("%s%d", signForDelta(this.countDelta), Math.abs(this.countDelta));

        if (this.snapshotView.showSizeDeltaAsPercent)
            data["sizeDelta"] = showDeltaAsPercent(this.sizeDeltaPercent);
        else
            data["sizeDelta"] = WebInspector.UIString("%s%s", signForDelta(this.sizeDelta), Number.bytesToString(Math.abs(this.sizeDelta)));

        return data;
    },

    get countDeltaPercent()
    {
        if (this.baseCount > 0) {
            if (this.count > 0)
                return this.countDelta / this.baseCount * 100.0;
            else
                return Number.NEGATIVE_INFINITY;
        } else
            return Number.POSITIVE_INFINITY;
    },

    get sizeDeltaPercent()
    {
        if (this.baseSize > 0) {
            if (this.size > 0)
                return this.sizeDelta / this.baseSize * 100.0;
            else
                return Number.NEGATIVE_INFINITY;
        } else
            return Number.POSITIVE_INFINITY;
    }
};

WebInspector.HeapSnapshotDataGridNode.prototype.__proto__ = WebInspector.HeapSnapshotDataGridNodeWithRetainers.prototype;

WebInspector.HeapSnapshotDataGridList = function(snapshotView, baseEntries, snapshotEntries)
{
    this.tree = this;
    this.snapshotView = snapshotView;
    this.children = [];
    this.lastComparator = null;
    this.populateChildren(baseEntries, snapshotEntries);
};

WebInspector.HeapSnapshotDataGridList.prototype = {
    appendChild: function(child)
    {
        this.insertChild(child, this.children.length);
    },

    insertChild: function(child, index)
    {
        this.children.splice(index, 0, child);
    },

    removeChildren: function()
    {
        this.children = [];
    },

    produceDiff: function(baseEntries, currentEntries, callback)
    {
        for (var item in currentEntries)
            callback(baseEntries[item], currentEntries[item]);

        for (item in baseEntries) {
            if (!(item in currentEntries))
                callback(baseEntries[item], null);
        }
    },

    populateChildren: function(baseEntries, snapshotEntries)
    {
        var self = this;
        this.produceDiff(baseEntries, snapshotEntries, function(baseItem, snapshotItem) {
            self.appendChild(new WebInspector.HeapSnapshotDataGridNode(self.snapshotView, baseItem, snapshotItem, self));
        });
    },

    sort: WebInspector.HeapSnapshotDataGridNodeWithRetainers.prototype.sort,
    getTotalCount: WebInspector.HeapSnapshotDataGridNodeWithRetainers.prototype.getTotalCount,
    getTotalSize: WebInspector.HeapSnapshotDataGridNodeWithRetainers.prototype.getTotalSize
};

WebInspector.HeapSnapshotDataGridList.propertyComparators = [{}, {}];

WebInspector.HeapSnapshotDataGridList.propertyComparator = function(property, isAscending)
{
    var comparator = this.propertyComparators[(isAscending ? 1 : 0)][property];
    if (!comparator) {
        comparator = function(lhs, rhs) {
            var l = lhs[property], r = rhs[property];
            var result = l < r ? -1 : (l > r ? 1 : 0);
            return isAscending ? result : -result;
        };
        this.propertyComparators[(isAscending ? 1 : 0)][property] = comparator;
    }
    return comparator;
};

WebInspector.HeapSnapshotDataGridRetainerNode = function(snapshotView, baseEntry, snapshotEntry, owningTree)
{
    this.snapshotView = snapshotView;

    if (!snapshotEntry)
        snapshotEntry = { cons: baseEntry.cons, count: 0, clusters: {} };
    this.constructorName = snapshotEntry.cons;
    this.count = snapshotEntry.count;
    this.retainers = {};
    if (this.isEmptySet(snapshotEntry.clusters)) {
        if (this.constructorName in this.snapshotView.snapshot.entries)
            this.retainers = this.snapshotView.snapshot.entries[this.constructorName].retainers;
    } else {
        // In case when an entry is retained by clusters, we need to gather up the list
        // of retainers by merging retainers of every cluster.
        // E.g. having such a tree:
        //   A
        //     Object:1  10
        //       X       3
        //       Y       4
        //     Object:2  5
        //       X       6
        //
        // will result in a following retainers list: X 9, Y 4.
        for (var clusterName in snapshotEntry.clusters) {
            if (clusterName in this.snapshotView.snapshot.clusters) {
                var clusterRetainers = this.snapshotView.snapshot.clusters[clusterName].retainers;
                for (var clusterRetainer in clusterRetainers) {
                    var clusterRetainerEntry = clusterRetainers[clusterRetainer];
                    if (!(clusterRetainer in this.retainers))
                        this.retainers[clusterRetainer] = { cons: clusterRetainerEntry.cons, count: 0, clusters: {} };
                    this.retainers[clusterRetainer].count += clusterRetainerEntry.count;
                    for (var clusterRetainerCluster in clusterRetainerEntry.clusters)
                        this.retainers[clusterRetainer].clusters[clusterRetainerCluster] = true;
                }
            }
        }
    }  

    if (!baseEntry)
        baseEntry = { count: 0, clusters: {} };
    this.baseCount = baseEntry.count;
    this.countDelta = this.count - this.baseCount;

    this.size = this.count;  // This way, when sorting by sizes entries will be sorted by references count.

    WebInspector.HeapSnapshotDataGridNodeWithRetainers.call(this, owningTree);
}

WebInspector.HeapSnapshotDataGridRetainerNode.prototype = {
    get data()
    {
        var data = {};

        data["cons"] = this.constructorName;
        if (this.snapshotView.showCountAsPercent)
            data["count"] = WebInspector.UIString("%.2f%%", this.countPercent);
        else
            data["count"] = this.count;
        data["size"] = "";
        data["countDelta"] = "";
        data["sizeDelta"] = "";

        return data;
    }
};

WebInspector.HeapSnapshotDataGridRetainerNode.prototype.__proto__ = WebInspector.HeapSnapshotDataGridNodeWithRetainers.prototype;

