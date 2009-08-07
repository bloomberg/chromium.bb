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
    },

    addSnapshot: function(snapshot) {
        this._snapshots.push(snapshot);

        var sidebarParent = this.sidebarTree;
        var snapshotsTreeElement = new WebInspector.HeapSnapshotSidebarTreeElement(snapshot);
        snapshotsTreeElement.small = false;
        snapshot._snapshotsTreeElement = snapshotsTreeElement;

        sidebarParent.appendChild(snapshotsTreeElement);
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
            snapshot._snapshotView = new WebInspector.HeapSnapshotView(snapshot);
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

WebInspector.HeapSnapshotView = function(snapshot)
{
    WebInspector.View.call(this);

    this.element.addStyleClass("heap-snapshot-view");

    var columns = { "cons": { title: WebInspector.UIString("Constructor"), disclosure: true, sortable: true },
                    "count": { title: WebInspector.UIString("Count"), width: "54px", sortable: true },
                    "size": { title: WebInspector.UIString("Size"), width: "72px", sort: "descending", sortable: true } };

    this.dataGrid = new WebInspector.DataGrid(columns);
    this.dataGrid.addEventListener("sorting changed", this._sortData, this);
    this.element.appendChild(this.dataGrid.element);

    this.snapshot = snapshot;
    this.snapshotDataGridList = this.createSnapshotDataGridList();
    this.snapshotDataGridList.sort(WebInspector.HeapSnapshotDataGridList.propertyComparator("objectsSize", false));

    this.refresh();
};

WebInspector.HeapSnapshotView.prototype = {
    get statusBarItems()
    {
        return [];
    },

    get snapshot()
    {
        return this._snapshot;
    },

    set snapshot(snapshot)
    {
        this._snapshot = snapshot;
    },

    createSnapshotDataGridList: function()
    {
        if (!this._snapshotDataGridList)
            this._snapshotDataGridList = new WebInspector.HeapSnapshotDataGridList(this, this.snapshot.entries);
        return this._snapshotDataGridList;
    },

    refresh: function()
    {
        this.dataGrid.removeChildren();

        var children = this.snapshotDataGridList.children;
        var count = children.length;
        for (var index = 0; index < count; ++index)
            this.dataGrid.appendChild(children[index]);
    },

    _sortData: function()
    {
        var sortAscending = this.dataGrid.sortOrder === "ascending";
        var sortColumnIdentifier = this.dataGrid.sortColumnIdentifier;
        var sortProperty = {
                "cons": "constructorName",
                "count": "objectsCount",
                "size": "objectsSize"
        }[sortColumnIdentifier];

        this.snapshotDataGridList.sort(WebInspector.HeapSnapshotDataGridList.propertyComparator(sortProperty, sortAscending));

        this.refresh();
    }
};

WebInspector.HeapSnapshotView.prototype.__proto__ = WebInspector.View.prototype;

WebInspector.HeapSnapshotSidebarTreeElement = function(snapshot)
{
    this.snapshot = snapshot;
    this._snapshotNumber = snapshot.number;

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
        return WebInspector.UIString("Snapshot %d", this._snapshotNumber);
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
        return WebInspector.UIString("Used %s of %s", Number.bytesToString(this.snapshot.used), Number.bytesToString(this.snapshot.capacity));
    },

    set subtitle(x)
    {
        this._subTitle = x;
        this.refreshTitles();
    }
};

WebInspector.HeapSnapshotSidebarTreeElement.prototype.__proto__ = WebInspector.SidebarTreeElement.prototype;

WebInspector.HeapSnapshotDataGridNode = function(snapshotView, snapshotEntry, owningList)
{
    this.snapshotView = snapshotView;
    this.snapshotEntry = snapshotEntry;

    WebInspector.DataGridNode.call(this, null, false);

    this.list = owningList;
    this.lastComparator = null;

    this.constructorName = snapshotEntry.cons;
    this.objectsCount = snapshotEntry.count;
    this.objectsSize = snapshotEntry.size;
};

WebInspector.HeapSnapshotDataGridNode.prototype = {
    get data()
    {
        var data = {
            cons: this.constructorName,
            count: this.objectsCount,
            size: Number.bytesToString(this.objectsSize)
        };
        return data;
    }
};

WebInspector.HeapSnapshotDataGridNode.prototype.__proto__ = WebInspector.DataGridNode.prototype;

WebInspector.HeapSnapshotDataGridList = function(snapshotView, snapshotEntries)
{
    this.list = this;
    this.snapshotView = snapshotView;
    this.children = [];
    this.lastComparator = null;
    this.populateChildren(snapshotEntries);
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

    populateChildren: function(snapshotEntries)
    {
        var count = snapshotEntries.length;
        for (var index = 0; index < count; ++index)
            this.appendChild(new WebInspector.HeapSnapshotDataGridNode(this.snapshotView, snapshotEntries[index], this));
    },

    sort: function(comparator, force) {
        if (!force && this.lastComparator === comparator)
            return;

        this.children.sort(comparator);
        this.lastComparator = comparator;
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
