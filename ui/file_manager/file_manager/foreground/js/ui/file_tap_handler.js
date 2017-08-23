// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Processes touch events and calls back upon tap, longpress and longtap.
 * This class is similar to cr.ui.TouchHandler. The major difference is that,
 * the user of this class can choose to either handle the event as a tap
 * distincted from mouse clicks, or leave it handled by the mouse event
 * handlers by default.
 *
 * @constructor
 */
function FileTapHandler() {
  /**
   * Whether the pointer is currently down and at the same place as the initial
   * position.
   * @type {boolean}
   * @private
   */
  this.tapStarted_ = false;

  /**
   * @type {boolean}
   * @private
   */
  this.isLongTap_ = false;

  /**
   * @type {boolean}
   * @private
   */
  this.hasLongPressProcessed_ = false;

  /**
   * @type {number}
   * @private
   */
  this.longTapDetectorTimerId_ = -1;

  /**
   * The absolute sum of all touch y deltas.
   * @type {number}
   * @private
   */
  this.totalMoveY_ = 0;

  /**
   * The absolute sum of all touch x deltas.
   * @type {number}
   * @private
   */
  this.totalMoveX_ = 0;

  /**
   * If defined, the identifer of the single touch that is active.  Note that
   * 0 is a valid touch identifier - it should not be treated equivalently to
   * undefined.
   * @type {number|undefined}
   * @private
   */
  this.activeTouch_ = undefined;
}

/**
 * The minimum duration of a tap to be recognized as long press and long tap.
 * This should be consistent with the Views of Android.
 * https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/view/ViewConfiguration.java
 * Also this should also be consistent with Chrome's behavior for issuing
 * drag-and-drop events by touchscreen.
 * @type {number}
 * @const
 */
FileTapHandler.LONG_PRESS_THRESHOLD_MILLISECONDS = 500;

/**
 * Maximum movement of touch required to be considered a tap.
 * @type {number}
 * @private
 */
FileTapHandler.MAX_TRACKING_FOR_TAP_ = 8;

/**
 * @enum {string}
 * @const
 */
FileTapHandler.TapEvent = {
  TAP: 'tap',
  LONG_PRESS: 'longpress',
  LONG_TAP: 'longtap'
};

/**
 * Handles touch events.
 * The propagation of the |event| will be cancelled if the |callback| takes any
 * action, so as to avoid receiving mouse click events for the tapping and
 * processing them duplicatedly.
 * @param {!Event} event a touch event.
 * @param {number} index of the target item in the file list.
 * @param {function(!Event, number, !FileTapHandler.TapEvent)} callback called
 *     when a tap event is detected. Should return ture if it has taken any
 *     action, and false if it ignroes the event.
 * @return {boolean} true if a tap or longtap event was detected and the
 *     callback processed it. False otherwise.
 */
FileTapHandler.prototype.handleTouchEvents = function(event, index, callback) {
  switch (event.type) {
    case 'touchstart':
      // Only process single touches.  If there is already a touch happening, or
      // two simultaneous touches then just ignore them.
      if (event.touches.length > 1) {
        // Note that we could cancel an active touch here.  That would make
        // simultaneous touch behave similar to near-simultaneous. However, if
        // the user is dragging something, an accidental second touch could be
        // quite disruptive if it cancelled their drag.  Better to just ignore
        // it.

        // Invalidate current touch to distinguish it from normal tap.
        this.tapStarted_ = false;
        return false;
      }

      // It's still possible there could be an active "touch" if the user is
      // simultaneously using a mouse and a touch input.
      // TODO(yamaguchi): add this after adding handler for touchcancel that
      // can reset this.activeTouch_ to undefined.
      // if (this.activeTouch_ !== undefined)
      //   return;
      var touch = event.targetTouches[0];
      this.activeTouch_ = touch.identifier;
      this.startTouchX_ = this.lastTouchX_ = touch.clientX;
      this.startTouchY_ = this.lastTouchY_ = touch.clientY;

      this.tapStarted_ = true;
      this.isLongTap_ = false;
      this.hasLongPressProcessed_ = false;
      this.longTapDetectorTimerId_ = setTimeout(function() {
        this.longTapDetectorTimerId_ = -1;
        if (!this.tapStarted_)
          return;
        this.isLongTap_ = true;
        if (callback(event, index, FileTapHandler.TapEvent.LONG_PRESS)) {
          this.hasLongPressProcessed_ = true;
        }
      }.bind(this), FileTapHandler.LONG_PRESS_THRESHOLD_MILLISECONDS);
      break;

    case 'touchmove':
      var touch = this.findActiveTouch_(event.changedTouches);
      if (!touch)
        break;

      var clientX = touch.clientX;
      var clientY = touch.clientY;

      var moveX = this.lastTouchX_ - clientX;
      var moveY = this.lastTouchY_ - clientY;
      this.totalMoveX_ += Math.abs(moveX);
      this.totalMoveY_ += Math.abs(moveY);
      this.lastTouchX_ = clientX;
      this.lastTouchY_ = clientY;

      var couldBeTap =
          this.totalMoveY_ <= FileTapHandler.MAX_TRACKING_FOR_TAP_ ||
          this.totalMoveX_ <= FileTapHandler.MAX_TRACKING_FOR_TAP_;

      if (!couldBeTap)
        // If the pointer is slided, it is a drag. It is no longer a tap.
        this.tapStarted_ = false;
      this.lastMoveX_ = moveX;
      this.lastMoveY_ = moveY;
      break;

    case 'touchend':
      // Mark as no longer being touched
      this.activeTouch_ = undefined;
      if (this.longTapDetectorTimerId_ != -1) {
        clearTimeout(this.longTapDetectorTimerId_);
        this.longTapDetectorTimerId_ = -1;
      }
      if (!this.tapStarted_)
        break;
      if (this.isLongTap_) {
        if (this.hasLongPressProcessed_ ||
            callback(event, index, FileTapHandler.TapEvent.LONG_TAP)) {
          event.preventDefault();
          return true;
        }
      } else {
        if (callback(event, index, FileTapHandler.TapEvent.TAP)) {
          event.preventDefault();
          return true;
        }
      }
      break;
  }
  return false;
};

/**
 * Given a list of Touches, find the one matching our activeTouch
 * identifier. Note that Chrome currently always uses 0 as the identifier.
 * In that case we'll end up always choosing the first element in the list.
 * @param {TouchList} touches The list of Touch objects to search.
 * @return {!Touch|undefined} The touch matching our active ID if any.
 * @private
 */
FileTapHandler.prototype.findActiveTouch_ = function(touches) {
  assert(this.activeTouch_ !== undefined, 'Expecting an active touch');
  // A TouchList isn't actually an array, so we shouldn't use
  // Array.prototype.filter/some, etc.
  for (var i = 0; i < touches.length; i++) {
    if (touches[i].identifier == this.activeTouch_)
      return touches[i];
  }
  return undefined;
};