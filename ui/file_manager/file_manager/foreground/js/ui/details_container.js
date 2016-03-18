// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!HTMLElement} element
 * @param {!SingleFileDetailsPanel} singlePanel
 * @param {!MultiFileDetailsPanel} multiPanel
 * @param {!Element} splitter
 * @param {!Element} button
 * @param {!FilesToggleRipple} toggleRipple
 * @constructor
 * @struct
 */
function DetailsContainer(element, singlePanel, multiPanel, splitter, button,
    toggleRipple) {
  /**
   * Container element.
   * @private {!HTMLElement}
   * @const
   */
  this.element_ = element;
  /**
   * Splitter element between the file list and the details panel.
   * @private {!Element}
   * @const
   */
  this.splitter_ = splitter;
  /**
   * "View details" button element.
   * @private {!Element}
   * @const
   */
  this.button_ = button;
  /**
   * Ripple element of "View details" button.
   * @private {!Element}
   * @const
   */
  this.toggleRipple_ = toggleRipple;
  /**
   * Details panel for a single file.
   * @private {!SingleFileDetailsPanel}
   * @const
   */
  this.singlePanel_ = singlePanel;
  /**
   * Details panel for a multiple files.
   * @private {!MultiFileDetailsPanel}
   * @const
   */
  this.multiPanel_ = multiPanel;
  /**
   * @type {boolean}
   */
  this.visible = false;
  /**
   * @private {Array<!FileEntry>}
   */
  this.pendingEntries_ = null;
  this.setVisibility(false);
}

DetailsContainer.prototype.onFileSelectionChanged = function(event) {
  var entries = event.target.selection.entries;
  if (this.visible) {
    this.pendingEntries_ = null;
    this.display_(entries);
  } else {
    this.pendingEntries_ = entries;
  }
};

/**
 * Disply details of entries
 * @param {!Array<!FileEntry>} entries
 */
DetailsContainer.prototype.display_ = function(entries) {
  if (entries.length === 0) {
    this.singlePanel_.removeAttribute('activated');
    this.multiPanel_.removeAttribute('activated');
    // TODO(ryoh): make a panel for empty selection
  } else if (entries.length === 1) {
    this.singlePanel_.setAttribute('activated', '');
    this.multiPanel_.removeAttribute('activated');
    this.singlePanel_.onFileSelectionChanged(entries[0]);
    this.multiPanel_.cancelLoading();
  } else {
    this.singlePanel_.removeAttribute('activated');
    this.multiPanel_.setAttribute('activated', '');
    this.multiPanel_.onFileSelectionChanged(entries);
    this.singlePanel_.cancelLoading();
  }
};

/**
 * Sets the details panel visibility
 * @param {boolean} visibility True if the details panel is visible.
 */
DetailsContainer.prototype.setVisibility = function(visibility) {
  this.visible = visibility;
  if (visibility) {
    this.splitter_.setAttribute('activated', '');
    this.element_.setAttribute('activated', '');
    if (this.pendingEntries_) {
      this.display_(this.pendingEntries_);
    }
  } else {
    this.splitter_.removeAttribute('activated');
    this.element_.removeAttribute('activated');
  }
  this.toggleRipple_.activated = visibility;
  this.singlePanel_.onVisibilityChanged(visibility);
};

/**
 * Sets date and time format.
 * @param {boolean} use12hourClock True if 12 hours clock, False if 24 hours.
 */
DetailsContainer.prototype.setDateTimeFormat = function(use12hourClock) {
  this.singlePanel_.setDateTimeFormat(use12hourClock);
};
