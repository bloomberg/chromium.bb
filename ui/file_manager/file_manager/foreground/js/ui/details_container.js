// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!HTMLElement} element
 * @param {!SingleFileDetailsPanel} singlePanel
 * @param {!Element} splitter
 * @param {!Element} button
 * @param {!FilesToggleRipple} toggleRipple
 * @constructor
 * @struct
 */
function DetailsContainer(element, singlePanel, splitter, button,
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
   * @type {boolean}
   */
  this.visible = false;
  this.setVisibility(false);
}

DetailsContainer.prototype.onFileSelectionChanged = function(event) {
  var entries = event.target.selection.entries;
  if (entries.length === 0) {
    this.singlePanel_.removeAttribute('activated');
    this.singlePanel_.classList.toggle('activated', false);
    // TODO(ryoh): make a panel for empty selection
  } else if (entries.length === 1) {
    this.singlePanel_.setAttribute('activated', '');
    this.singlePanel_.onFileSelectionChanged(entries[0]);
  } else {
    // TODO(ryoh): make a panel for multiple selection
  }
};

/**
 * Sets the details panel visibility
 * @param {boolean} visibility True if the details panel is visible.
 */
DetailsContainer.prototype.setVisibility = function(visibility) {
  if (visibility) {
    this.splitter_.setAttribute('activated', '');
    this.element_.setAttribute('activated', '');
  } else {
    this.splitter_.removeAttribute('activated');
    this.element_.removeAttribute('activated');
  }
  this.visible = visibility;
  this.toggleRipple_.activated = visibility;
};
