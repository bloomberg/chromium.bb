/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @unrestricted
 */
Resources.IDBDatabaseView = class extends UI.VBox {
  /**
   * @param {!Resources.IndexedDBModel} model
   * @param {!Resources.IndexedDBModel.Database} database
   */
  constructor(model, database) {
    super();

    this._model = model;

    this._reportView = new UI.ReportView(database.databaseId.name);
    this._reportView.show(this.contentElement);

    var bodySection = this._reportView.appendSection('');
    this._securityOriginElement = bodySection.appendField(Common.UIString('Security origin'));
    this._versionElement = bodySection.appendField(Common.UIString('Version'));

    var footer = this._reportView.appendSection('').appendRow();
    this._clearButton = UI.createTextButton(
        Common.UIString('Delete database'), () => this._deleteDatabase(), Common.UIString('Delete database'));
    footer.appendChild(this._clearButton);

    this._refreshButton = UI.createTextButton(
        Common.UIString('Refresh database'), () => this._refreshDatabaseButtonClicked(),
        Common.UIString('Refresh database'));
    footer.appendChild(this._refreshButton);

    this.update(database);
  }

  _refreshDatabase() {
    this._securityOriginElement.textContent = this._database.databaseId.securityOrigin;
    this._versionElement.textContent = this._database.version;
  }

  _refreshDatabaseButtonClicked() {
    this._model.refreshDatabase(this._database.databaseId);
  }

  /**
   * @param {!Resources.IndexedDBModel.Database} database
   */
  update(database) {
    this._database = database;
    this._refreshDatabase();
    this._updatedForTests();
  }

  _updatedForTests() {
    // Sniffed in tests.
  }

  _deleteDatabase() {
    UI.ConfirmDialog.show(
        this.element, Common.UIString('Are you sure you want to delete "%s"?', this._database.databaseId.name),
        () => this._model.deleteDatabase(this._database.databaseId));
  }
};

/**
 * @unrestricted
 */
