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
  this.innerHTML =
      '<div class="tree-row">' +
      ' <span class="expand-icon"></span>' +
      ' <span class="icon"></span>' +
      ' <span class="label entry-name"></span>' +
      '</div>' +
      '<div class="tree-children"></div>';

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
    entries.sort(function(a, b) {
      return (a.name.toLowerCase() > b.name.toLowerCase()) ? 1 : -1;
    });
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
  this.parentTree_.metadataCache.getOne(
      this.dirEntry_,
      'drive',
      function(metadata) {
        icon.classList.toggle('shared', metadata && metadata.shared);
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
 */
DirectoryTree.decorate = function(
    el, directoryModel, volumeManager, metadataCache) {
  el.__proto__ = DirectoryTree.prototype;
  (/** @type {DirectoryTree} */ el).decorate(
      directoryModel, volumeManager, metadataCache);
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
};

cr.defineProperty(DirectoryTree, 'contextMenuForSubitems', cr.PropertyKind.JS);

/**
 * Calls DirectoryItemTreeBaseMethods.updateSubElementsFromList().
 *
 * @param {boolean} recursive True if the all visible sub-directories are
 *     updated recursively including left arrows. If false, the update walks
 *     only immediate child directories without arrows.
 */
DirectoryTree.prototype.updateSubElementsFromList = function(recursive) {
  DirectoryItemTreeBaseMethods.updateSubElementsFromList.call(this, recursive);
};

/**
 * Calls DirectoryItemTreeBaseMethods.updateSubElementsFromList().
 *
 * @param {DirectoryEntry|Object} entry The entry to be searched for. Can be
 *     a fake.
 * @return {boolean} True if the parent item is found.
 */
DirectoryTree.prototype.searchAndSelectByEntry = function(entry) {
  return DirectoryItemTreeBaseMethods.searchAndSelectByEntry.call(this, entry);
};

/**
 * Decorates an element.
 * @param {DirectoryModel} directoryModel Current DirectoryModel.
 * @param {VolumeManagerWrapper} volumeManager VolumeManager of the system.
 * @param {MetadataCache} metadataCache Shared MetadataCache instance.
 */
DirectoryTree.prototype.decorate = function(
    directoryModel, volumeManager, metadataCache) {
  cr.ui.Tree.prototype.decorate.call(this);

  this.sequence_ = 0;
  this.directoryModel_ = directoryModel;
  this.volumeManager_ = volumeManager;
  this.metadataCache_ = metadataCache;
  this.entries_ = [];
  this.currentVolumeInfo_ = null;

  this.fileFilter_ = this.directoryModel_.getFileFilter();
  this.fileFilter_.addEventListener('changed',
                                    this.onFilterChanged_.bind(this));

  this.directoryModel_.addEventListener('directory-changed',
      this.onCurrentDirectoryChanged_.bind(this));

  // Add a handler for directory change.
  this.addEventListener('change', function() {
    if (this.selectedItem)
      this.directoryModel_.activateDirectoryEntry(this.selectedItem.entry);
  }.bind(this));

  this.privateOnDirectoryChangedBound_ =
      this.onDirectoryContentChanged_.bind(this);
  chrome.fileBrowserPrivate.onDirectoryChanged.addListener(
      this.privateOnDirectoryChangedBound_);

  this.scrollBar_ = MainPanelScrollBar();
  this.scrollBar_.initialize(this.parentNode, this);
};

/**
 * Select the item corresponding to the given entry.
 * @param {DirectoryEntry|Object} entry The directory entry to be selected. Can
 *     be a fake.
 */
DirectoryTree.prototype.selectByEntry = function(entry) {
  // If the target directory is not in the tree, do nothing.
  var locationInfo = this.volumeManager_.getLocationInfo(entry);
  if (!locationInfo || !locationInfo.isDriveBased)
    return;

  var volumeInfo = this.volumeManager_.getVolumeInfo(entry);
  if (this.selectedItem && util.isSameEntry(entry, this.selectedItem.entry))
    return;

  if (this.searchAndSelectByEntry(entry))
    return;

  this.updateSubDirectories(false /* recursive */);
  var currentSequence = ++this.sequence_;
  volumeInfo.resolveDisplayRoot(function() {
    if (this.sequence_ !== currentSequence)
      return;
    if (!this.searchAndSelectByEntry(entry))
      this.selectedItem = null;
  }.bind(this));
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
  var callback = opt_callback || function() {};
  this.entries_ = [];

  var compareEntries = function(a, b) {
    return a.toURL() < b.toURL();
  };

  // Add fakes (if any).
  for (var key in this.currentVolumeInfo_.fakeEntries) {
    this.entries_.push(this.currentVolumeInfo_.fakeEntries[key]);
  }

  // If the display root is not available yet, then redraw anyway with what
  // we have. However, concurrently try to resolve the display root and then
  // redraw.
  if (!this.currentVolumeInfo_.displayRoot) {
    this.entries_.sort(compareEntries);
    this.redraw(recursive);
  }

  this.currentVolumeInfo_.resolveDisplayRoot(function(displayRoot) {
    this.entries_.push(this.currentVolumeInfo_.displayRoot);
    this.entries_.sort(compareEntries);
    this.redraw(recursive);  // Redraw.
    callback();
  }.bind(this), callback /* Ignore errors. */);
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

  var locationInfo = this.volumeManager_.getLocationInfo(event.entry);
  if (!locationInfo || !locationInfo.isDriveBased)
    return;

  var myDriveItem = this.items[0];
  if (myDriveItem)
    myDriveItem.updateItemByEntry(event.entry);
};

/**
 * Invoked when the current directory is changed.
 * @param {!UIEvent} event Event.
 * @private
 */
DirectoryTree.prototype.onCurrentDirectoryChanged_ = function(event) {
  this.currentVolumeInfo_ =
      this.volumeManager_.getVolumeInfo(event.newDirEntry);
  this.selectByEntry(event.newDirEntry);
};

/**
 * Sets the margin height for the transparent preview panel at the bottom.
 * @param {number} margin Margin to be set in px.
 */
DirectoryTree.prototype.setBottomMarginForPanel = function(margin) {
  this.style.paddingBottom = margin + 'px';
  this.scrollBar_.setBottomMarginForPanel(margin);
};

/**
 * Updates the UI after the layout has changed.
 */
DirectoryTree.prototype.relayout = function() {
  cr.dispatchSimpleEvent(this, 'relayout');
};
