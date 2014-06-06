// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * A navigation list item.
 * @constructor
 * @extends {HTMLLIElement}
 */
var NavigationListItem = cr.ui.define('li');

NavigationListItem.prototype = {
  __proto__: HTMLLIElement.prototype,
  get modelItem() { return this.modelItem_; }
};

/**
 * Decorate the item.
 */
NavigationListItem.prototype.decorate = function() {
  // decorate() may be called twice: from the constructor and from
  // List.createItem(). This check prevents double-decorating.
  if (this.className)
    return;

  this.className = 'root-item';
  this.setAttribute('role', 'option');

  this.iconDiv_ = cr.doc.createElement('div');
  this.iconDiv_.className = 'volume-icon';
  this.appendChild(this.iconDiv_);

  this.label_ = cr.doc.createElement('div');
  this.label_.className = 'root-label entry-name';
  this.appendChild(this.label_);

  cr.defineProperty(this, 'lead', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(this, 'selected', cr.PropertyKind.BOOL_ATTR);
};

/**
 * Associate a model with this item.
 * @param {NavigationModelItem} modelItem NavigationModelItem of this item.
 */
NavigationListItem.prototype.setModelItem = function(modelItem) {
  if (this.modelItem_)
    console.warn('NavigationListItem.setModelItem should be called only once.');

  this.modelItem_ = modelItem;

  var typeIcon;
  if (modelItem.isVolume) {
    if (modelItem.volumeInfo.volumeType == 'provided') {
      var backgroundImage = '-webkit-image-set(' +
          'url(chrome://extension-icon/' + modelItem.volumeInfo.extensionId +
              '/24/1) 1x, ' +
          'url(chrome://extension-icon/' + modelItem.volumeInfo.extensionId +
              '/48/1) 2x);';
      // The icon div is not yet added to DOM, therefore it is impossible to
      // use style.backgroundImage.
      this.iconDiv_.setAttribute(
          'style', 'background-image: ' + backgroundImage);
    }
    typeIcon = modelItem.volumeInfo.volumeType;
  } else if (modelItem.isShortcut) {
    // Shortcuts are available for Drive only.
    typeIcon = VolumeManagerCommon.VolumeType.DRIVE;
  }

  this.iconDiv_.setAttribute('volume-type-icon', typeIcon);

  if (modelItem.isVolume) {
    this.iconDiv_.setAttribute(
        'volume-subtype', modelItem.volumeInfo.deviceType);
  }

  this.label_.textContent = modelItem.label;

  if (modelItem.isVolume &&
      (modelItem.volumeInfo.volumeType ===
           VolumeManagerCommon.VolumeType.ARCHIVE ||
       modelItem.volumeInfo.volumeType ===
           VolumeManagerCommon.VolumeType.REMOVABLE ||
       modelItem.volumeInfo.volumeType ===
           VolumeManagerCommon.VolumeType.PROVIDED)) {
    this.eject_ = cr.doc.createElement('div');
    // Block other mouse handlers.
    this.eject_.addEventListener(
        'mouseup', function(event) { event.stopPropagation() });
    this.eject_.addEventListener(
        'mousedown', function(event) { event.stopPropagation() });

    this.eject_.className = 'root-eject';
    this.eject_.addEventListener('click', function(event) {
      event.stopPropagation();
      cr.dispatchSimpleEvent(this, 'eject');
    }.bind(this));

    this.appendChild(this.eject_);
  }
};

/**
 * Associate a context menu with this item.
 * @param {cr.ui.Menu} menu Menu this item.
 */
NavigationListItem.prototype.maybeSetContextMenu = function(menu) {
  if (!this.modelItem_) {
    console.error('NavigationListItem.maybeSetContextMenu must be called ' +
                  'after setModelItem().');
    return;
  }

  // The context menu is shown on the following items:
  // - Removable and Archive volumes
  // - Folder shortcuts
  if (this.modelItem_.isVolume && (this.modelItem_.volumeInfo.volumeType ===
          VolumeManagerCommon.VolumeType.REMOVABLE ||
      this.modelItem_.volumeInfo.volumeType ===
          VolumeManagerCommon.VolumeType.ARCHIVE ||
      this.modelItem_.volumeInfo.volumeType ===
          VolumeManagerCommon.VolumeType.PROVIDED) ||
      this.modelItem_.isShortcut) {
    cr.ui.contextMenuHandler.setContextMenu(this, menu);
  }
};

/**
 * A navigation list.
 * @constructor
 * @extends {cr.ui.List}
 */
function NavigationList() {
}

/**
 * NavigationList inherits cr.ui.List.
 */
NavigationList.prototype = {
  __proto__: cr.ui.List.prototype,

  set dataModel(dataModel) {
    if (!this.onListContentChangedBound_)
      this.onListContentChangedBound_ = this.onListContentChanged_.bind(this);

    if (this.dataModel_) {
      this.dataModel_.removeEventListener(
          'change', this.onListContentChangedBound_);
      this.dataModel_.removeEventListener(
          'permuted', this.onListContentChangedBound_);
    }

    var parentSetter = cr.ui.List.prototype.__lookupSetter__('dataModel');
    parentSetter.call(this, dataModel);

    // This must be placed after the parent method is called, in order to make
    // it sure that the list was changed.
    dataModel.addEventListener('change', this.onListContentChangedBound_);
    dataModel.addEventListener('permuted', this.onListContentChangedBound_);
  },

  get dataModel() {
    return this.dataModel_;
  },

  // TODO(yoshiki): Add a setter of 'directoryModel'.
};

/**
 * @param {HTMLElement} el Element to be DirectoryItem.
 * @param {VolumeManagerWrapper} volumeManager The VolumeManager of the system.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 *     folders.
 */
NavigationList.decorate = function(el, volumeManager, directoryModel) {
  el.__proto__ = NavigationList.prototype;
  el.decorate(volumeManager, directoryModel);
};

/**
 * @param {VolumeManagerWrapper} volumeManager The VolumeManager of the system.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 */
NavigationList.prototype.decorate = function(volumeManager, directoryModel) {
  cr.ui.List.decorate(this);
  this.__proto__ = NavigationList.prototype;

  this.directoryModel_ = directoryModel;
  this.volumeManager_ = volumeManager;
  this.selectionModel = new cr.ui.ListSingleSelectionModel();

  this.directoryModel_.addEventListener('directory-changed',
      this.onCurrentDirectoryChanged_.bind(this));
  this.selectionModel.addEventListener(
      'change', this.onSelectionChange_.bind(this));
  this.selectionModel.addEventListener(
      'beforeChange', this.onBeforeSelectionChange_.bind(this));

  this.scrollBar_ = new ScrollBar();
  this.scrollBar_.initialize(this.parentNode, this);

  // Keeps track of selected model item to detect if it is changed actually.
  this.currentModelItem_ = null;

  // Overriding default role 'list' set by cr.ui.List.decorate() to 'listbox'
  // role for better accessibility on ChromeOS.
  this.setAttribute('role', 'listbox');

  var self = this;
  this.itemConstructor = function(modelItem) {
    return self.renderRoot_(modelItem);
  };
};

/**
 * This overrides cr.ui.List.measureItem().
 * In the method, a temporary element is added/removed from the list, and we
 * need to omit animations for such temporary items.
 *
 * @param {ListItem=} opt_item The list item to be measured.
 * @return {{height: number, marginTop: number, marginBottom:number,
 *     width: number, marginLeft: number, marginRight:number}} Size.
 * @override
 */
NavigationList.prototype.measureItem = function(opt_item) {
  this.measuringTemporaryItemNow_ = true;
  var result = cr.ui.List.prototype.measureItem.call(this, opt_item);
  this.measuringTemporaryItemNow_ = false;
  return result;
};

/**
 * Creates an element of a navigation list. This method is called from
 * cr.ui.List internally.
 *
 * @param {NavigationModelItem} modelItem NavigationModelItem to be rendered.
 * @return {NavigationListItem} Rendered element.
 * @private
 */
NavigationList.prototype.renderRoot_ = function(modelItem) {
  var item = new NavigationListItem();
  item.setModelItem(modelItem);

  var handleClick = function() {
    if (!item.selected)
      return;
    this.activateModelItem_(item.modelItem);
    // On clicking the root item, clears the selection.
    this.directoryModel_.clearSelection();
  }.bind(this);
  item.addEventListener('click', handleClick);

  var handleEject = function() {
    var unmountCommand = cr.doc.querySelector('command#unmount');
    // Let's make sure 'canExecute' state of the command is properly set for
    // the root before executing it.
    unmountCommand.canExecuteChange(item);
    unmountCommand.execute(item);
  };
  item.addEventListener('eject', handleEject);

  if (this.contextMenu_)
    item.maybeSetContextMenu(this.contextMenu_);

  return item;
};

/**
 * Sets a context menu. Context menu is enabled only on archive and removable
 * volumes as of now.
 *
 * @param {cr.ui.Menu} menu Context menu.
 */
NavigationList.prototype.setContextMenu = function(menu) {
  this.contextMenu_ = menu;

  for (var i = 0; i < this.dataModel.length; i++) {
    this.getListItemByIndex(i).maybeSetContextMenu(this.contextMenu_);
  }
};

/**
 * Selects the n-th item from the list.
 * @param {number} index Item index.
 * @return {boolean} True for success, otherwise false.
 */
NavigationList.prototype.selectByIndex = function(index) {
  if (index < 0 || index > this.dataModel.length - 1)
    return false;

  this.selectionModel.selectedIndex = index;
  this.activateModelItem_(this.dataModel.item(index));
  return true;
};


/**
 * Selects the passed item of the model.
 * @param {NavigationModelItem} modelItem Model item to be activated.
 * @private
 */
NavigationList.prototype.activateModelItem_ = function(modelItem) {
  var onEntryResolved = function(entry) {
    // Changes directory to the model item's root directory if needed.
    if (!util.isSameEntry(this.directoryModel_.getCurrentDirEntry(), entry)) {
      metrics.recordUserAction('FolderShortcut.Navigate');
      this.directoryModel_.changeDirectoryEntry(entry);
    }
  }.bind(this);

  if (modelItem.isVolume) {
    modelItem.volumeInfo.resolveDisplayRoot(
        onEntryResolved,
        function() {
          // Error, the display root is not available. It may happen on Drive.
          this.dataModel.onItemNotFoundError(modelItem);
        }.bind(this));
  } else if (modelItem.isShortcut) {
    // For shortcuts we already have an Entry, but it has to be resolved again
    // in case, it points to a non-existing directory.
    var url = modelItem.entry.toURL();
    webkitResolveLocalFileSystemURL(
        url,
        onEntryResolved,
        function() {
          // Error, the entry can't be re-resolved. It may happen for shortcuts
          // which targets got removed after resolving the Entry during
          // initialization.
          this.dataModel.onItemNotFoundError(modelItem);
        }.bind(this));
  }
};

/**
 * Handler before root item change.
 * @param {Event} event The event.
 * @private
 */
NavigationList.prototype.onBeforeSelectionChange_ = function(event) {
  if (event.changes.length == 1 && !event.changes[0].selected)
    event.preventDefault();
};

/**
 * Handler for root item being clicked.
 * @param {Event} event The event.
 * @private
 */
NavigationList.prototype.onSelectionChange_ = function(event) {
  var index = this.selectionModel.selectedIndex;
  if (index < 0 || index > this.dataModel.length - 1)
    return;

  // If the selected model item is not changed actually, we don't change the
  // current directory even if the selected index is changed.
  var modelItem = this.dataModel.item(index);
  if (modelItem === this.currentModelItem_)
    return;

  // Remembers the selected model item.
  this.currentModelItem_ = modelItem;

  // This handler is invoked even when the navigation list itself changes the
  // selection. In such case, we shouldn't handle the event.
  if (this.dontHandleSelectionEvent_)
    return;

  this.activateModelItem_(modelItem);
};

/**
 * Invoked when the current directory is changed.
 * @param {Event} event The event.
 * @private
 */
NavigationList.prototype.onCurrentDirectoryChanged_ = function(event) {
  this.selectBestMatchItem_();
};

/**
 * Invoked when the content in the data model is changed.
 * @param {Event} event The event.
 * @private
 */
NavigationList.prototype.onListContentChanged_ = function(event) {
  this.selectBestMatchItem_();
};

/**
 * Synchronizes the volume list selection with the current directory, after
 * it is changed outside of the volume list.
 * @private
 */
NavigationList.prototype.selectBestMatchItem_ = function() {
  var entry = this.directoryModel_.getCurrentDirEntry();
  // It may happen that the current directory is not set yet, for update events.
  if (!entry)
    return;

  // (1) Select the nearest shortcut directory.
  var entryURL = entry.toURL();
  var bestMatchIndex = -1;
  var bestMatchSubStringLen = 0;
  for (var i = 0; i < this.dataModel.length; i++) {
    var modelItem = this.dataModel.item(i);
    if (!modelItem.isShortcut)
      continue;
    var modelItemURL = modelItem.entry.toURL();
    // Relying on URL's format is risky and should be avoided. However, there is
    // no other way to quickly check if an entry is an ancestor of another one.
    if (entryURL.indexOf(modelItemURL) === 0) {
      if (bestMatchSubStringLen < modelItemURL.length) {
        bestMatchIndex = i;
        bestMatchSubStringLen = modelItemURL.length;
      }
    }
  }
  if (bestMatchIndex != -1) {
    // Don't to invoke the handler of this instance, sets the guard.
    this.dontHandleSelectionEvent_ = true;
    this.selectionModel.selectedIndex = bestMatchIndex;
    this.dontHandleSelectionEvent_ = false;
    return;
  }

  // (2) Selects the volume of the current directory.
  var volumeInfo = this.volumeManager_.getVolumeInfo(entry);
  if (!volumeInfo)
    return;
  for (var i = 0; i < this.dataModel.length; i++) {
    var modelItem = this.dataModel.item(i);
    if (!modelItem.isVolume)
      continue;
    if (modelItem.volumeInfo === volumeInfo) {
      // Don't to invoke the handler of this instance, sets the guard.
      this.dontHandleSelectionEvent_ = true;
      this.selectionModel.selectedIndex = i;
      this.dontHandleSelectionEvent_ = false;
      return;
    }
  }
};
