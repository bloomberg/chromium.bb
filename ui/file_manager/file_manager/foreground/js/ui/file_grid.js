// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * FileGrid constructor.
 *
 * Represents grid for the Grid View in the File Manager.
 * @constructor
 * @extends {cr.ui.Grid}
 */

function FileGrid() {
  throw new Error('Use FileGrid.decorate');
}

/**
 * Thumbnail quality.
 * @enum {number}
 */
FileGrid.ThumbnailQuality = {
  LOW: 0,
  HIGH: 1
};

/**
 * Inherits from cr.ui.Grid.
 */
FileGrid.prototype.__proto__ = cr.ui.Grid.prototype;

/**
 * Decorates an HTML element to be a FileGrid.
 * @param {HTMLElement} self The grid to decorate.
 * @param {MetadataCache} metadataCache Metadata cache to find entries
 *                                      metadata.
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 */
FileGrid.decorate = function(self, metadataCache, volumeManager) {
  cr.ui.Grid.decorate(self);
  self.__proto__ = FileGrid.prototype;
  self.metadataCache_ = metadataCache;
  self.volumeManager_ = volumeManager;

  self.scrollBar_ = new MainPanelScrollBar();
  self.scrollBar_.initialize(self.parentNode, self);
  self.setBottomMarginForPanel(0);

  self.itemConstructor = function(entry) {
    var item = self.ownerDocument.createElement('LI');
    FileGrid.Item.decorate(item, entry, self);
    return item;
  };

  self.relayoutRateLimiter_ =
      new AsyncUtil.RateLimiter(self.relayoutImmediately_.bind(self));
};

/**
 * Updates items to reflect metadata changes.
 * @param {string} type Type of metadata changed.
 * @param {Array.<Entry>} entries Entries whose metadata changed.
 */
FileGrid.prototype.updateListItemsMetadata = function(type, entries) {
  var urls = util.entriesToURLs(entries);
  var boxes = this.querySelectorAll('.img-container');
  for (var i = 0; i < boxes.length; i++) {
    var box = boxes[i];
    var listItem = this.getListItemAncestor(box);
    var entry = listItem && this.dataModel.item(listItem.listIndex);
    if (!entry || urls.indexOf(entry.toURL()) === -1)
      continue;

    FileGrid.decorateThumbnailBox(box,
                                  entry,
                                  this.metadataCache_,
                                  this.volumeManager_,
                                  ThumbnailLoader.FillMode.FIT,
                                  FileGrid.ThumbnailQuality.LOW);
  }
};

/**
 * Redraws the UI. Skips multiple consecutive calls.
 */
FileGrid.prototype.relayout = function() {
  this.relayoutRateLimiter_.run();
};

/**
 * Redraws the UI immediately.
 * @private
 */
FileGrid.prototype.relayoutImmediately_ = function() {
  this.startBatchUpdates();
  this.columns = 0;
  this.redraw();
  this.endBatchUpdates();
  cr.dispatchSimpleEvent(this, 'relayout');
};

/**
 * Decorates thumbnail.
 * @param {HTMLElement} li List item.
 * @param {Entry} entry Entry to render a thumbnail for.
 * @param {MetadataCache} metadataCache To retrieve metadata.
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 */
FileGrid.decorateThumbnail = function(li, entry, metadataCache, volumeManager) {
  li.className = 'thumbnail-item';
  if (entry)
    filelist.decorateListItem(li, entry, metadataCache);

  var frame = li.ownerDocument.createElement('div');
  frame.className = 'thumbnail-frame';
  li.appendChild(frame);

  var box = li.ownerDocument.createElement('div');
  if (entry) {
    FileGrid.decorateThumbnailBox(box,
                                  entry,
                                  metadataCache,
                                  volumeManager,
                                  ThumbnailLoader.FillMode.AUTO,
                                  FileGrid.ThumbnailQuality.LOW);
  }
  frame.appendChild(box);

  var bottom = li.ownerDocument.createElement('div');
  bottom.className = 'thumbnail-bottom';
  bottom.appendChild(filelist.renderFileNameLabel(li.ownerDocument, entry));
  frame.appendChild(bottom);
};

/**
 * Decorates the box containing a centered thumbnail image.
 *
 * @param {HTMLDivElement} box Box to decorate.
 * @param {Entry} entry Entry which thumbnail is generating for.
 * @param {MetadataCache} metadataCache To retrieve metadata.
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 * @param {ThumbnailLoader.FillMode} fillMode Fill mode.
 * @param {FileGrid.ThumbnailQuality} quality Thumbnail quality.
 * @param {function(HTMLElement)=} opt_imageLoadCallback Callback called when
 *     the image has been loaded before inserting it into the DOM.
 */
