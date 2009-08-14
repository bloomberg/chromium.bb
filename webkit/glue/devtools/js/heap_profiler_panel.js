// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Heap profiler panel implementation.
 */

WebInspector.HeapProfilerPanel = function() {
    WebInspector.Panel.call(this);

    this.element.addStyleClass("heap-profiler");

    this.sidebarElement = document.createElement("div");
    this.sidebarElement.id = "heap-snapshot-sidebar";
    this.sidebarElement.className = "sidebar";
    this.element.appendChild(this.sidebarElement);

    this.sidebarResizeElement = document.createElement("div");
    this.sidebarResizeElement.className = "sidebar-resizer-vertical";
    this.sidebarResizeElement.addEventListener("mousedown", this._startSidebarDragging.bind(this), false);
    this.element.appendChild(this.sidebarResizeElement);

    this.sidebarTreeElement = document.createElement("ol");
    this.sidebarTreeElement.className = "sidebar-tree";
    this.sidebarElement.appendChild(this.sidebarTreeElement);

    this.sidebarTree = new TreeOutline(this.sidebarTreeElement);

    this.snapshotViews = document.createElement("div");
    this.snapshotViews.id = "heap-snapshot-views";
    this.element.appendChild(this.snapshotViews);

    this.snapshotButton = document.createElement("button");
    this.snapshotButton.title = WebInspector.UIString("Take heap snapshot.");
    this.snapshotButton.className = "heap-snapshot-status-bar-item status-bar-item";
    this.snapshotButton.addEventListener("click", this._snapshotClicked.bind(this), false);

    this.snapshotViewStatusBarItemsContainer = document.createElement("div");
    this.snapshotViewStatusBarItemsContainer.id = "heap-snapshot-status-bar-items";

    this.reset();
};

WebInspector.HeapProfilerPanel.prototype = {
    toolbarItemClass: "heap-profiler",

    get toolbarItemLabel() {
        return WebInspector.UIString("Heap");
    },

    get statusBarItems() {
        return [this.snapshotButton, this.snapshotViewStatusBarItemsContainer];
    },

    show: function() {
        WebInspector.Panel.prototype.show.call(this);
        this._updateSidebarWidth();
    },

    reset: function() {
        if (this._snapshots) {
            var snapshotsLength = this._snapshots.length;
            for (var i = 0; i < snapshotsLength; ++i) {
                var snapshot = this._snapshots[i];
                delete snapshot._snapshotView;
            }
        }

        this._snapshots = [];

        this.sidebarTreeElement.removeStyleClass("some-expandable");

        this.sidebarTree.removeChildren();
        this.snapshotViews.removeChildren();

        this.snapshotViewStatusBarItemsContainer.removeChildren();
    },

    handleKeyEvent: function(event) {
        this.sidebarTree.handleKeyEvent(event);
    },

    addSnapshot: function(snapshot) {
        this._snapshots.push(snapshot);
        snapshot.list = this._snapshots;
        snapshot.listIndex = this._snapshots.length - 1;

        var snapshotsTreeElement = new WebInspector.HeapSnapshotSidebarTreeElement(snapshot);
        snapshotsTreeElement.small = false;
        snapshot._snapshotsTreeElement = snapshotsTreeElement;

        this.sidebarTree.appendChild(snapshotsTreeElement);

        this.dispatchEventToListeners("snapshot added");
    },

    showSnapshot: function(snapshot) {
        if (!snapshot)
            return;

        if (this.visibleView)
            this.visibleView.hide();
        var view = this.snapshotViewForSnapshot(snapshot);
        view.show(this.snapshotViews);
        this.visibleView = view;

        this.snapshotViewStatusBarItemsContainer.removeChildren();
        var statusBarItems = view.statusBarItems;
        for (var i = 0; i < statusBarItems.length; ++i)
            this.snapshotViewStatusBarItemsContainer.appendChild(statusBarItems[i]);
    },

    showView: function(view)
    {
        this.showSnapshot(view.snapshot);
    },

    snapshotViewForSnapshot: function(snapshot)
    {
        if (!snapshot)
            return null;
        if (!snapshot._snapshotView)
            snapshot._snapshotView = new WebInspector.HeapSnapshotView(this, snapshot);
        return snapshot._snapshotView;
    },

    closeVisibleView: function()
    {
        if (this.visibleView)
            this.visibleView.hide();
        delete this.visibleView;
    },

    _snapshotClicked: function() {
        devtools.tools.getDebuggerAgent().startProfiling(
            devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_HEAP_SNAPSHOT |
                devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_HEAP_STATS |
                devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_JS_CONSTRUCTORS);
    },

    _startSidebarDragging: function(event)
    {
        WebInspector.elementDragStart(this.sidebarResizeElement, this._sidebarDragging.bind(this), this._endSidebarDragging.bind(this), event, "col-resize");
    },

    _sidebarDragging: function(event)
    {
        this._updateSidebarWidth(event.pageX);

        event.preventDefault();
    },

    _endSidebarDragging: function(event)
    {
        WebInspector.elementDragEnd(event);
    },

    _updateSidebarWidth: function(width)
    {
        if (this.sidebarElement.offsetWidth <= 0) {
            // The stylesheet hasn"t loaded yet or the window is closed,
            // so we can"t calculate what is need. Return early.
            return;
        }

        if (!("_currentSidebarWidth" in this))
            this._currentSidebarWidth = this.sidebarElement.offsetWidth;
        if (typeof width === "undefined")
            width = this._currentSidebarWidth;

        width = Number.constrain(width, Preferences.minSidebarWidth, window.innerWidth / 2);
        this._currentSidebarWidth = width;
        this.sidebarElement.style.width = width + "px";
        this.snapshotViews.style.left = width + "px";
        this.snapshotViewStatusBarItemsContainer.style.left = width + "px";
        this.sidebarResizeElement.style.left = (width - 3) + "px";
    }
};

