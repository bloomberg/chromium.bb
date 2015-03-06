// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provide support for drag-and-drop operations in shaped windows. The
 * standard API doesn't work because no "dragover" events are generated
 * if the mouse moves outside the window region.
 */
'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @param {Element} element The element to register for drag and drop.
 * @param {function(number, number):void} dragUpdate Callback to receive the
 *     X and Y deltas as the element is dragged.
 * @param {function():void=} opt_dragStart Initiation callback.
 * @param {function():void=} opt_dragEnd Completion callback.
 */
remoting.DragAndDrop = function(element, dragUpdate,
                                opt_dragStart, opt_dragEnd) {
  /** @private */
  this.element_ = element;

  /** @private */
  this.dragUpdate_ = dragUpdate;

  /** @private */
  this.dragStart_ = opt_dragStart;

  /** @private */
  this.dragEnd_ = opt_dragEnd;

  /** @private {number} */
  this.previousDeltaX_ = 0;

  /** @private {number} */
  this.previousDeltaY_ = 0;

  /** @type {boolean} */
  this.seenNonZeroDelta_ = false;

  /** @private {function(Event):void} */
  this.callOnMouseUp_ = this.onMouseUp_.bind(this);

  /** @private {function(Event):void} */
  this.callOnMouseMove_ = this.onMouseMove_.bind(this);

  element.addEventListener('mousedown', this.onMouseDown_.bind(this), false);
};

/**
 * @param {Event} event
 */
remoting.DragAndDrop.prototype.onMouseDown_ = function(event) {
  if (event.button != 0) {
    return;
  }
  this.previousDeltaX_ = 0;
  this.previousDeltaY_ = 0;
  this.seenNonZeroDelta_ = false;
  this.element_.addEventListener('mousemove', this.callOnMouseMove_, false);
  this.element_.addEventListener('mouseup', this.callOnMouseUp_, false);
  this.element_.requestPointerLock();
  if (this.dragStart_) {
    this.dragStart_();
  }
};

/**
 * TODO(jamiewalch): Remove the workarounds in this method once the pointer-lock
 * API is fixed (crbug.com/419562).
 *
 * @param {Event} event
 */
remoting.DragAndDrop.prototype.onMouseMove_ = function(event) {
  // Ignore the first non-zero delta. A click event will generate a bogus
  // mousemove event, even if the mouse doesn't move.
  if (!this.seenNonZeroDelta_ &&
      (event.movementX != 0 || event.movementY != 0)) {
    this.seenNonZeroDelta_ = true;
  }

  /**
   * The mouse lock API is buggy when used with shaped windows, and occasionally
   * generates single, large deltas that must be filtered out.
   *
   * @param {number} previous
   * @param {number} current
   * @return {number}
   */
  var adjustDelta = function(previous, current) {
    var THRESHOLD = 100;  // Based on observed values.
    if (Math.abs(previous < THRESHOLD) && Math.abs(current) >= THRESHOLD) {
      return 0;
    }
    return current;
  };
  this.previousDeltaX_ = adjustDelta(this.previousDeltaX_, event.movementX);
  this.previousDeltaY_ = adjustDelta(this.previousDeltaY_, event.movementY);
  if (this.previousDeltaX_ != 0 || this.previousDeltaY_ != 0) {
    this.dragUpdate_(this.previousDeltaX_, this.previousDeltaY_);
  }
};

/**
 * @param {Event} event
 */
remoting.DragAndDrop.prototype.onMouseUp_ = function(event) {
  this.element_.removeEventListener('mousemove', this.callOnMouseMove_, false);
  this.element_.removeEventListener('mouseup', this.callOnMouseUp_, false);
  document.exitPointerLock();
  if (this.dragEnd_) {
    this.dragEnd_();
  }
};
