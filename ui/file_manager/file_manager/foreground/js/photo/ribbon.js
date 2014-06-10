// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Scrollable thumbnail ribbon at the bottom of the Gallery in the Slide mode.
 *
 * @param {Document} document Document.
 * @param {MetadataCache} metadataCache MetadataCache instance.
 * @param {cr.ui.ArrayDataModel} dataModel Data model.
 * @param {cr.ui.ListSelectionModel} selectionModel Selection model.
 * @return {Element} Ribbon element.
 * @constructor
 */
function Ribbon(document, metadataCache, dataModel, selectionModel) {
  var self = document.createElement('div');
  Ribbon.decorate(self, metadataCache, dataModel, selectionModel);
  return self;
}

/**
 * Inherit from HTMLDivElement.
 */
Ribbon.prototype.__proto__ = HTMLDivElement.prototype;

/**
 * Decorate a Ribbon instance.
 *
 * @param {Ribbon} self Self pointer.
 * @param {MetadataCache} metadataCache MetadataCache instance.
 * @param {cr.ui.ArrayDataModel} dataModel Data model.
 * @param {cr.ui.ListSelectionModel} selectionModel Selection model.
 */
Ribbon.decorate = function(self, metadataCache, dataModel, selectionModel) {
  self.__proto__ = Ribbon.prototype;
  self.metadataCache_ = metadataCache;
  self.dataModel_ = dataModel;
  self.selectionModel_ = selectionModel;

  self.className = 'ribbon';
};

/**
 * Max number of thumbnails in the ribbon.
 * @type {number}
 */
Ribbon.ITEMS_COUNT = 5;

/**
 * Force redraw the ribbon.
 */
Ribbon.prototype.redraw = function() {
  this.onSelection_();
};

/**
 * Clear all cached data to force full redraw on the next selection change.
 */
Ribbon.prototype.reset = function() {
  this.renderCache_ = {};
  this.firstVisibleIndex_ = 0;
  this.lastVisibleIndex_ = -1;  // Zero thumbnails
};

/**
 * Enable the ribbon.
 */
Ribbon.prototype.enable = function() {
  this.onContentBound_ = this.onContentChange_.bind(this);
  this.dataModel_.addEventListener('content', this.onContentBound_);

  this.onSpliceBound_ = this.onSplice_.bind(this);
  this.dataModel_.addEventListener('splice', this.onSpliceBound_);

  this.onSelectionBound_ = this.onSelection_.bind(this);
  this.selectionModel_.addEventListener('change', this.onSelectionBound_);

  this.reset();
  this.redraw();
};

/**
 * Disable ribbon.
 */
Ribbon.prototype.disable = function() {
  this.dataModel_.removeEventListener('content', this.onContentBound_);
  this.dataModel_.removeEventListener('splice', this.onSpliceBound_);
  this.selectionModel_.removeEventListener('change', this.onSelectionBound_);

  this.removeVanishing_();
  this.textContent = '';
};

/**
 * Data model splice handler.
 * @param {Event} event Event.
 * @private
 */
Ribbon.prototype.onSplice_ = function(event) {
  if (event.removed.length == 0)
    return;

  if (event.removed.length > 1) {
    console.error('Cannot remove multiple items');
    return;
  }

  var removed = this.renderCache_[event.removed[0].getEntry().toURL()];
  if (!removed || !removed.parentNode || !removed.hasAttribute('selected')) {
    console.error('Can only remove the selected item');
    return;
  }

  var persistentNodes = this.querySelectorAll('.ribbon-image:not([vanishing])');
  if (this.lastVisibleIndex_ < this.dataModel_.length) { // Not at the end.
    var lastNode = persistentNodes[persistentNodes.length - 1];
    if (lastNode.nextSibling) {
      // Pull back a vanishing node from the right.
      lastNode.nextSibling.removeAttribute('vanishing');
    } else {
      // Push a new item at the right end.
      this.appendChild(this.renderThumbnail_(this.lastVisibleIndex_));
    }
  } else {
    // No items to the right, move the window to the left.
    this.lastVisibleIndex_--;
    if (this.firstVisibleIndex_) {
      this.firstVisibleIndex_--;
      var firstNode = persistentNodes[0];
      if (firstNode.previousSibling) {
        // Pull back a vanishing node from the left.
        firstNode.previousSibling.removeAttribute('vanishing');
      } else {
        // Push a new item at the left end.
        var newThumbnail = this.renderThumbnail_(this.firstVisibleIndex_);
        newThumbnail.style.marginLeft = -(this.clientHeight - 2) + 'px';
        this.insertBefore(newThumbnail, this.firstChild);
        setTimeout(function() {
          newThumbnail.style.marginLeft = '0';
        }, 0);
      }
    }
  }

  removed.removeAttribute('selected');
  removed.setAttribute('vanishing', 'smooth');
  this.scheduleRemove_();
};