WebInspector.HeapProfilerPanel.prototype.__proto__ = WebInspector.Panel.prototype;

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
        var lastComparator = WebInspector.HeapSnapshotDataGridList.propertyComparator("objectsSize", false);
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
                "count": "objectsCount",
                "size": "objectsSize",
                "countDelta": this.showCountDeltaAsPercent ? "objectsCountDeltaPercent" : "objectsCountDelta",
                "sizeDelta": this.showSizeDeltaAsPercent ? "objectsSizeDeltaPercent" : "objectsSizeDelta"
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
    }
};

WebInspector.HeapSnapshotView.prototype.__proto__ = WebInspector.View.prototype;

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
        WebInspector.panels.heap.showSnapshot(this.snapshot);
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

WebInspector.HeapSnapshotDataGridNode = function(snapshotView, baseEntry, snapshotEntry, owningList)
{
    WebInspector.DataGridNode.call(this, null, false);

    this.snapshotView = snapshotView;
    this.list = owningList;

    if (!snapshotEntry)
        snapshotEntry = { cons: baseEntry.cons, count: 0, size: 0 };
    this.constructorName = snapshotEntry.cons;
    this.objectsCount = snapshotEntry.count;
    this.objectsSize = snapshotEntry.size;

    if (!baseEntry)
        baseEntry = { count: 0, size: 0 };
    this.baseObjectsCount = baseEntry.count;
    this.objectsCountDelta = this.objectsCount - this.baseObjectsCount;
    this.baseObjectsSize = baseEntry.size;
    this.objectsSizeDelta = this.objectsSize - this.baseObjectsSize;
};

WebInspector.HeapSnapshotDataGridNode.prototype = {
    get data()
    {
        var data = {};

        data["cons"] = this.constructorName;

        if (this.snapshotView.showCountAsPercent)
            data["count"] = WebInspector.UIString("%.2f%%", this.objectsCountPercent);
        else
            data["count"] = this.objectsCount;

        if (this.snapshotView.showSizeAsPercent)
            data["size"] = WebInspector.UIString("%.2f%%", this.objectsSizePercent);
        else
            data["size"] = Number.bytesToString(this.objectsSize);

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
            data["countDelta"] = showDeltaAsPercent(this.objectsCountDeltaPercent);
        else
            data["countDelta"] = WebInspector.UIString("%s%d", signForDelta(this.objectsCountDelta), Math.abs(this.objectsCountDelta));

        if (this.snapshotView.showSizeDeltaAsPercent)
            data["sizeDelta"] = showDeltaAsPercent(this.objectsSizeDeltaPercent);
        else
            data["sizeDelta"] = WebInspector.UIString("%s%s", signForDelta(this.objectsSizeDelta), Number.bytesToString(Math.abs(this.objectsSizeDelta)));

        return data;
    },

    get objectsCountPercent()
    {
        return this.objectsCount / this.list.objectsCount * 100.0;
    },

    get objectsSizePercent()
    {
        return this.objectsSize / this.list.objectsSize * 100.0;
    },

    get objectsCountDeltaPercent()
    {
        if (this.baseObjectsCount > 0) {
            if (this.objectsCount > 0)
                return this.objectsCountDelta / this.baseObjectsCount * 100.0;
            else
                return Number.NEGATIVE_INFINITY;
        } else
            return Number.POSITIVE_INFINITY;
    },

    get objectsSizeDeltaPercent()
    {
        if (this.baseObjectsSize > 0) {
            if (this.objectsSize > 0)
                return this.objectsSizeDelta / this.baseObjectsSize * 100.0;
            else
                return Number.NEGATIVE_INFINITY;
        } else
            return Number.POSITIVE_INFINITY;
    }
};

WebInspector.HeapSnapshotDataGridNode.prototype.__proto__ = WebInspector.DataGridNode.prototype;

WebInspector.HeapSnapshotDataGridList = function(snapshotView, baseEntries, snapshotEntries)
{
    this.list = this;
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
        for (var item in snapshotEntries)
            this.appendChild(new WebInspector.HeapSnapshotDataGridNode(this.snapshotView, baseEntries[item], snapshotEntries[item], this));

        for (item in baseEntries) {
            if (!(item in snapshotEntries))
                this.appendChild(new WebInspector.HeapSnapshotDataGridNode(this.snapshotView, baseEntries[item], null, this));
        }
    },

    sort: function(comparator, force) {
        if (!force && this.lastComparator === comparator)
            return;

        this.children.sort(comparator);
        this.lastComparator = comparator;
    },

    get objectsCount() {
        if (!this._objectsCount) {
            this._objectsCount = 0;
            for (var i = 0, n = this.children.length; i < n; ++i) {
                this._objectsCount += this.children[i].objectsCount;
            }
        }
        return this._objectsCount;
    },

    get objectsSize() {
        if (!this._objectsSize) {
            this._objectsSize = 0;
            for (var i = 0, n = this.children.length; i < n; ++i) {
                this._objectsSize += this.children[i].objectsSize;
            }
        }
        return this._objectsSize;
    }
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
