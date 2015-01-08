// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {PreviewPanelModel.VisibilityType} initialVisibilityType
 * @param {!Array<!cr.ui.Command>} autoVisibilityCommands
 * @extends {cr.EventTarget}
 * @constructor
 * @struct
 * @suppress {checkStructDictInheritance}
 */
function PreviewPanelModel(initialVisibilityType, autoVisibilityCommands) {
  cr.EventTarget.call(this);

  /**
   * @type {!PreviewPanelModel.VisibilityType}
   * @private
   */
  this.visibilityType_ = initialVisibilityType;

  /**
   * @type {!Array<!cr.ui.Command>}
   * @const
   * @private
   */
  this.autoVisibilityCommands_ = autoVisibilityCommands;

  /**
   * FileSelection to be displayed.
   * @type {FileSelection}
   * @private
   */
  this.selection_ = /** @type {FileSelection} */
      ({entries: [], computeBytes: function() {}, totalCount: 0});

  /**
   * Visibility type of the preview panel.
   * @type {boolean}
   * @private
   */
  this.visible = false;

  for (var i = 0; i < this.autoVisibilityCommands_.length; i++) {
    this.autoVisibilityCommands_[i].addEventListener(
        'disabledChange', this.updateVisibility_.bind(this));
  }

  this.updateVisibility_();
}

PreviewPanelModel.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Visibility type of the preview panel.
 * @enum {string}
 * @const
 */
PreviewPanelModel.VisibilityType = {
  // Preview panel always shows.
  ALWAYS_VISIBLE: 'alwaysVisible',
  // Preview panel shows when the selection property are set.
  AUTO: 'auto',
  // Preview panel does not show.
  ALWAYS_HIDDEN: 'alwaysHidden'
};

/**
 * @enum {string}
 */
PreviewPanelModel.EventType = {
  CHANGE: 'change'
};

PreviewPanelModel.prototype.setSelection = function(selection) {
  this.selection_ = selection;
  this.updateVisibility_();
};

PreviewPanelModel.prototype.setVisibilityType = function(type) {
  this.visibilityType_ = type;
  this.updateVisibility_();
};

PreviewPanelModel.prototype.updateVisibility_ = function() {
  // Get the new visibility value.
  var newVisible = true;
  switch (this.visibilityType_) {
    case PreviewPanelModel.VisibilityType.ALWAYS_VISIBLE:
      newVisible = true;
      break;
    case PreviewPanelModel.VisibilityType.AUTO:
      newVisible =
          this.selection_.entries.length !== 0 ||
          this.autoVisibilityCommands_.some(function(command) {
            return !command.disabled;
          });
      break;
    case PreviewPanelModel.VisibilityType.ALWAYS_HIDDEN:
      newVisible = false;
      break;
    default:
      assertNotReached();
  }

  // If the visibility has been already the new value, just return.
  if (this.visible === newVisible)
    return;

  // Set the new visibility value.
  this.visible = newVisible;
  cr.dispatchSimpleEvent(this, PreviewPanelModel.EventType.CHANGE);
};
