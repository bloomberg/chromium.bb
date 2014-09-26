// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

////////////////////////////////////////////////////////////////////////////////
// DirectoryTreeBase

/**
 * Implementation of methods for DirectoryTree and DirectoryItem. These classes
 * inherits cr.ui.Tree/TreeItem so we can't make them inherit this class.
 * Instead, we separate their implementations to this separate object and call
 * it with setting 'this' from DirectoryTree/Item.
 */
var DirectoryItemTreeBaseMethods = {};

/**
 * Updates sub-elements of {@code this} reading {@code DirectoryEntry}.
 * The list of {@code DirectoryEntry} are not updated by this method.
 *
 * @param {boolean} recursive True if the all visible sub-directories are
 *     updated recursively including left arrows. If false, the update walks
 *     only immediate child directories without arrows.
 */
DirectoryItemTreeBaseMethods.updateSubElementsFromList = function(recursive) {
  var index = 0;
  var tree = this.parentTree_ || this;  // If no parent, 'this' itself is tree.
  while (this.entries_[index]) {
    var currentEntry = this.entries_[index];
    var currentElement = this.items[index];
    var label = util.getEntryLabel(tree.volumeManager_, currentEntry);

    if (index >= this.items.length) {
      var item = new DirectoryItem(label, currentEntry, this, tree);
      this.add(item);
      index++;
    } else if (util.isSameEntry(currentEntry, currentElement.entry)) {
      currentElement.updateSharedStatusIcon();
      if (recursive && this.expanded)
        currentElement.updateSubDirectories(true /* recursive */);

      index++;
    } else if (currentEntry.toURL() < currentElement.entry.toURL()) {
      var item = new DirectoryItem(label, currentEntry, this, tree);
      this.addAt(item, index);
      index++;
    } else if (currentEntry.toURL() > currentElement.entry.toURL()) {
      this.remove(currentElement);
    }
  }

  var removedChild;
  while (removedChild = this.items[index]) {
    this.remove(removedChild);
  }

  if (index === 0) {
    this.hasChildren = false;
    this.expanded = false;
  } else {
    this.hasChildren = true;
  }
};

/**
 * Finds a parent directory of the {@code entry} in {@code this}, and
 * invokes the DirectoryItem.selectByEntry() of the found directory.
 *
 * @param {DirectoryEntry|Object} entry The entry to be searched for. Can be
 *     a fake.
 * @return {boolean} True if the parent item is found.
 */
DirectoryItemTreeBaseMethods.searchAndSelectByEntry = function(entry) {
  for (var i = 0; i < this.items.length; i++) {
    var item = this.items[i];
    if (util.isDescendantEntry(item.entry, entry) ||
        util.isSameEntry(item.entry, entry)) {
      item.selectByEntry(entry);
      return true;
    }
  }
  return false;
};

Object.freeze(DirectoryItemTreeBaseMethods);

var TREE_ITEM_INNTER_HTML =
    '<div class="tree-row">' +
    ' <span class="expand-icon"></span>' +
    ' <span class="icon"></span>' +
    ' <span class="label entry-name"></span>' +
    '</div>' +
    '<div class="tree-children"></div>';

////////////////////////////////////////////////////////////////////////////////
// DirectoryItem

/**
 * A directory in the tree. Each element represents one directory.
 *
 * @param {string} label Label for this item.
 * @param {DirectoryEntry} dirEntry DirectoryEntry of this item.
 * @param {DirectoryItem|DirectoryTree} parentDirItem Parent of this item.
 * @param {DirectoryTree} tree Current tree, which contains this item.
 * @extends {cr.ui.TreeItem}
 * @constructor
 */
function DirectoryItem(label, dirEntry, parentDirItem, tree) {
  var item = new cr.ui.TreeItem();
  DirectoryItem.decorate(item, label, dirEntry, parentDirItem, tree);
  return item;
}

/**
 * @param {HTMLElement} el Element to be DirectoryItem.
 * @param {string} label Label for this item.
 * @param {DirectoryEntry} dirEntry DirectoryEntry of this item.
 * @param {DirectoryItem|DirectoryTree} parentDirItem Parent of this item.
 * @param {DirectoryTree} tree Current tree, which contains this item.
 */
DirectoryItem.decorate =
    function(el, label, dirEntry, parentDirItem, tree) {
  el.__proto__ = DirectoryItem.prototype;
  (/** @type {DirectoryItem} */ el).decorate(
      label, dirEntry, parentDirItem, tree);
};

