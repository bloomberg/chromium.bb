// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Base item of NavigationListModel. Should not be created directly.
 * @param {string} label Label.
 * @constructor
 */
function NavigationModelItem(label) {
  this.label_ = label;
}

NavigationModelItem.prototype = {
  get label() { return this.label_; }
};

/**
 * Check whether given two model items are same.
 * @param {NavigationModelItem} item1 The first item to be compared.
 * @param {NavigationModelItem} item2 The second item to be compared.
 * @return {boolean} True if given two model items are same.
 */
NavigationModelItem.isSame = function(item1, item2) {
  if (item1.isVolume != item2.isVolume)
    return false;

  if (item1.isVolume)
    return item1.volumeInfo === item2.volumeInfo;
  else
    return util.isSameEntry(item1.entry, item2.entry);
};

/**
 * Item of NavigationListModel for shortcuts.
 *
 * @param {string} label Label.
 * @param {!DirectoryEntry} entry Entry. Cannot be null.
 * @constructor
 * @extends {NavigationModelItem}
 */
function NavigationModelShortcutItem(label, entry) {
  NavigationModelItem.call(this, label);
  this.entry_ = entry;
  Object.freeze(this);
}

NavigationModelShortcutItem.prototype = {
  __proto__: NavigationModelItem.prototype,
  get entry() { return this.entry_; },
  get isVolume() { return false; },
  get isShortcut() { return true; }
};

/**
 * Item of NavigationListModel for volumes.
 *
 * @param {string} label Label.
 * @param {!VolumeInfo} volumeInfo Volume info for the volume. Cannot be null.
 * @constructor
 * @extends {NavigationModelItem}
 */
function NavigationModelVolumeItem(label, volumeInfo) {
  NavigationModelItem.call(this, label);
  this.volumeInfo_ = volumeInfo;
  // Start resolving the display root because it is used
  // for determining executability of commands.
  this.volumeInfo_.resolveDisplayRoot(
      function() {}, function() {});
  Object.freeze(this);
}

NavigationModelVolumeItem.prototype = {
  __proto__: NavigationModelItem.prototype,
  get volumeInfo() { return this.volumeInfo_; },
  get isVolume() { return true; },
  get isShortcut() { return false; }
};

/**
 * A navigation list model. This model combines the 2 lists.
 * @param {VolumeManagerWrapper} volumeManager VolumeManagerWrapper instance.
 * @param {cr.ui.ArrayDataModel} shortcutListModel The list of folder shortcut.
 * @constructor
 * @extends {cr.EventTarget}
 */