FileGrid.decorateThumbnailBox = function(
    box, entry, metadataCache, volumeManager, fillMode, quality,
    opt_imageLoadCallback) {
  var locationInfo = volumeManager.getLocationInfo(entry);
  box.className = 'img-container';

  if (entry.isDirectory) {
    box.setAttribute('generic-thumbnail', 'folder');
    if (locationInfo && locationInfo.isDriveBased) {
      metadataCache.getOne(entry, 'external', function(metadata) {
        if (metadata.shared)
          box.classList.add('shared');
      });
    }
    if (opt_imageLoadCallback)
      setTimeout(opt_imageLoadCallback, 0, null /* callback parameter */);
    return;
  }

  var metadataTypes = 'thumbnail|filesystem|external|media';

  // Drive provides high quality thumbnails via USE_EMBEDDED, however local
  // images usually provide very tiny thumbnails, therefore USE_EMBEDDE can't
  // be used to obtain high quality output.
  var useEmbedded;
  switch (quality) {
    case FileGrid.ThumbnailQuality.LOW:
      useEmbedded = ThumbnailLoader.UseEmbedded.USE_EMBEDDED;
      break;
    case FileGrid.ThumbnailQuality.HIGH:
      // TODO(mtomasz): Use Entry instead of paths.
      useEmbedded = (locationInfo && locationInfo.isDriveBased) ?
          ThumbnailLoader.UseEmbedded.USE_EMBEDDED :
          ThumbnailLoader.UseEmbedded.NO_EMBEDDED;
      break;
  }

  metadataCache.getOne(entry, metadataTypes,
      function(metadata) {
        new ThumbnailLoader(entry,
                            ThumbnailLoader.LoaderType.IMAGE,
                            metadata,
                            undefined,  // opt_mediaType
                            useEmbedded).
            load(box,
                fillMode,
                ThumbnailLoader.OptimizationMode.DISCARD_DETACHED,
                opt_imageLoadCallback);
      });
};

/**
 * Item for the Grid View.
 * @constructor
 */
FileGrid.Item = function() {
  throw new Error();
};

/**
 * Inherits from cr.ui.ListItem.
 */
FileGrid.Item.prototype.__proto__ = cr.ui.ListItem.prototype;

Object.defineProperty(FileGrid.Item.prototype, 'label', {
  /**
   * @this {FileGrid.Item}
   * @return {string} Label of the item.
   */
  get: function() {
    return this.querySelector('filename-label').textContent;
  }
});

/**
 * @param {Element} li List item element.
 * @param {Entry} entry File entry.
 * @param {FileGrid} grid Owner.
 */
FileGrid.Item.decorate = function(li, entry, grid) {
  li.__proto__ = FileGrid.Item.prototype;
  // TODO(mtomasz): Pass the metadata cache and the volume manager directly
  // instead of accessing private members of grid.
  FileGrid.decorateThumbnail(
      li, entry, grid.metadataCache_, grid.volumeManager_, true);

  // Override the default role 'listitem' to 'option' to match the parent's
  // role (listbox).
  li.setAttribute('role', 'option');
};

/**
 * Sets the margin height for the transparent preview panel at the bottom.
 * @param {number} margin Margin to be set in px.
 */
FileGrid.prototype.setBottomMarginForPanel = function(margin) {
  // +20 bottom margin is needed to match the bottom margin size with the
  // margin between its items.
  this.style.paddingBottom = (margin + 20) + 'px';
  this.scrollBar_.setBottomMarginForPanel(margin);
};

/**
 * Obtains if the drag selection should be start or not by referring the mouse
 * event.
 * @param {MouseEvent} event Drag start event.
 * @return {boolean} True if the mouse is hit to the background of the list.
 */
FileGrid.prototype.shouldStartDragSelection = function(event) {
  var pos = DragSelector.getScrolledPosition(this, event);
  return this.getHitElements(pos.x, pos.y).length === 0;
};

/**
 * Obtains the column/row index that the coordinate points.
 * @param {number} coordinate Vertical/horizontal coordinate value that points
 *     column/row.
 * @param {number} step Length from a column/row to the next one.
 * @param {number} threshold Threshold that determines whether 1 offset is added
 *     to the return value or not. This is used in order to handle the margin of
 *     column/row.
 * @return {number} Index of hit column/row.
 * @private
 */
FileGrid.prototype.getHitIndex_ = function(coordinate, step, threshold) {
  var index = ~~(coordinate / step);
  return (coordinate % step >= threshold) ? index + 1 : index;
};

/**
 * Obtains the index list of elements that are hit by the point or the
 * rectangle.
 *
 * We should match its argument interface with FileList.getHitElements.
 *
 * @param {number} x X coordinate value.
 * @param {number} y Y coordinate value.
 * @param {number=} opt_width Width of the coordinate.
 * @param {number=} opt_height Height of the coordinate.
 * @return {Array.<number>} Index list of hit elements.
 */
FileGrid.prototype.getHitElements = function(x, y, opt_width, opt_height) {
  var currentSelection = [];
  var right = x + (opt_width || 0);
  var bottom = y + (opt_height || 0);
  var itemMetrics = this.measureItem();
  var horizontalStartIndex = this.getHitIndex_(
      x, itemMetrics.width, itemMetrics.width - itemMetrics.marginRight);
  var horizontalEndIndex = Math.min(this.columns, this.getHitIndex_(
      right, itemMetrics.width, itemMetrics.marginLeft));
  var verticalStartIndex = this.getHitIndex_(
      y, itemMetrics.height, itemMetrics.height - itemMetrics.bottom);
  var verticalEndIndex = this.getHitIndex_(
      bottom, itemMetrics.height, itemMetrics.marginTop);
  for (var verticalIndex = verticalStartIndex;
       verticalIndex < verticalEndIndex;
       verticalIndex++) {
    var indexBase = this.getFirstItemInRow(verticalIndex);
    for (var horizontalIndex = horizontalStartIndex;
         horizontalIndex < horizontalEndIndex;
         horizontalIndex++) {
      var index = indexBase + horizontalIndex;
      if (0 <= index && index < this.dataModel.length)
        currentSelection.push(index);
    }
  }
  return currentSelection;
};