DirectoryItem.prototype = {
  __proto__: cr.ui.TreeItem.prototype,

  /**
   * The DirectoryEntry corresponding to this DirectoryItem. This may be
   * a dummy DirectoryEntry.
   * @type {DirectoryEntry|Object}
   */
  get entry() {
    return this.dirEntry_;
  },

  /**
   * The element containing the label text and the icon.
   * @type {!HTMLElement}
   * @override
   */
  get labelElement() {
    return this.firstElementChild.querySelector('.label');
  }
};

/**
 * Calls DirectoryItemTreeBaseMethods.updateSubElementsFromList().
 *
 * @param {boolean} recursive True if the all visible sub-directories are
 *     updated recursively including left arrows. If false, the update walks
 *     only immediate child directories without arrows.
 */
DirectoryItem.prototype.updateSubElementsFromList = function(recursive) {
  DirectoryItemTreeBaseMethods.updateSubElementsFromList.call(this, recursive);
};

/**
 * Calls DirectoryItemTreeBaseMethods.updateSubElementsFromList().
 *
 * @param {DirectoryEntry|Object} entry The entry to be searched for. Can be
 *     a fake.
 * @return {boolean} True if the parent item is found.
 */
DirectoryItem.prototype.searchAndSelectByEntry = function(entry) {
  return DirectoryItemTreeBaseMethods.searchAndSelectByEntry.call(this, entry);
};

/**
 * @param {string} label Localized label for this item.
 * @param {DirectoryEntry} dirEntry DirectoryEntry of this item.
 * @param {DirectoryItem|DirectoryTree} parentDirItem Parent of this item.
 * @param {DirectoryTree} tree Current tree, which contains this item.
 */
DirectoryItem.prototype.decorate = function(
    label, dirEntry, parentDirItem, tree) {
  this.innerHTML = TREE_ITEM_INNTER_HTML;
  this.parentTree_ = tree;
  this.directoryModel_ = tree.directoryModel;
  this.parent_ = parentDirItem;
  this.label = label;
  this.dirEntry_ = dirEntry;
  this.fileFilter_ = this.directoryModel_.getFileFilter();

  // Sets hasChildren=false tentatively. This will be overridden after
  // scanning sub-directories in updateSubElementsFromList().
  this.hasChildren = false;

  this.addEventListener('expand', this.onExpand_.bind(this), false);
  var icon = this.querySelector('.icon');
  icon.classList.add('volume-icon');
  var location = tree.volumeManager.getLocationInfo(dirEntry);
  if (location && location.rootType && location.isRootEntry) {
    icon.setAttribute('volume-type-icon', location.rootType);
  } else {
    icon.setAttribute('file-type-icon', 'folder');
    this.updateSharedStatusIcon();
  }

  if (this.parentTree_.contextMenuForSubitems)
    this.setContextMenu(this.parentTree_.contextMenuForSubitems);
  // Adds handler for future change.
  this.parentTree_.addEventListener(
      'contextMenuForSubitemsChange',
      function(e) { this.setContextMenu(e.newValue); }.bind(this));

  if (parentDirItem.expanded)
    this.updateSubDirectories(false /* recursive */);
};

/**
 * Overrides WebKit's scrollIntoViewIfNeeded, which doesn't work well with
 * a complex layout. This call is not necessary, so we are ignoring it.
 *
 * @param {boolean} unused Unused.
 * @override
 */
DirectoryItem.prototype.scrollIntoViewIfNeeded = function(unused) {
};

/**
 * Removes the child node, but without selecting the parent item, to avoid
 * unintended changing of directories. Removing is done externally, and other
 * code will navigate to another directory.
 *
 * @param {!cr.ui.TreeItem} child The tree item child to remove.
 * @override
 */
DirectoryItem.prototype.remove = function(child) {
  this.lastElementChild.removeChild(child);
  if (this.items.length == 0)
    this.hasChildren = false;
};

/**
 * Invoked when the item is being expanded.
 * @param {!UIEvent} e Event.
 * @private
 **/
DirectoryItem.prototype.onExpand_ = function(e) {
  this.updateSubDirectories(
      true /* recursive */,
      function() {},
      function() {
        this.expanded = false;
      }.bind(this));

  e.stopPropagation();
};

/**
 * Invoked when the tree item is clicked.
 *
 * @param {Event} e Click event.
 * @override
 */
DirectoryItem.prototype.handleClick = function(e) {
  cr.ui.TreeItem.prototype.handleClick.call(this, e);
  if (!e.target.classList.contains('expand-icon'))
    this.directoryModel_.activateDirectoryEntry(this.entry);
};

/**
 * Retrieves the latest subdirectories and update them on the tree.
 * @param {boolean} recursive True if the update is recursively.
 * @param {function()=} opt_successCallback Callback called on success.
 * @param {function()=} opt_errorCallback Callback called on error.
 */
