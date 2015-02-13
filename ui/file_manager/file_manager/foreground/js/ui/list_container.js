// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @struct
 */
function TextSearchState() {
  /**
   * @type {string}
   */
  this.text = '';

  /**
   * @type {!Date}
   */
  this.date = new Date();
}

/**
 * List container for the file table and the grid view.
 * @param {!HTMLElement} element Element of the container.
 * @param {!FileTable} table File table.
 * @param {!FileGrid} grid File grid.
 * @constructor
 * @struct
 */
function ListContainer(element, table, grid) {
  /**
   * The container element of the file list.
   * @type {!HTMLElement}
   * @const
   */
  this.element = element;

  /**
   * The file table.
   * @type {!FileTable}
   * @const
   */
  this.table = table;

  /**
   * The file grid.
   * @type {!FileGrid}
   * @const
   */
  this.grid = grid;

  /**
   * Current file list.
   * @type {ListContainer.ListType}
   */
  this.currentListType = ListContainer.ListType.UNINITIALIZED;

  /**
   * The input element to rename entry.
   * @type {!HTMLInputElement}
   * @const
   */
  this.renameInput =
      assertInstanceof(document.createElement('input'), HTMLInputElement);
  this.renameInput.className = 'rename entry-name';

  /**
   * Spinner on file list which is shown while loading.
   * @type {!HTMLElement}
   * @const
   */
  this.spinner = queryRequiredElement(element, '.spinner-layer');

  /**
   * @type {cr.ui.ArrayDataModel}
   */
  this.dataModel = null;

  /**
   * @type {ListThumbnailLoader}
   */
  this.listThumbnailLoader = null;

  /**
   * @type {cr.ui.ListSelectionModel|cr.ui.ListSingleSelectionModel}
   */
  this.selectionModel = null;

  /**
   * Data model which is used as a placefolder in inactive file list.
   * @type {!cr.ui.ArrayDataModel}
   * @const
   * @private
   */
  this.emptyDataModel_ = new cr.ui.ArrayDataModel([]);

  /**
   * Selection model which is used as a placefolder in inactive file list.
   * @type {!cr.ui.ListSelectionModel}
   * @const
   * @private
   */
  this.emptySelectionModel_ = new cr.ui.ListSelectionModel();

  /**
   * @type {!TextSearchState}
   * @const
   */
  this.textSearchState = new TextSearchState();

  // Overriding the default role 'list' to 'listbox' for better accessibility
  // on ChromeOS.
  this.table.list.setAttribute('role', 'listbox');
  this.table.list.id = 'file-list';
  this.grid.setAttribute('role', 'listbox');
  this.grid.id = 'file-list';
  this.element.addEventListener('keydown', this.onKeyDown_.bind(this));
  this.element.addEventListener('keypress', this.onKeyPress_.bind(this));
  this.element.addEventListener('mousemove', this.onMouseMove_.bind(this));
}

/**
 * @enum {string}
 * @const
 */
ListContainer.EventType = {
  TEXT_SEARCH: 'textsearch'
};

/**
 * @enum {string}
 * @const
 */
ListContainer.ListType = {
  UNINITIALIZED: 'uninitialized',
  DETAIL: 'detail',
  THUMBNAIL: 'thumb'
};

/**
 * Metadata property names used by FileTable and FileGrid.
 * These metadata is expected to be cached.
 * @const {!Array<string>}
 */
ListContainer.METADATA_PREFETCH_PROPERTY_NAMES = [
  'availableOffline',
  'customIconUrl',
  'hosted',
  'modificationTime',
  'shared',
  'size',
];

ListContainer.prototype = /** @struct */ {
  /**
   * @return {!FileTable|!FileGrid}
   */
  get currentView() {
    switch (this.currentListType) {
      case ListContainer.ListType.DETAIL:
        return this.table;
      case ListContainer.ListType.THUMBNAIL:
        return this.grid;
    }
    assertNotReached();
  },

  /**
   * @return {!cr.ui.List}
   */
  get currentList() {
    switch (this.currentListType) {
      case ListContainer.ListType.DETAIL:
        return this.table.list;
      case ListContainer.ListType.THUMBNAIL:
        return this.grid;
    }
    assertNotReached();
  }
};

/**
 * Notifies begginig of batch update to the UI.
 */
