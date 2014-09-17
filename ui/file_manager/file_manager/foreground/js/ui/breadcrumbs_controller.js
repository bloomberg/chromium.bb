// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @extends {cr.EventTarget}
 * @param {HTMLDivElement} div Div container for breadcrumbs.
 * @param {MetadataCache} metadataCache To retrieve metadata.
 * @param {VolumeManagerWrapper} volumeManager Volume manager.
 * @constructor
 */
function BreadcrumbsController(div, metadataCache, volumeManager) {
  this.bc_ = div;
  this.metadataCache_ = metadataCache;
  this.volumeManager_ = volumeManager;
  this.entry_ = null;

  /**
   * Sequence value to skip requests that are out of date.
   * @type {number}
   * @private
   */
  this.showSequence_ = 0;

  // Register events and seql the object.
  div.addEventListener('click', this.onClick_.bind(this));
}

/**
 * Extends cr.EventTarget.
 */
BreadcrumbsController.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Shows breadcrumbs.
 *
 * @param {Entry} entry Target entry.
 */
BreadcrumbsController.prototype.show = function(entry) {
  if (entry === this.entry_)
    return;

  this.entry_ = entry;
  this.showSequence_++;

  var queue = new AsyncUtil.Queue();
  var entries = [];
  var error = false;

  // Obtain entries from the target entry to the root.
  var resolveParent = function(currentEntry, previousEntry, callback) {
    var entryLocationInfo = this.volumeManager_.getLocationInfo(currentEntry);
    if (!entryLocationInfo) {
      error = true;
      callback();
      return;
    }

    if (entryLocationInfo.isRootEntry &&
        entryLocationInfo.rootType ===
            VolumeManagerCommon.RootType.DRIVE_OTHER) {
      this.metadataCache_.getOne(previousEntry, 'external', function(result) {
        if (result && result.sharedWithMe) {
          // Adds the shared-with-me entry instead.
          var driveVolumeInfo = entryLocationInfo.volumeInfo;
          var sharedWithMeEntry =
              driveVolumeInfo.fakeEntries[
                  VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME];
          if (sharedWithMeEntry)
            entries.unshift(sharedWithMeEntry);
          else
            error = true;
        } else {
          entries.unshift(currentEntry);
        }
        // Finishes traversal since the current is root.
        callback();
      });
      return;
    }

    entries.unshift(currentEntry);
    if (!entryLocationInfo.isRootEntry) {
      currentEntry.getParent(function(parentEntry) {
        resolveParent(parentEntry, currentEntry, callback);
      }.bind(this), function() {
        error = true;
        callback();
      });
    } else {
      callback();
    }
  }.bind(this);

  queue.run(resolveParent.bind(this, entry, null));

  queue.run(function(callback) {
    // If an error occurred, just skip.
    if (error) {
      callback();
      return;
    }

    // If the path is not under the drive other root, it is not needed to
    // override root type.
    var locationInfo = this.volumeManager_.getLocationInfo(entry);
    if (!locationInfo)
      error = true;

    callback();
  }.bind(this));

  // Update DOM element.
  queue.run(function(sequence, callback) {
    // Check the sequence number to skip requests that are out of date.
    if (this.showSequence_ === sequence) {
      this.bc_.hidden = false;
      this.bc_.textContent = '';
      if (!error)
        this.updateInternal_(entries);
    }
    callback();
  }.bind(this, this.showSequence_));
};

/**
 * Updates the breadcrumb display.
 * @param {Array.<Entry>} entries Entries on the target path.
 * @private
 */
BreadcrumbsController.prototype.updateInternal_ = function(entries) {
  // Make elements.
  var doc = this.bc_.ownerDocument;
  for (var i = 0; i < entries.length; i++) {
    // Add a component.
    var entry = entries[i];
    var div = doc.createElement('div');
    div.className = 'breadcrumb-path entry-name';
    div.textContent = util.getEntryLabel(this.volumeManager_, entry);
    div.entry = entry;
    this.bc_.appendChild(div);

    // If this is the last component, break here.
    if (i === entries.length - 1) {
      div.classList.add('breadcrumb-last');
      break;
    }

    // Add a separator.
    var separator = doc.createElement('div');
    separator.className = 'separator';
    this.bc_.appendChild(separator);
  }

  this.truncate();
};