DirectoryItem.prototype.updateSubDirectories = function(
    recursive, opt_successCallback, opt_errorCallback) {
  if (util.isFakeEntry(this.entry)) {
    if (opt_errorCallback)
      opt_errorCallback();
    return;
  }

  var sortEntries = function(fileFilter, entries) {
    entries.sort(util.compareName);
    return entries.filter(fileFilter.filter.bind(fileFilter));
  };

  var onSuccess = function(entries) {
    this.entries_ = entries;
    this.redrawSubDirectoryList_(recursive);
    opt_successCallback && opt_successCallback();
  }.bind(this);

  var reader = this.entry.createReader();
  var entries = [];
  var readEntry = function() {
    reader.readEntries(function(results) {
      if (!results.length) {
        onSuccess(sortEntries(this.fileFilter_, entries));
        return;
      }

      for (var i = 0; i < results.length; i++) {
        var entry = results[i];
        if (entry.isDirectory)
          entries.push(entry);
      }
      readEntry();
    }.bind(this));
  }.bind(this);
  readEntry();
};

/**
 * Searches for the changed directory in the current subtree, and if it is found
 * then updates it.
 *
 * @param {DirectoryEntry} changedDirectoryEntry The entry ot the changed
 *     directory.
 */
DirectoryItem.prototype.updateItemByEntry = function(changedDirectoryEntry) {
  if (util.isSameEntry(changedDirectoryEntry, this.entry)) {
    this.updateSubDirectories(false /* recursive */);
    return;
  }

  // Traverse the entire subtree to find the changed element.
  for (var i = 0; i < this.items.length; i++) {
    var item = this.items[i];
    if (util.isDescendantEntry(item.entry, changedDirectoryEntry) ||
        util.isSameEntry(item.entry, changedDirectoryEntry)) {
      item.updateItemByEntry(changedDirectoryEntry);
      break;
    }
  }
};

/**
 * Update the icon based on whether the folder is shared on Drive.
 */
DirectoryItem.prototype.updateSharedStatusIcon = function() {
  var icon = this.querySelector('.icon');
  this.parentTree_.metadataCache.getLatest(
      [this.dirEntry_],
      'external',
      function(metadata) {
        icon.classList.toggle('shared', metadata[0] && metadata[0].shared);
      });
};

/**
 * Redraw subitems with the latest information. The items are sorted in
 * alphabetical order, case insensitive.
 * @param {boolean} recursive True if the update is recursively.
 * @private
 */
DirectoryItem.prototype.redrawSubDirectoryList_ = function(recursive) {
  this.updateSubElementsFromList(recursive);
};

/**
 * Select the item corresponding to the given {@code entry}.
 * @param {DirectoryEntry|Object} entry The entry to be selected. Can be a fake.
 */
DirectoryItem.prototype.selectByEntry = function(entry) {
  if (util.isSameEntry(entry, this.entry)) {
    this.selected = true;
    return;
  }

  if (this.searchAndSelectByEntry(entry))
    return;

  // If the entry doesn't exist, updates sub directories and tries again.
  this.updateSubDirectories(
      false /* recursive */,
      this.searchAndSelectByEntry.bind(this, entry));
};

/**
 * Executes the assigned action as a drop target.
 */
DirectoryItem.prototype.doDropTargetAction = function() {
  this.expanded = true;
};

/**
 * Sets the context menu for directory tree.
 * @param {cr.ui.Menu} menu Menu to be set.
 */
DirectoryItem.prototype.setContextMenu = function(menu) {
  var tree = this.parentTree_ || this;  // If no parent, 'this' itself is tree.
  var locationInfo = tree.volumeManager_.getLocationInfo(this.entry);
  if (locationInfo && locationInfo.isEligibleForFolderShortcut)
    cr.ui.contextMenuHandler.setContextMenu(this, menu);
};

/**
 * Change current directory to the entry of this item.
 */
DirectoryItem.prototype.activate = function() {
  this.parentTree_.directoryModel.activateDirectoryEntry(this.entry);
};

////////////////////////////////////////////////////////////////////////////////
// VolumeItem

/**
 * A TreeItem which represents a volume. Volume items are displayed as
 * top-level children of DirectoryTree.
 *
 * @param {NavigationModelItem} modelItem NavigationModelItem of this volume.
 * @param {DirectoryTree} tree Current tree, which contains this item.
 * @extends {cr.ui.TreeItem}
 * @constructor
 */
function VolumeItem(modelItem, tree) {
  var item = new cr.ui.TreeItem();
  item.__proto__ = VolumeItem.prototype;
  item.decorate(modelItem, tree);
  return item;
}