Resources.IDBDataView = class extends UI.SimpleView {
  /**
   * @param {!Resources.IndexedDBModel} model
   * @param {!Resources.IndexedDBModel.DatabaseId} databaseId
   * @param {!Resources.IndexedDBModel.ObjectStore} objectStore
   * @param {?Resources.IndexedDBModel.Index} index
   */
  constructor(model, databaseId, objectStore, index) {
    super(Common.UIString('IDB'));
    this.registerRequiredCSS('resources/indexedDBViews.css');

    this._model = model;
    this._databaseId = databaseId;
    this._isIndex = !!index;

    this.element.classList.add('indexed-db-data-view');

    this._createEditorToolbar();

    this._refreshButton = new UI.ToolbarButton(Common.UIString('Refresh'), 'largeicon-refresh');
    this._refreshButton.addEventListener(UI.ToolbarButton.Events.Click, this._refreshButtonClicked, this);

    this._clearButton = new UI.ToolbarButton(Common.UIString('Clear object store'), 'largeicon-clear');
    this._clearButton.addEventListener(UI.ToolbarButton.Events.Click, this._clearButtonClicked, this);

    this._pageSize = 50;
    this._skipCount = 0;

    this.update(objectStore, index);
    this._entries = [];
  }

  /**
   * @return {!DataGrid.DataGrid}
   */
  _createDataGrid() {
    var keyPath = this._isIndex ? this._index.keyPath : this._objectStore.keyPath;

    var columns = /** @type {!Array<!DataGrid.DataGrid.ColumnDescriptor>} */ ([]);
    columns.push({id: 'number', title: Common.UIString('#'), sortable: false, width: '50px'});
    columns.push(
        {id: 'key', titleDOMFragment: this._keyColumnHeaderFragment(Common.UIString('Key'), keyPath), sortable: false});
    if (this._isIndex) {
      columns.push({
        id: 'primaryKey',
        titleDOMFragment: this._keyColumnHeaderFragment(Common.UIString('Primary key'), this._objectStore.keyPath),
        sortable: false
      });
    }
    columns.push({id: 'value', title: Common.UIString('Value'), sortable: false});

    var dataGrid = new DataGrid.DataGrid(columns);
    dataGrid.setStriped(true);
    return dataGrid;
  }

  /**
   * @param {string} prefix
   * @param {*} keyPath
   * @return {!DocumentFragment}
   */
  _keyColumnHeaderFragment(prefix, keyPath) {
    var keyColumnHeaderFragment = createDocumentFragment();
    keyColumnHeaderFragment.createTextChild(prefix);
    if (keyPath === null)
      return keyColumnHeaderFragment;

    keyColumnHeaderFragment.createTextChild(' (' + Common.UIString('Key path: '));
    if (Array.isArray(keyPath)) {
      keyColumnHeaderFragment.createTextChild('[');
      for (var i = 0; i < keyPath.length; ++i) {
        if (i !== 0)
          keyColumnHeaderFragment.createTextChild(', ');
        keyColumnHeaderFragment.appendChild(this._keyPathStringFragment(keyPath[i]));
      }
      keyColumnHeaderFragment.createTextChild(']');
    } else {
      var keyPathString = /** @type {string} */ (keyPath);
      keyColumnHeaderFragment.appendChild(this._keyPathStringFragment(keyPathString));
    }
    keyColumnHeaderFragment.createTextChild(')');
    return keyColumnHeaderFragment;
  }

  /**
   * @param {string} keyPathString
   * @return {!DocumentFragment}
   */
  _keyPathStringFragment(keyPathString) {
    var keyPathStringFragment = createDocumentFragment();
    keyPathStringFragment.createTextChild('"');
    var keyPathSpan = keyPathStringFragment.createChild('span', 'source-code indexed-db-key-path');
    keyPathSpan.textContent = keyPathString;
    keyPathStringFragment.createTextChild('"');
    return keyPathStringFragment;
  }

  _createEditorToolbar() {
    var editorToolbar = new UI.Toolbar('data-view-toolbar', this.element);

    this._pageBackButton = new UI.ToolbarButton(Common.UIString('Show previous page'), 'largeicon-play-back');
    this._pageBackButton.addEventListener(UI.ToolbarButton.Events.Click, this._pageBackButtonClicked, this);
    editorToolbar.appendToolbarItem(this._pageBackButton);

    this._pageForwardButton = new UI.ToolbarButton(Common.UIString('Show next page'), 'largeicon-play');
    this._pageForwardButton.setEnabled(false);
    this._pageForwardButton.addEventListener(UI.ToolbarButton.Events.Click, this._pageForwardButtonClicked, this);
    editorToolbar.appendToolbarItem(this._pageForwardButton);

    this._keyInputElement = editorToolbar.element.createChild('input', 'key-input');
    this._keyInputElement.placeholder = Common.UIString('Start from key');
    this._keyInputElement.addEventListener('paste', this._keyInputChanged.bind(this), false);
    this._keyInputElement.addEventListener('cut', this._keyInputChanged.bind(this), false);
    this._keyInputElement.addEventListener('keypress', this._keyInputChanged.bind(this), false);
    this._keyInputElement.addEventListener('keydown', this._keyInputChanged.bind(this), false);
  }

  /**
   * @param {!Common.Event} event
   */
  _pageBackButtonClicked(event) {
    this._skipCount = Math.max(0, this._skipCount - this._pageSize);
    this._updateData(false);
  }

  /**
   * @param {!Common.Event} event
   */
  _pageForwardButtonClicked(event) {
    this._skipCount = this._skipCount + this._pageSize;
    this._updateData(false);
  }

  _keyInputChanged() {
    window.setTimeout(this._updateData.bind(this, false), 0);
  }

  /**
   * @param {!Resources.IndexedDBModel.ObjectStore} objectStore
   * @param {?Resources.IndexedDBModel.Index} index
   */
  update(objectStore, index) {
    this._objectStore = objectStore;
    this._index = index;

    if (this._dataGrid)
      this._dataGrid.asWidget().detach();
    this._dataGrid = this._createDataGrid();
    this._dataGrid.asWidget().show(this.element);

    this._skipCount = 0;
    this._updateData(true);
  }

  /**
   * @param {string} keyString
   */
  _parseKey(keyString) {
    var result;
    try {
      result = JSON.parse(keyString);
    } catch (e) {
      result = keyString;
    }
    return result;
  }

  /**
   * @param {boolean} force
   */
  _updateData(force) {
    var key = this._parseKey(this._keyInputElement.value);
    var pageSize = this._pageSize;
    var skipCount = this._skipCount;
    this._refreshButton.setEnabled(false);
    this._clearButton.setEnabled(!this._isIndex);

    if (!force && this._lastKey === key && this._lastPageSize === pageSize && this._lastSkipCount === skipCount)
      return;

    if (this._lastKey !== key || this._lastPageSize !== pageSize) {
      skipCount = 0;
      this._skipCount = 0;
    }
    this._lastKey = key;
    this._lastPageSize = pageSize;
    this._lastSkipCount = skipCount;

    /**
     * @param {!Array.<!Resources.IndexedDBModel.Entry>} entries
     * @param {boolean} hasMore
     * @this {Resources.IDBDataView}
     */
    function callback(entries, hasMore) {
      this._refreshButton.setEnabled(true);
      this.clear();
      this._entries = entries;
      for (var i = 0; i < entries.length; ++i) {
        var data = {};
        data['number'] = i + skipCount;
        data['key'] = entries[i].key;
        data['primaryKey'] = entries[i].primaryKey;
        data['value'] = entries[i].value;

        var node = new Resources.IDBDataGridNode(data);
        this._dataGrid.rootNode().appendChild(node);
      }

      this._pageBackButton.setEnabled(!!skipCount);
      this._pageForwardButton.setEnabled(hasMore);
      this._updatedDataForTests();
    }

    var idbKeyRange = key ? window.IDBKeyRange.lowerBound(key) : null;
    if (this._isIndex) {
      this._model.loadIndexData(
          this._databaseId, this._objectStore.name, this._index.name, idbKeyRange, skipCount, pageSize,
          callback.bind(this));
    } else {
      this._model.loadObjectStoreData(
          this._databaseId, this._objectStore.name, idbKeyRange, skipCount, pageSize, callback.bind(this));
    }
  }

  _updatedDataForTests() {
    // Sniffed in tests.
  }

  /**
   * @param {!Common.Event} event
   */
  _refreshButtonClicked(event) {
    this._updateData(true);
  }

  /**
   * @param {!Common.Event} event
   */
  async _clearButtonClicked(event) {
    this._clearButton.setEnabled(false);
    await this._model.clearObjectStore(this._databaseId, this._objectStore.name);
    this._clearButton.setEnabled(true);
    this._updateData(true);
  }

  /**
   * @override
   * @return {!Array.<!UI.ToolbarItem>}
   */
  syncToolbarItems() {
    return [this._refreshButton, this._clearButton];
  }

  clear() {
    this._dataGrid.rootNode().removeChildren();
    this._entries = [];
  }
};

/**
 * @unrestricted
 */
Resources.IDBDataGridNode = class extends DataGrid.DataGridNode {
  /**
   * @param {!Object.<string, *>} data
   */
  constructor(data) {
    super(data, false);
    this.selectable = false;
  }

  /**
   * @override
   * @return {!Element}
   */
  createCell(columnIdentifier) {
    var cell = super.createCell(columnIdentifier);
    var value = /** @type {!SDK.RemoteObject} */ (this.data[columnIdentifier]);

    switch (columnIdentifier) {
      case 'value':
      case 'key':
      case 'primaryKey':
        cell.removeChildren();
        var objectElement = ObjectUI.ObjectPropertiesSection.defaultObjectPresentation(value, undefined, true);
        cell.appendChild(objectElement);
        break;
      default:
    }

    return cell;
  }
};