/**
 * Updates breadcrumbs widths in order to truncate it properly.
 */
BreadcrumbsController.prototype.truncate = function() {
  if (!this.bc_.firstChild)
    return;

  // Assume style.width == clientWidth (items have no margins or paddings).

  for (var item = this.bc_.firstChild; item; item = item.nextSibling) {
    item.removeAttribute('style');
    item.removeAttribute('collapsed');
  }

  var containerWidth = this.bc_.clientWidth;

  var pathWidth = 0;
  var currentWidth = 0;
  var lastSeparator;
  for (var item = this.bc_.firstChild; item; item = item.nextSibling) {
    if (item.className == 'separator') {
      pathWidth += currentWidth;
      currentWidth = item.clientWidth;
      lastSeparator = item;
    } else {
      currentWidth += item.clientWidth;
    }
  }
  if (pathWidth + currentWidth <= containerWidth)
    return;
  if (!lastSeparator) {
    this.bc_.lastChild.style.width =
        Math.min(currentWidth, containerWidth) + 'px';
    return;
  }
  var lastCrumbSeparatorWidth = lastSeparator.clientWidth;
  // Current directory name may occupy up to 70% of space or even more if the
  // path is short.
  var maxPathWidth = Math.max(Math.round(containerWidth * 0.3),
                              containerWidth - currentWidth);
  maxPathWidth = Math.min(pathWidth, maxPathWidth);

  var parentCrumb = lastSeparator.previousSibling;
  var collapsedWidth = 0;
  if (parentCrumb && pathWidth - maxPathWidth > parentCrumb.clientWidth) {
    // At least one crumb is hidden completely (or almost completely).
    // Show sign of hidden crumbs like this:
    // root > some di... > ... > current directory.
    parentCrumb.setAttribute('collapsed', '');
    collapsedWidth = Math.min(maxPathWidth, parentCrumb.clientWidth);
    maxPathWidth -= collapsedWidth;
    if (parentCrumb.clientWidth != collapsedWidth)
      parentCrumb.style.width = collapsedWidth + 'px';

    lastSeparator = parentCrumb.previousSibling;
    if (!lastSeparator)
      return;
    collapsedWidth += lastSeparator.clientWidth;
    maxPathWidth = Math.max(0, maxPathWidth - lastSeparator.clientWidth);
  }

  pathWidth = 0;
  for (var item = this.bc_.firstChild; item != lastSeparator;
       item = item.nextSibling) {
    // TODO(serya): Mixing access item.clientWidth and modifying style and
    // attributes could cause multiple layout reflows.
    if (pathWidth + item.clientWidth <= maxPathWidth) {
      pathWidth += item.clientWidth;
    } else if (pathWidth == maxPathWidth) {
      item.style.width = '0';
    } else if (item.classList.contains('separator')) {
      // Do not truncate separator. Instead let the last crumb be longer.
      item.style.width = '0';
      maxPathWidth = pathWidth;
    } else {
      // Truncate the last visible crumb.
      item.style.width = (maxPathWidth - pathWidth) + 'px';
      pathWidth = maxPathWidth;
    }
  }

  currentWidth = Math.min(currentWidth,
                          containerWidth - pathWidth - collapsedWidth);
  this.bc_.lastChild.style.width =
      (currentWidth - lastCrumbSeparatorWidth) + 'px';
};

/**
 * Hide breadcrumbs div.
 */
BreadcrumbsController.prototype.hide = function() {
  this.bc_.hidden = true;
};

/**
 * Handle a click event on a breadcrumb element.
 * @param {Event} event The click event.
 * @private
 */
BreadcrumbsController.prototype.onClick_ = function(event) {
  if (!event.target.classList.contains('breadcrumb-path') ||
      event.target.classList.contains('breadcrumb-last'))
    return;

  var newEvent = new Event('pathclick');
  newEvent.entry = event.target.entry;
  this.dispatchEvent(newEvent);
};