VolumeItem.prototype = {
  __proto__: cr.ui.TreeItem.prototype,
  get entry() {
    return this.volumeInfo_.displayRoot;
  },
  get modelItem() {
    return this.modelItem_;
  },
  get volumeInfo() {
    return this.volumeInfo_;
  },
  get labelElement() {
    return this.firstElementChild.querySelector('.label');
  },
  // Overrides the property 'expanded' to prevent volume items from shrinking.
  get expanded() {
    return Object.getOwnPropertyDescriptor(
        cr.ui.TreeItem.prototype, 'expanded').get.call(this);
  },
  set expanded(b) {
    if (!b)
      return;
    Object.getOwnPropertyDescriptor(
        cr.ui.TreeItem.prototype, 'expanded').set.call(this, b);
  }
};

/**
 * Calls DirectoryItemTreeBaseMethods.updateSubElementsFromList().
 *
 * @param {DirectoryEntry|Object} entry The entry to be searched for. Can be
 *     a fake.
 * @return {boolean} True if the parent item is found.
 */
VolumeItem.prototype.searchAndSelectByEntry = function(entry) {
  return DirectoryItemTreeBaseMethods.searchAndSelectByEntry.call(this, entry);
};

/**
 * Decorates this element.
 * @param {NavigationModelItem} modelItem NavigationModelItem of this volume.
 * @param {DirectoryTree} tree Current tree, which contains this item.
 */
VolumeItem.prototype.decorate = function(modelItem, tree) {
  this.innerHTML = TREE_ITEM_INNTER_HTML;
  this.parentTree_ = tree;
  this.modelItem_ = modelItem;
  this.volumeInfo_ = modelItem.volumeInfo;
  this.label = modelItem.volumeInfo.label;

  this.setupIcon_(this.querySelector('.icon'), this.volumeInfo);
  this.setupEjectButton_(this.rowElement);
  if (tree.contextMenuForRootItems)
    this.setContextMenu(tree.contextMenuForRootItems);

  // Populate children of this volume using resolved display root.
  this.volumeInfo_.resolveDisplayRoot(function(displayRoot) {
    this.updateSubDirectories(false /* recursive */);
  }.bind(this));
};

/**
 * Invoked when the tree item is clicked.
 *
 * @param {Event} e Click event.
 * @override
 */
VolumeItem.prototype.handleClick = function(e) {
  // If the currently selected volume is clicked, change current directory to
  // the volume's root.
  if (this.selected)
    this.activate();

  cr.ui.TreeItem.prototype.handleClick.call(this, e);

  // Resets file selection when a volume is clicked.
  this.parentTree_.directoryModel.clearSelection();

  // If the Drive volume is clicked, select one of the children instead of this
  // item itself.
  if (this.isDrive()) {
    this.volumeInfo_.resolveDisplayRoot(function(displayRoot) {
      this.searchAndSelectByEntry(displayRoot);
    }.bind(this));
  }
};

/**
 * Retrieves the latest subdirectories and update them on the tree.
 * @param {boolean} recursive True if the update is recursively.
 */
VolumeItem.prototype.updateSubDirectories = function(recursive) {
  // Drive volume has children including fake entries (offline, recent, etc...).
  if (this.isDrive() && this.entry && !this.hasChildren) {
    var entries = [this.entry];
    if (this.parentTree_.fakeEntriesVisible_) {
      for (var key in this.volumeInfo.fakeEntries)
        entries.push(this.volumeInfo.fakeEntries[key]);
    }
    // This list is sorted by URL on purpose.
    entries.sort(function(a, b) { return a.toURL() < b.toURL(); });

    for (var i = 0; i < entries.length; i++) {
      var item = new DirectoryItem(
          util.getEntryLabel(this.parentTree_.volumeManager_, entries[i]),
          entries[i], this, this.parentTree_);
      this.add(item);
      item.updateSubDirectories(false);
    }
    this.expanded = true;
  }
};

/**
 * Searches for the changed directory in the current subtree, and if it is found
 * then updates it.
 *
 * @param {DirectoryEntry} changedDirectoryEntry The entry ot the changed
 *     directory.
 */
VolumeItem.prototype.updateItemByEntry = function(changedDirectoryEntry) {
  if (this.isDrive())
    this.items[0].updateItemByEntry(changedDirectoryEntry);
};

/**
 * Select the item corresponding to the given entry.
 * @param {DirectoryEntry|Object} entry The directory entry to be selected. Can
 *     be a fake.
 */
VolumeItem.prototype.selectByEntry = function(entry) {
  // If this volume is drive, find the item to be selected amang children.
  if (this.isDrive()) {
    this.searchAndSelectByEntry(entry);
    return;
  }
  if (util.isSameEntry(this.entry, entry) ||
      util.isDescendantEntry(this.entry, entry))
    this.selected = true;
};