function NavigationListModel(volumeManager, shortcutListModel) {
  cr.EventTarget.call(this);

  this.volumeManager_ = volumeManager;
  this.shortcutListModel_ = shortcutListModel;

  var volumeInfoToModelItem = function(volumeInfo) {
    return new NavigationModelVolumeItem(
        volumeInfo.label,
        volumeInfo);
  }.bind(this);

  var entryToModelItem = function(entry) {
    var item = new NavigationModelShortcutItem(
        entry.name,
        entry);
    return item;
  }.bind(this);

  /**
   * Type of updated list.
   * @enum {number}
   * @const
   */
  var ListType = {
    VOLUME_LIST: 1,
    SHORTCUT_LIST: 2
  };
  Object.freeze(ListType);

  // Generates this.volumeList_ and this.shortcutList_ from the models.
  this.volumeList_ =
      this.volumeManager_.volumeInfoList.slice().map(volumeInfoToModelItem);

  this.shortcutList_ = [];
  for (var i = 0; i < this.shortcutListModel_.length; i++) {
    var shortcutEntry = this.shortcutListModel_.item(i);
    var volumeInfo = this.volumeManager_.getVolumeInfo(shortcutEntry);
    this.shortcutList_.push(entryToModelItem(shortcutEntry));
  }

  // Generates a combined 'permuted' event from an event of either list.
  var permutedHandler = function(listType, event) {
    var permutation;

    // Build the volumeList.
    if (listType == ListType.VOLUME_LIST) {
      // The volume is mounted or unmounted.
      var newList = [];

      // Use the old instances if they just move.
      for (var i = 0; i < event.permutation.length; i++) {
        if (event.permutation[i] >= 0)
          newList[event.permutation[i]] = this.volumeList_[i];
      }

      // Create missing instances.
      for (var i = 0; i < event.newLength; i++) {
        if (!newList[i]) {
          newList[i] = volumeInfoToModelItem(
              this.volumeManager_.volumeInfoList.item(i));
        }
      }
      this.volumeList_ = newList;

      permutation = event.permutation.slice();

      // shortcutList part has not been changed, so the permutation should be
      // just identity mapping with a shift.
      for (var i = 0; i < this.shortcutList_.length; i++) {
        permutation.push(i + this.volumeList_.length);
      }
    } else {
      // Build the shortcutList.

      // volumeList part has not been changed, so the permutation should be
      // identity mapping.

      permutation = [];
      for (var i = 0; i < this.volumeList_.length; i++) {
        permutation[i] = i;
      }

      var modelIndex = 0;
      var oldListIndex = 0;
      var newList = [];
      while (modelIndex < this.shortcutListModel_.length &&
             oldListIndex < this.shortcutList_.length) {
        var shortcutEntry = this.shortcutListModel_.item(modelIndex);
        var cmp = this.shortcutListModel_.compare(
            shortcutEntry, this.shortcutList_[oldListIndex].entry);
        if (cmp > 0) {
          // The shortcut at shortcutList_[oldListIndex] is removed.
          permutation.push(-1);
          oldListIndex++;
          continue;
        }

        if (cmp === 0) {
          // Reuse the old instance.
          permutation.push(newList.length + this.volumeList_.length);
          newList.push(this.shortcutList_[oldListIndex]);
          oldListIndex++;
        } else {
          // We needs to create a new instance for the shortcut entry.
          newList.push(entryToModelItem(shortcutEntry));
        }
        modelIndex++;
      }

      // Add remaining (new) shortcuts if necessary.
      for (; modelIndex < this.shortcutListModel_.length; modelIndex++) {
        var shortcutEntry = this.shortcutListModel_.item(modelIndex);
        newList.push(entryToModelItem(shortcutEntry));
      }

      // Fill remaining permutation if necessary.
      for (; oldListIndex < this.shortcutList_.length; oldListIndex++)
        permutation.push(-1);

      this.shortcutList_ = newList;
    }

    // Dispatch permuted event.
    var permutedEvent = new Event('permuted');
    permutedEvent.newLength =
        this.volumeList_.length + this.shortcutList_.length;
    permutedEvent.permutation = permutation;
    this.dispatchEvent(permutedEvent);
  };

  this.volumeManager_.volumeInfoList.addEventListener(
      'permuted', permutedHandler.bind(this, ListType.VOLUME_LIST));
  this.shortcutListModel_.addEventListener(
      'permuted', permutedHandler.bind(this, ListType.SHORTCUT_LIST));

  // 'change' event is just ignored, because it is not fired neither in
  // the folder shortcut list nor in the volume info list.
  // 'splice' and 'sorted' events are not implemented, since they are not used
  // in list.js.
}

/**
 * NavigationList inherits cr.EventTarget.
 */
NavigationListModel.prototype = {
  __proto__: cr.EventTarget.prototype,
  get length() { return this.length_(); },
  get folderShortcutList() { return this.shortcutList_; }
};

/**
 * Returns the item at the given index.
 * @param {number} index The index of the entry to get.
 * @return {NavigationModelItem} The item at the given index.
 */
NavigationListModel.prototype.item = function(index) {
  var offset = this.volumeList_.length;
  if (index < offset)
    return this.volumeList_[index];
  return this.shortcutList_[index - offset];
};

/**
 * Returns the number of items in the model.
 * @return {number} The length of the model.
 * @private
 */
NavigationListModel.prototype.length_ = function() {
  return this.volumeList_.length + this.shortcutList_.length;
};

/**
 * Returns the first matching item.
 * @param {NavigationModelItem} modelItem The entry to find.
 * @param {number=} opt_fromIndex If provided, then the searching start at
 *     the {@code opt_fromIndex}.
 * @return {number} The index of the first found element or -1 if not found.
 */
NavigationListModel.prototype.indexOf = function(modelItem, opt_fromIndex) {
  for (var i = opt_fromIndex || 0; i < this.length; i++) {
    if (modelItem === this.item(i))
      return i;
  }
  return -1;
};

/**
 * Called externally when one of the items is not found on the filesystem.
 * @param {NavigationModelItem} modelItem The entry which is not found.
 */
NavigationListModel.prototype.onItemNotFoundError = function(modelItem) {
  if (modelItem.isVolume) {
    // TODO(mtomasz, yoshiki): Implement when needed.
    return;
  }
  if (modelItem.isShortcut) {
    // For shortcuts, lets the shortcut model handle this situation.
    this.shortcutListModel_.onItemNotFoundError(modelItem.entry);
  }
};