/**
 * Selection change handler.
 * @private
 */
Ribbon.prototype.onSelection_ = function() {
  var indexes = this.selectionModel_.selectedIndexes;
  if (indexes.length == 0)
    return;  // Ignore temporary empty selection.
  var selectedIndex = indexes[0];

  var length = this.dataModel_.length;

  // TODO(dgozman): use margin instead of 2 here.
  var itemWidth = this.clientHeight - 2;
  var fullItems = Ribbon.ITEMS_COUNT;
  fullItems = Math.min(fullItems, length);
  var right = Math.floor((fullItems - 1) / 2);

  var fullWidth = fullItems * itemWidth;
  this.style.width = fullWidth + 'px';

  var lastIndex = selectedIndex + right;
  lastIndex = Math.max(lastIndex, fullItems - 1);
  lastIndex = Math.min(lastIndex, length - 1);
  var firstIndex = lastIndex - fullItems + 1;

  if (this.firstVisibleIndex_ != firstIndex ||
      this.lastVisibleIndex_ != lastIndex) {

    if (this.lastVisibleIndex_ == -1) {
      this.firstVisibleIndex_ = firstIndex;
      this.lastVisibleIndex_ = lastIndex;
    }

    this.removeVanishing_();

    this.textContent = '';
    var startIndex = Math.min(firstIndex, this.firstVisibleIndex_);
    // All the items except the first one treated equally.
    for (var index = startIndex + 1;
         index <= Math.max(lastIndex, this.lastVisibleIndex_);
         ++index) {
      // Only add items that are in either old or the new viewport.
      if (this.lastVisibleIndex_ < index && index < firstIndex ||
          lastIndex < index && index < this.firstVisibleIndex_)
        continue;
      var box = this.renderThumbnail_(index);
      box.style.marginLeft = '0';
      this.appendChild(box);
      if (index < firstIndex || index > lastIndex) {
        // If the node is not in the new viewport we only need it while
        // the animation is playing out.
        box.setAttribute('vanishing', 'slide');
      }
    }

    var slideCount = this.childNodes.length + 1 - Ribbon.ITEMS_COUNT;
    var margin = itemWidth * slideCount;
    var startBox = this.renderThumbnail_(startIndex);
    if (startIndex == firstIndex) {
      // Sliding to the right.
      startBox.style.marginLeft = -margin + 'px';
      if (this.firstChild)
        this.insertBefore(startBox, this.firstChild);
      else
        this.appendChild(startBox);
      setTimeout(function() {
        startBox.style.marginLeft = '0';
      }, 0);
    } else {
      // Sliding to the left. Start item will become invisible and should be
      // removed afterwards.
      startBox.setAttribute('vanishing', 'slide');
      startBox.style.marginLeft = '0';
      if (this.firstChild)
        this.insertBefore(startBox, this.firstChild);
      else
        this.appendChild(startBox);
      setTimeout(function() {
        startBox.style.marginLeft = -margin + 'px';
      }, 0);
    }

    ImageUtil.setClass(this, 'fade-left',
        firstIndex > 0 && selectedIndex != firstIndex);

    ImageUtil.setClass(this, 'fade-right',
        lastIndex < length - 1 && selectedIndex != lastIndex);

    this.firstVisibleIndex_ = firstIndex;
    this.lastVisibleIndex_ = lastIndex;

    this.scheduleRemove_();
  }

  var oldSelected = this.querySelector('[selected]');
  if (oldSelected) oldSelected.removeAttribute('selected');

  var newSelected =
      this.renderCache_[this.dataModel_.item(selectedIndex).getEntry().toURL()];
  if (newSelected) newSelected.setAttribute('selected', true);
};