/**
 * Sets the context menu for volume items.
 * @param {cr.ui.Menu} menu Menu to be set.
 */
VolumeItem.prototype.setContextMenu = function(menu) {
  if (this.isRemovable_())
    cr.ui.contextMenuHandler.setContextMenu(this, menu);
};

/**
 * Change current entry to this volume's root directory.
 */
VolumeItem.prototype.activate = function() {
  var directoryModel = this.parentTree_.directoryModel;
  var onEntryResolved = function(entry) {
    // Changes directory to the model item's root directory if needed.
    if (!util.isSameEntry(directoryModel.getCurrentDirEntry(), entry)) {
      metrics.recordUserAction('FolderShortcut.Navigate');
      directoryModel.changeDirectoryEntry(entry);
    }
    // In case of failure in resolveDisplayRoot() in the volume's decorate(),
    // update the volume's children here.
    this.updateSubDirectories(false);
  }.bind(this);

  this.volumeInfo.resolveDisplayRoot(
      onEntryResolved,
      function() {
        // Error, the display root is not available. It may happen on Drive.
        this.parentTree_.dataModel.onItemNotFoundError(this.modelItem);
      }.bind(this));
};

/**
 * @return {boolean} True if this is Drive volume.
 */
VolumeItem.prototype.isDrive = function() {
  return this.volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE;
};

/**
 * @return {boolean} True if this volume can be removed by user operation.
 * @private
 */
VolumeItem.prototype.isRemovable_ = function() {
  var volumeType = this.volumeInfo.volumeType;
  return volumeType === VolumeManagerCommon.VolumeType.ARCHIVE ||
         volumeType === VolumeManagerCommon.VolumeType.REMOVABLE ||
         volumeType === VolumeManagerCommon.VolumeType.PROVIDED;
};

/**
 * Set up icon of this volume item.
 * @param {HTMLElement} icon Icon element to be setup.
 * @param {VolumeInfo} volumeInfo VolumeInfo determines the icon type.
 * @private
 */
VolumeItem.prototype.setupIcon_ = function(icon, volumeInfo) {
  icon.classList.add('volume-icon');
  if (volumeInfo.volumeType === 'provided') {
    var backgroundImage = '-webkit-image-set(' +
        'url(chrome://extension-icon/' + volumeInfo.extensionId +
            '/24/1) 1x, ' +
        'url(chrome://extension-icon/' + volumeInfo.extensionId +
            '/48/1) 2x);';
    // The icon div is not yet added to DOM, therefore it is impossible to
    // use style.backgroundImage.
    icon.setAttribute(
        'style', 'background-image: ' + backgroundImage);
  }
  icon.setAttribute('volume-type-icon', volumeInfo.volumeType);
  icon.setAttribute('volume-subtype', volumeInfo.deviceType);
};

/**
 * Set up eject button if needed.
 * @param {HTMLElement} rowElement The parent element for eject button.
 * @private
 */
VolumeItem.prototype.setupEjectButton_ = function(rowElement) {
  if (this.isRemovable_()) {
    var ejectButton = cr.doc.createElement('div');
    // Block other mouse handlers.
    ejectButton.addEventListener(
        'mouseup', function(event) { event.stopPropagation() });
    ejectButton.addEventListener(
        'mousedown', function(event) { event.stopPropagation() });
    ejectButton.className = 'root-eject';
    ejectButton.addEventListener('click', function(event) {
      event.stopPropagation();
      var unmountCommand = cr.doc.querySelector('command#unmount');
      // Let's make sure 'canExecute' state of the command is properly set for
      // the root before executing it.
      unmountCommand.canExecuteChange(this);
      unmountCommand.execute(this);
    }.bind(this));
    rowElement.appendChild(ejectButton);
  }
};

////////////////////////////////////////////////////////////////////////////////
// ShortcutItem

/**
 * A TreeItem which represents a shortcut for Drive folder.
 * Shortcut items are displayed as top-level children of DirectoryTree.
 *
 * @param {NavigationModelItem} modelItem NavigationModelItem of this volume.
 * @param {DirectoryTree} tree Current tree, which contains this item.
 * @extends {cr.ui.TreeItem}
 * @constructor
 */
function ShortcutItem(modelItem, tree) {
  var item = new cr.ui.TreeItem();
  item.__proto__ = ShortcutItem.prototype;
  item.decorate(modelItem, tree);
  return item;
}

ShortcutItem.prototype = {
  __proto__: cr.ui.TreeItem.prototype,
  get entry() {
    return this.dirEntry_;
  },
  get modelItem() {
    return this.modelItem_;
  },
  get labelElement() {
    return this.firstElementChild.querySelector('.label');
  }
};

