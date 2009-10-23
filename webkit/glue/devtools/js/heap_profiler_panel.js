// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Heap profiler panel implementation.
 */

WebInspector.ProfilesPanel.prototype.addSnapshot = function(snapshot) {
    snapshot.title = WebInspector.UIString("Snapshot %d", snapshot.number);

    var snapshots = WebInspector.HeapSnapshotProfileType.snapshots;
    snapshots.push(snapshot);

    snapshot.listIndex = snapshots.length - 1;

    this.addProfileHeader(WebInspector.HeapSnapshotProfileType.TypeId, snapshot);

    this.dispatchEventToListeners("snapshot added");
}


WebInspector.HeapSnapshotView = function(parent, profile)
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
    this.summaryBar.calculator = new WebInspector.HeapSummaryCalculator(profile.used);
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

    this.profile = profile;

    this.baseSelectElement = document.createElement("select");
    this.baseSelectElement.className = "status-bar-item";
    this.baseSelectElement.addEventListener("change", this._changeBase.bind(this), false);
    this._updateBaseOptions();
    if (this.profile.listIndex > 0)
        this.baseSelectElement.selectedIndex = this.profile.listIndex - 1;
    else
        this.baseSelectElement.selectedIndex = this.profile.listIndex;
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

    get profile()
    {
        return this._profile;
    },

    set profile(profile)
    {
        this._profile = profile;
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
        if (this.baseSnapshot === WebInspector.HeapSnapshotProfileType.snapshots[this.baseSelectElement.selectedIndex])
            return;

        this._resetDataGridList();
        this.refresh();
    },

    _createSnapshotDataGridList: function()
    {
        if (this._snapshotDataGridList)
          delete this._snapshotDataGridList;

        this._snapshotDataGridList = new WebInspector.HeapSnapshotDataGridList(this, this.baseSnapshot.entries, this.profile.entries);
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
        this.baseSnapshot = WebInspector.HeapSnapshotProfileType.snapshots[this.baseSelectElement.selectedIndex];
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
        var list = WebInspector.HeapSnapshotProfileType.snapshots;
        // We're assuming that snapshots can only be added.
        if (this.baseSelectElement.length == list.length)
            return;

        for (var i = this.baseSelectElement.length, n = list.length; i < n; ++i) {
            var baseOption = document.createElement("option");
            baseOption.label = WebInspector.UIString("Compared to %s", list[i].title);
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
        this.summaryBar.update(this.profile.lowlevels);
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
    this.profile = snapshot;

    WebInspector.SidebarTreeElement.call(this, "heap-snapshot-sidebar-tree-item", "", "", snapshot, false);

    this.refreshTitles();
};

WebInspector.HeapSnapshotSidebarTreeElement.prototype = {
    get mainTitle()
    {
        if (this._mainTitle)
            return this._mainTitle;
        return this.profile.title;
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
        return WebInspector.UIString("Used %s of %s (%.0f%%)", Number.bytesToString(this.profile.used, null, false), Number.bytesToString(this.profile.capacity, null, false), this.profile.used / this.profile.capacity * 100.0);
    },

    set subtitle(x)
    {
        this._subTitle = x;
        this.refreshTitles();
    }
};

WebInspector.HeapSnapshotSidebarTreeElement.prototype.__proto__ = WebInspector.ProfileSidebarTreeElement.prototype;

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
        var self = this;
        this.produceDiff(this.baseRetainers, this.retainers, function(baseItem, snapshotItem) {
            self.appendChild(new WebInspector.HeapSnapshotDataGridRetainerNode(self.snapshotView, baseItem, snapshotItem, self.tree));
        });

        if (this._parent) {
            var currentComparator = this._parent.lastComparator;
            if (currentComparator)
                this.sort(currentComparator, true);
        }

        this.removeEventListener("populate", this._populate, this);
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

    signForDelta: function(delta) {
        if (delta == 0)
            return "";
        if (delta > 0)
            return "+";
        else
            // Math minus sign, same width as plus.
            return "\u2212";
    },

    showDeltaAsPercent: function(value) {
        if (value === Number.POSITIVE_INFINITY)
            return WebInspector.UIString("new");
        else if (value === Number.NEGATIVE_INFINITY)
            return WebInspector.UIString("deleted");
        if (value > 1000.0)
            return WebInspector.UIString("%s >1000%%", this.signForDelta(value));
        return WebInspector.UIString("%s%.2f%%", this.signForDelta(value), Math.abs(value));
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
    },

    getData: function(showSize)
    {
        var data = {};

        data["cons"] = this.constructorName;

        if (this.snapshotView.showCountAsPercent)
            data["count"] = WebInspector.UIString("%.2f%%", this.countPercent);
        else
            data["count"] = this.count;

        if (showSize) {
            if (this.snapshotView.showSizeAsPercent)
                data["size"] = WebInspector.UIString("%.2f%%", this.sizePercent);
            else
                data["size"] = Number.bytesToString(this.size);
        } else {
            data["size"] = "";
        }

        if (this.snapshotView.showCountDeltaAsPercent)
            data["countDelta"] = this.showDeltaAsPercent(this.countDeltaPercent);
        else
            data["countDelta"] = WebInspector.UIString("%s%d", this.signForDelta(this.countDelta), Math.abs(this.countDelta));

        if (showSize) {
            if (this.snapshotView.showSizeDeltaAsPercent)
                data["sizeDelta"] = this.showDeltaAsPercent(this.sizeDeltaPercent);
            else
                data["sizeDelta"] = WebInspector.UIString("%s%s", this.signForDelta(this.sizeDelta), Number.bytesToString(Math.abs(this.sizeDelta)));
        } else {
            data["sizeDelta"] = "";
        }

        return data;
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
        return this.getData(true);
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

    populateChildren: function(baseEntries, snapshotEntries)
    {
        var self = this;
        this.produceDiff(baseEntries, snapshotEntries, function(baseItem, snapshotItem) {
            self.appendChild(new WebInspector.HeapSnapshotDataGridNode(self.snapshotView, baseItem, snapshotItem, self));
        });
    },

    produceDiff: WebInspector.HeapSnapshotDataGridNodeWithRetainers.prototype.produceDiff,
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
    this.retainers = this._calculateRetainers(this.snapshotView.profile, snapshotEntry.clusters);

    if (!baseEntry)
        baseEntry = { count: 0, clusters: {} };
    this.baseCount = baseEntry.count;
    this.countDelta = this.count - this.baseCount;
    this.baseRetainers = this._calculateRetainers(this.snapshotView.baseSnapshot, baseEntry.clusters);

    this.size = this.count;  // This way, when sorting by sizes entries will be sorted by references count.

    WebInspector.HeapSnapshotDataGridNodeWithRetainers.call(this, owningTree);
}

WebInspector.HeapSnapshotDataGridRetainerNode.prototype = {
    get data()
    {
        return this.getData(false);
    },

    _calculateRetainers: function(snapshot, clusters) {
        var retainers = {};
        if (this.isEmptySet(clusters)) {
            if (this.constructorName in snapshot.entries)
                return snapshot.entries[this.constructorName].retainers;
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
            for (var clusterName in clusters) {
                if (clusterName in snapshot.clusters) {
                    var clusterRetainers = snapshot.clusters[clusterName].retainers;
                    for (var clusterRetainer in clusterRetainers) {
                        var clusterRetainerEntry = clusterRetainers[clusterRetainer];
                        if (!(clusterRetainer in retainers))
                            retainers[clusterRetainer] = { cons: clusterRetainerEntry.cons, count: 0, clusters: {} };
                        retainers[clusterRetainer].count += clusterRetainerEntry.count;
                        for (var clusterRetainerCluster in clusterRetainerEntry.clusters)
                            retainers[clusterRetainer].clusters[clusterRetainerCluster] = true;
                    }
                }
            }
        }
        return retainers;
    }
};

WebInspector.HeapSnapshotDataGridRetainerNode.prototype.__proto__ = WebInspector.HeapSnapshotDataGridNodeWithRetainers.prototype;


WebInspector.HeapSnapshotProfileType = function()
{
    WebInspector.ProfileType.call(this, WebInspector.HeapSnapshotProfileType.TypeId, WebInspector.UIString("HEAP SNAPSHOTS"));
}

WebInspector.HeapSnapshotProfileType.TypeId = "HEAP";

WebInspector.HeapSnapshotProfileType.snapshots = [];

WebInspector.HeapSnapshotProfileType.prototype = {
    get buttonTooltip()
    {
        return WebInspector.UIString("Take heap snapshot.");
    },

    get buttonStyle()
    {
        return "heap-snapshot-status-bar-item status-bar-item";
    },

    buttonClicked: function()
    {
        InspectorController.takeHeapSnapshot();
    },

    createSidebarTreeElementForProfile: function(profile)
    {
        var element = new WebInspector.HeapSnapshotSidebarTreeElement(profile);
        element.small = false;
        return element;
    },

    createView: function(profile)
    {
        return new WebInspector.HeapSnapshotView(WebInspector.panels.profiles, profile);
    }
}

WebInspector.HeapSnapshotProfileType.prototype.__proto__ = WebInspector.ProfileType.prototype;


(function() {
    var originalCreatePanels = WebInspector._createPanels;
    WebInspector._createPanels = function() {
        originalCreatePanels.apply(this, arguments);
        if (WebInspector.panels.profiles)
            WebInspector.panels.profiles.registerProfileType(new WebInspector.HeapSnapshotProfileType());
    }
})();