/**
 * Schedule the removal of thumbnails marked as vanishing.
 * @private
 */
Ribbon.prototype.scheduleRemove_ = function() {
  if (this.removeTimeout_)
    clearTimeout(this.removeTimeout_);

  this.removeTimeout_ = setTimeout(function() {
    this.removeTimeout_ = null;
    this.removeVanishing_();
  }.bind(this), 200);
};

/**
 * Remove all thumbnails marked as vanishing.
 * @private
 */
Ribbon.prototype.removeVanishing_ = function() {
  if (this.removeTimeout_) {
    clearTimeout(this.removeTimeout_);
    this.removeTimeout_ = 0;
  }
  var vanishingNodes = this.querySelectorAll('[vanishing]');
  for (var i = 0; i != vanishingNodes.length; i++) {
    vanishingNodes[i].removeAttribute('vanishing');
    this.removeChild(vanishingNodes[i]);
  }
};

/**
 * Create a DOM element for a thumbnail.
 *
 * @param {number} index Item index.
 * @return {Element} Newly created element.
 * @private
 */
Ribbon.prototype.renderThumbnail_ = function(index) {
  var item = this.dataModel_.item(index);
  var url = item.getEntry().toURL();

  var cached = this.renderCache_[url];
  if (cached) {
    var img = cached.querySelector('img');
    if (img)
      img.classList.add('cached');
    return cached;
  }

  var thumbnail = this.ownerDocument.createElement('div');
  thumbnail.className = 'ribbon-image';
  thumbnail.addEventListener('click', function() {
    var index = this.dataModel_.indexOf(item);
    this.selectionModel_.unselectAll();
    this.selectionModel_.setIndexSelected(index, true);
  }.bind(this));

  util.createChild(thumbnail, 'image-wrapper');

  this.metadataCache_.getOne(item.getEntry(), Gallery.METADATA_TYPE,
      this.setThumbnailImage_.bind(this, thumbnail, item.getEntry()));

  // TODO: Implement LRU eviction.
  // Never evict the thumbnails that are currently in the DOM because we rely
  // on this cache to find them by URL.
  this.renderCache_[url] = thumbnail;
  return thumbnail;
};

/**
 * Set the thumbnail image.
 *
 * @param {Element} thumbnail Thumbnail element.
 * @param {FileEntry} entry Image Entry.
 * @param {Object} metadata Metadata.
 * @private
 */
Ribbon.prototype.setThumbnailImage_ = function(thumbnail, entry, metadata) {
  new ThumbnailLoader(entry, ThumbnailLoader.LoaderType.IMAGE, metadata).load(
      thumbnail.querySelector('.image-wrapper'),
      ThumbnailLoader.FillMode.FILL /* fill */,
      ThumbnailLoader.OptimizationMode.NEVER_DISCARD);
};

/**
 * Content change handler.
 *
 * @param {Event} event Event.
 * @private
 */
Ribbon.prototype.onContentChange_ = function(event) {
  var url = event.item.getEntry().toURL();
  if (event.oldEntry.toURL() !== url) {
    this.remapCache_(event.oldEntry.toURL(), url);
  } else {
    delete this.renderCache_[url];
    var index = this.dataModel_.indexOf(event.item);
    this.renderThumbnail_(index);
  }

  var thumbnail = this.renderCache_[url];
  if (thumbnail && event.metadata)
    this.setThumbnailImage_(thumbnail, event.item.getEntry(), event.metadata);
};

/**
 * Update the thumbnail element cache.
 *
 * @param {string} oldUrl Old url.
 * @param {string} newUrl New url.
 * @private
 */
Ribbon.prototype.remapCache_ = function(oldUrl, newUrl) {
  if (oldUrl != newUrl && (oldUrl in this.renderCache_)) {
    this.renderCache_[newUrl] = this.renderCache_[oldUrl];
    delete this.renderCache_[oldUrl];
  }
};