/**
 * Finds a parent directory of the {@code entry} in {@code this}, and
 * invokes the DirectoryItem.selectByEntry() of the found directory.
 *
 * @param {DirectoryEntry|Object} entry The entry to be searched for. Can be
 *     a fake.
 * @return {boolean} True if the parent item is found.
 */
ShortcutItem.prototype.searchAndSelectByEntry = function(entry) {
  // Always false as shortcuts have no children.
  return false;
};

/**
 * Decorates this element.
 * @param {NavigationModelItem} modelItem NavigationModelItem of this volume.
 * @param {DirectoryTree} tree Current tree, which contains this item.
 */
ShortcutItem.prototype.decorate = function(modelItem, tree) {
  this.innerHTML = TREE_ITEM_INNTER_HTML;
  this.parentTree_ = tree;
  this.label = modelItem.entry.name;
  this.dirEntry_ = modelItem.entry;
  this.modelItem_ = modelItem;

  var icon = this.querySelector('.icon');
  icon.classList.add('volume-icon');
  icon.setAttribute('volume-type-icon', VolumeManagerCommon.VolumeType.DRIVE);

  if (tree.contextMenuForRootItems)
    this.setContextMenu(tree.contextMenuForRootItems);
};

/**
 * Invoked when the tree item is clicked.
 *
 * @param {Event} e Click event.
 * @override
 */
ShortcutItem.prototype.handleClick = function(e) {
  cr.ui.TreeItem.prototype.handleClick.call(this, e);
  this.parentTree_.directoryModel.clearSelection();
};

/**
 * Select the item corresponding to the given entry.
 * @param {DirectoryEntry} entry The directory entry to be selected.
 */
ShortcutItem.prototype.selectByEntry = function(entry) {
  if (util.isSameEntry(entry, this.entry))
    this.selected = true;
};

/**
 * Sets the context menu for shortcut items.
 * @param {cr.ui.Menu} menu Menu to be set.
 */
ShortcutItem.prototype.setContextMenu = function(menu) {
  cr.ui.contextMenuHandler.setContextMenu(this, menu);
};

/**
 * Change current entry to the entry corresponding to this shortcut.
 */
ShortcutItem.prototype.activate = function() {
  var directoryModel = this.parentTree_.directoryModel;
  var onEntryResolved = function(entry) {
    // Changes directory to the model item's root directory if needed.
    if (!util.isSameEntry(directoryModel.getCurrentDirEntry(), entry)) {
      metrics.recordUserAction('FolderShortcut.Navigate');
      directoryModel.changeDirectoryEntry(entry);
    }
  }.bind(this);

  // For shortcuts we already have an Entry, but it has to be resolved again
  // in case, it points to a non-existing directory.
  webkitResolveLocalFileSystemURL(
      this.entry.toURL(),
      onEntryResolved,
      function() {
        // Error, the entry can't be re-resolved. It may happen for shortcuts
        // which targets got removed after resolving the Entry during
        // initialization.
        this.parentTree_.dataModel.onItemNotFoundError(this.modelItem);
      }.bind(this));
};

////////////////////////////////////////////////////////////////////////////////
// DirectoryTree

/**
 * Tree of directories on the middle bar. This element is also the root of
 * items, in other words, this is the parent of the top-level items.
 *
 * @constructor
 * @extends {cr.ui.Tree}
 */
function DirectoryTree() {}

/**
 * Decorates an element.
 * @param {HTMLElement} el Element to be DirectoryTree.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 * @param {VolumeManagerWrapper} volumeManager VolumeManager of the system.
 * @param {MetadataCache} metadataCache Shared MetadataCache instance.
 * @param {boolean} fakeEntriesVisible True if it should show the fakeEntries.
 */
DirectoryTree.decorate = function(
    el, directoryModel, volumeManager, metadataCache, fakeEntriesVisible) {
  el.__proto__ = DirectoryTree.prototype;
  (/** @type {DirectoryTree} */ el).decorate(
      directoryModel, volumeManager, metadataCache, fakeEntriesVisible);
};