ListContainer.prototype.startBatchUpdates = function() {
  this.table.startBatchUpdates();
  this.grid.startBatchUpdates();
};

/**
 * Notifies end of batch update to the UI.
 */
ListContainer.prototype.endBatchUpdates = function() {
  this.table.endBatchUpdates();
  this.grid.endBatchUpdates();
};

/**
 * Sets the current list type.
 * @param {ListContainer.ListType} listType New list type.
 */
ListContainer.prototype.setCurrentListType = function(listType) {
  assert(this.dataModel);
  assert(this.selectionModel);

  this.startBatchUpdates();
  this.currentListType = listType;
  // TODO(dzvorygin): style.display and dataModel setting order shouldn't
  // cause any UI bugs. Currently, the only right way is first to set display
  // style and only then set dataModel.
  // Always sharing the data model between the detail/thumb views confuses
  // them.  Instead we maintain this bogus data model, and hook it up to the
  // view that is not in use.
  switch (listType) {
    case ListContainer.ListType.DETAIL:
      this.table.dataModel = this.dataModel;
      this.table.setListThumbnailLoader(this.listThumbnailLoader);
      this.table.selectionModel = this.selectionModel;
      this.table.hidden = false;
      this.grid.hidden = true;
      this.grid.selectionModel = this.emptySelectionModel_;
      this.grid.setListThumbnailLoader(null);
      this.grid.dataModel = this.emptyDataModel_;
      break;

    case ListContainer.ListType.THUMBNAIL:
      this.grid.dataModel = this.dataModel;
      this.grid.setListThumbnailLoader(this.listThumbnailLoader);
      this.grid.selectionModel = this.selectionModel;
      this.grid.hidden = false;
      this.table.hidden = true;
      this.table.selectionModel = this.emptySelectionModel_;
      this.table.setListThumbnailLoader(null);
      this.table.dataModel = this.emptyDataModel_;
      break;

    default:
      assertNotReached();
      break;
  }
  this.endBatchUpdates();
};

/**
 * Clears hover highlighting in the list container until next mouse move.
 */
ListContainer.prototype.clearHover = function() {
  this.element.classList.add('nohover');
};

/**
 * Finds list item element from the ancestor node.
 * @param {!HTMLElement} node
 * @return {cr.ui.ListItem}
 */
ListContainer.prototype.findListItemForNode = function(node) {
  var item = this.currentList.getListItemAncestor(node);
  // TODO(serya): list should check that.
  return item && this.currentList.isItem(item) ?
      assertInstanceof(item, cr.ui.ListItem) : null;
};

/**
 * KeyDown event handler for the div#list-container element.
 * @param {!Event} event Key event.
 * @private
 */
ListContainer.prototype.onKeyDown_ = function(event) {
  // Ignore keydown handler in the rename input box.
  if (event.srcElement.tagName == 'INPUT') {
    event.stopImmediatePropagation();
    return;
  }

  switch (event.keyIdentifier) {
    case 'Home':
    case 'End':
    case 'Up':
    case 'Down':
    case 'Left':
    case 'Right':
      // When navigating with keyboard we hide the distracting mouse hover
      // highlighting until the user moves the mouse again.
      this.clearHover();
      break;
  }
};

/**
 * KeyPress event handler for the div#list-container element.
 * @param {!Event} event Key event.
 * @private
 */
ListContainer.prototype.onKeyPress_ = function(event) {
  // Ignore keypress handler in the rename input box.
  if (event.srcElement.tagName == 'INPUT' ||
      event.ctrlKey ||
      event.metaKey ||
      event.altKey) {
    event.stopImmediatePropagation();
    return;
  }

  var now = new Date();
  var character = String.fromCharCode(event.charCode).toLowerCase();
  var text = now - this.textSearchState.date > 1000 ? '' :
      this.textSearchState.text;
  this.textSearchState.text = text + character;
  this.textSearchState.date = now;

  if (this.textSearchState.text)
    cr.dispatchSimpleEvent(this.element, ListContainer.EventType.TEXT_SEARCH);
};

/**
 * Mousemove event handler for the div#list-container element.
 * @param {Event} event Mouse event.
 * @private
 */
ListContainer.prototype.onMouseMove_ = function(event) {
  // The user grabbed the mouse, restore the hover highlighting.
  this.element.classList.remove('nohover');
};