DirectoryTree.prototype = {
  __proto__: cr.ui.Tree.prototype,

  // DirectoryTree is always expanded.
  get expanded() { return true; },
  /**
   * @param {boolean} value Not used.
   */
  set expanded(value) {},

  /**
   * The DirectoryEntry corresponding to this DirectoryItem. This may be
   * a dummy DirectoryEntry.
   * @type {DirectoryEntry|Object}
   * @override
   **/
  get entry() {
    return this.dirEntry_;
  },

  /**
   * The DirectoryModel this tree corresponds to.
   * @type {DirectoryModel}
   */
  get directoryModel() {
    return this.directoryModel_;
  },

  /**
   * The VolumeManager instance of the system.
   * @type {VolumeManager}
   */
  get volumeManager() {
    return this.volumeManager_;
  },

  /**
   * The reference to shared MetadataCache instance.
   * @type {MetadataCache}
   */
  get metadataCache() {
    return this.metadataCache_;
  },

  set dataModel(dataModel) {
    if (!this.onListContentChangedBound_)
      this.onListContentChangedBound_ = this.onListContentChanged_.bind(this);

    if (this.dataModel_) {
      this.dataModel_.removeEventListener(
          'change', this.onListContentChangedBound_);
      this.dataModel_.removeEventListener(
          'permuted', this.onListContentChangedBound_);
    }
    this.dataModel_ = dataModel;
    dataModel.addEventListener('change', this.onListContentChangedBound_);
    dataModel.addEventListener('permuted', this.onListContentChangedBound_);
  },

  get dataModel() {
    return this.dataModel_;
  }
};

cr.defineProperty(DirectoryTree, 'contextMenuForSubitems', cr.PropertyKind.JS);
cr.defineProperty(DirectoryTree, 'contextMenuForRootItems', cr.PropertyKind.JS);

/**
 * Calls DirectoryItemTreeBaseMethods.updateSubElementsFromList().
 *
 * @param {boolean} recursive True if the all visible sub-directories are
 *     updated recursively including left arrows. If false, the update walks
 *     only immediate child directories without arrows.
 */
DirectoryTree.prototype.updateSubElementsFromList = function(recursive) {
  // First, current items which is not included in the dataModel should be
  // removed.
  for (var i = 0; i < this.items.length;) {
    var found = false;
    for (var j = 0; j < this.dataModel.length; j++) {
      if (NavigationModelItem.isSame(this.items[i].modelItem,
                                     this.dataModel.item(j))) {
        found = true;
        break;
      }
    }
    if (!found) {
      if (this.items[i].selected)
        this.items[i].selected = false;
      this.remove(this.items[i]);
    } else {
      i++;
    }
  }

  // Next, insert items which is in dataModel but not in current items.
  var modelIndex = 0;
  var itemIndex = 0;
  while (modelIndex < this.dataModel.length) {
    if (itemIndex < this.items.length &&
        NavigationModelItem.isSame(this.items[itemIndex].modelItem,
                                   this.dataModel.item(modelIndex))) {
      if (recursive && this.items[itemIndex] instanceof VolumeItem)
        this.items[itemIndex].updateSubDirectories(true);
    } else {
      var modelItem = this.dataModel.item(modelIndex);
      if (modelItem.isVolume)
        this.addAt(new VolumeItem(modelItem, this), itemIndex);
      else
        this.addAt(new ShortcutItem(modelItem, this), itemIndex);
    }
    itemIndex++;
    modelIndex++;
  }

  if (itemIndex !== 0)
    this.hasChildren = true;
};

/**
 * Finds a parent directory of the {@code entry} in {@code this}, and
 * invokes the DirectoryItem.selectByEntry() of the found directory.
 *
 * @param {DirectoryEntry|Object} entry The entry to be searched for. Can be
 *     a fake.
 * @return {boolean} True if the parent item is found.
 */
DirectoryTree.prototype.searchAndSelectByEntry = function(entry) {
  // If the |entry| is same as one of volumes or shortcuts, select it.
  for (var i = 0; i < this.items.length; i++) {
    // Skips the Drive root volume. For Drive entries, one of children of Drive
    // root or shortcuts should be selected.
    var item = this.items[i];
    if (item instanceof VolumeItem && item.isDrive())
      continue;

    if (util.isSameEntry(item.entry, entry)) {
      this.dontHandleChangeEvent_ = true;
      item.selectByEntry(entry);
      this.dontHandleChangeEvent_ = false;
      return true;
    }
  }
  // Otherwise, search whole tree.
  this.dontHandleChangeEvent_ = true;
  var found = DirectoryItemTreeBaseMethods.searchAndSelectByEntry.call(
      this, entry);
  this.dontHandleChangeEvent_ = false;
  return found;
};

/**
 * Decorates an element.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 * @param {VolumeManagerWrapper} volumeManager VolumeManager of the system.
 * @param {MetadataCache} metadataCache Shared MetadataCache instance.
 * @param {boolean} fakeEntriesVisible True if it should show the fakeEntries.
 */
DirectoryTree.prototype.decorate = function(
    directoryModel, volumeManager, metadataCache, fakeEntriesVisible) {
  cr.ui.Tree.prototype.decorate.call(this);

  this.sequence_ = 0;
  this.directoryModel_ = directoryModel;
  this.volumeManager_ = volumeManager;
  this.metadataCache_ = metadataCache;
  this.models_ = [];

  this.fileFilter_ = this.directoryModel_.getFileFilter();
  this.fileFilter_.addEventListener('changed',
                                    this.onFilterChanged_.bind(this));

  this.directoryModel_.addEventListener('directory-changed',
      this.onCurrentDirectoryChanged_.bind(this));

  // Add a handler for directory change.
  this.addEventListener('change', function() {
    if (this.selectedItem && !this.dontHandleChangeEvent_)
      this.selectedItem.activate();
  }.bind(this));

  this.privateOnDirectoryChangedBound_ =
      this.onDirectoryContentChanged_.bind(this);
  chrome.fileManagerPrivate.onDirectoryChanged.addListener(
      this.privateOnDirectoryChangedBound_);

  this.scrollBar_ = new MainPanelScrollBar();
  this.scrollBar_.initialize(this.parentNode, this);

  /**
   * Flag to show fake entries in the tree.
   * @type {boolean}
   * @private
   */
  this.fakeEntriesVisible_ = fakeEntriesVisible;
};

/**
 * Select the item corresponding to the given entry.
 * @param {DirectoryEntry|Object} entry The directory entry to be selected. Can
 *     be a fake.
 */
DirectoryTree.prototype.selectByEntry = function(entry) {
  if (this.selectedItem && util.isSameEntry(entry, this.selectedItem.entry))
    return;

  if (this.searchAndSelectByEntry(entry))
    return;

  this.updateSubDirectories(false /* recursive */);
  var currentSequence = ++this.sequence_;
  var volumeInfo = this.volumeManager_.getVolumeInfo(entry);
  volumeInfo.resolveDisplayRoot(function() {
    if (this.sequence_ !== currentSequence)
      return;
    if (!this.searchAndSelectByEntry(entry))
      this.selectedItem = null;
  }.bind(this));
};

/**
 * Select the volume or the shortcut corresponding to the given index.
 * @param {number} index 0-based index of the target top-level item.
 * @return {boolean} True if one of the volume items is selected.
 */
DirectoryTree.prototype.selectByIndex = function(index) {
  if (index < 0 || index >= this.items.length)
    return false;

  this.items[index].selected = true;
  return true;
};

/**
 * Retrieves the latest subdirectories and update them on the tree.
 *
 * @param {boolean} recursive True if the update is recursively.
 * @param {function()=} opt_callback Called when subdirectories are fully
 *     updated.
 */
DirectoryTree.prototype.updateSubDirectories = function(
    recursive, opt_callback) {
  this.redraw(recursive);
  if (opt_callback)
    opt_callback();
};

/**
 * Redraw the list.
 * @param {boolean} recursive True if the update is recursively. False if the
 *     only root items are updated.
 */
DirectoryTree.prototype.redraw = function(recursive) {
  this.updateSubElementsFromList(recursive);
};

/**
 * Invoked when the filter is changed.
 * @private
 */
DirectoryTree.prototype.onFilterChanged_ = function() {
  // Returns immediately, if the tree is hidden.
  if (this.hidden)
    return;

  this.redraw(true /* recursive */);
};

/**
 * Invoked when a directory is changed.
 * @param {!UIEvent} event Event.
 * @private
 */
DirectoryTree.prototype.onDirectoryContentChanged_ = function(event) {
  if (event.eventType !== 'changed')
    return;

  for (var i = 0; i < this.items.length; i++) {
    if (this.items[i] instanceof VolumeItem)
      this.items[i].updateItemByEntry(event.entry);
  }
};

/**
 * Invoked when the current directory is changed.
 * @param {!UIEvent} event Event.
 * @private
 */
DirectoryTree.prototype.onCurrentDirectoryChanged_ = function(event) {
  this.selectByEntry(event.newDirEntry);
};

/**
 * Invoked when the volume list or shortcut list is changed.
 * @private
 */
DirectoryTree.prototype.onListContentChanged_ = function() {
  this.updateSubDirectories(false, function() {
    // If no item is selected now, try to select the item corresponding to
    // current directory because the current directory might have been populated
    // in this tree in previous updateSubDirectories().
    if (!this.selectedItem) {
      var currentDir = this.directoryModel_.getCurrentDirEntry();
      if (currentDir)
        this.selectByEntry(currentDir);
    }
  }.bind(this));
};

/**
 * Updates the UI after the layout has changed.
 */
DirectoryTree.prototype.relayout = function() {
  cr.dispatchSimpleEvent(this, 'relayout');
};
