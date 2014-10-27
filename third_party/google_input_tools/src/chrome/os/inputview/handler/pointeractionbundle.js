// Copyright 2014 The ChromeOS IME Authors. All Rights Reserved.
// limitations under the License.
// See the License for the specific language governing permissions and
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// distributed under the License is distributed on an "AS-IS" BASIS,
// Unless required by applicable law or agreed to in writing, software
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// You may obtain a copy of the License at
// you may not use this file except in compliance with the License.
// Licensed under the Apache License, Version 2.0 (the "License");
//
goog.provide('i18n.input.chrome.inputview.handler.PointerActionBundle');

goog.require('goog.Timer');
goog.require('goog.dom');
goog.require('goog.events.EventTarget');
goog.require('goog.events.EventType');
goog.require('goog.math.Coordinate');
goog.require('i18n.input.chrome.inputview.SwipeDirection');
goog.require('i18n.input.chrome.inputview.events.DragEvent');
goog.require('i18n.input.chrome.inputview.events.EventType');
goog.require('i18n.input.chrome.inputview.events.PointerEvent');
goog.require('i18n.input.chrome.inputview.events.SwipeEvent');
goog.require('i18n.input.chrome.inputview.handler.SwipeState');



goog.scope(function() {



/**
 * The handler for long press.
 *
 * @param {!Node} target The target for this pointer event.
 * @param {goog.events.EventTarget=} opt_parentEventTarget The parent event
 *     target.
 * @constructor
 * @extends {goog.events.EventTarget}
 */
i18n.input.chrome.inputview.handler.PointerActionBundle = function(target,
    opt_parentEventTarget) {
  goog.base(this);
  this.setParentEventTarget(opt_parentEventTarget || null);

  /**
   * The target.
   *
   * @type {i18n.input.chrome.inputview.elements.Element}
   * @private
   */
  this.view_ = this.getView_(target);

  /**
   * The swipe offset.
   *
   * @type {!i18n.input.chrome.inputview.handler.SwipeState}
   * @private
   */
  this.swipeState_ = new i18n.input.chrome.inputview.handler.SwipeState();
};
goog.inherits(i18n.input.chrome.inputview.handler.PointerActionBundle,
    goog.events.EventTarget);
var PointerActionBundle = i18n.input.chrome.inputview.handler.
    PointerActionBundle;


/**
 * The current target after the touch point is moved.
 *
 * @type {!Node | Element}
 * @private
 */
PointerActionBundle.prototype.currentTarget_;


/**
 * How many milli-seconds to evaluate a double click event.
 *
 * @type {number}
 * @private
 */
PointerActionBundle.DOUBLE_CLICK_INTERVAL_ = 500;


/**
 * The timer ID.
 *
 * @type {number}
 * @private
 */
PointerActionBundle.prototype.longPressTimer_;


/**
 * The minimum swipe distance.
 *
 * @type {number}
 * @private
 */
PointerActionBundle.MINIMUM_SWIPE_DISTANCE_ = 20;


/**
 * The timestamp of the pointer down.
 *
 * @type {number}
 * @private
 */
PointerActionBundle.prototype.pointerDownTimeStamp_ = 0;


/**
 * The timestamp of the pointer up.
 *
 * @type {number}
 * @private
 */
PointerActionBundle.prototype.pointerUpTimeStamp_ = 0;


/**
 * True if it is double clicking.
 *
 * @type {boolean}
 * @private
 */
PointerActionBundle.prototype.isDBLClicking_ = false;


/**
 * True if it is long pressing.
 *
 * @type {boolean}
 * @private
 */
PointerActionBundle.prototype.isLongPressing_ = false;


/**
 * True if it is flickering.
 *
 * @type {boolean}
 * @private
 */
PointerActionBundle.prototype.isFlickering_ = false;


/**
 * Gets the view.
 *
 * @param {Node} target .
 * @return {i18n.input.chrome.inputview.elements.Element} .
 * @private
 */
PointerActionBundle.prototype.getView_ = function(target) {
  if (!target) {
    return null;
  }
  var element = /** @type {!Element} */ (target);
  var view = element['view'];
  while (!view && element) {
    view = element['view'];
    element = goog.dom.getParentElement(element);
  }
  return view;
};


/**
 * Handles touchmove event for one target.
 *
 * @param {!Event} touchEvent .
 */
PointerActionBundle.prototype.handleTouchMove = function(touchEvent) {
  var direction = 0;
  var deltaX = this.swipeState_.previousX == 0 ? 0 : (touchEvent.pageX -
      this.swipeState_.previousX);
  var deltaY = this.swipeState_.previousY == 0 ? 0 :
      (touchEvent.pageY - this.swipeState_.previousY);
  this.swipeState_.offsetX += deltaX;
  this.swipeState_.offsetY += deltaY;
  this.dispatchEvent(new i18n.input.chrome.inputview.events.DragEvent(
      this.view_, direction, /** @type {!Node} */ (touchEvent.target),
      touchEvent.pageX, touchEvent.pageY, deltaX, deltaY));

  var minimumSwipeDist = PointerActionBundle.
      MINIMUM_SWIPE_DISTANCE_;

  if (this.swipeState_.offsetX > minimumSwipeDist) {
    direction |= i18n.input.chrome.inputview.SwipeDirection.RIGHT;
    this.swipeState_.offsetX = 0;
  } else if (this.swipeState_.offsetX < -minimumSwipeDist) {
    direction |= i18n.input.chrome.inputview.SwipeDirection.LEFT;
    this.swipeState_.offsetX = 0;
  }

  if (Math.abs(deltaY) > Math.abs(deltaX)) {
    if (this.swipeState_.offsetY > minimumSwipeDist) {
      direction |= i18n.input.chrome.inputview.SwipeDirection.DOWN;
      this.swipeState_.offsetY = 0;
    } else if (this.swipeState_.offsetY < -minimumSwipeDist) {
      direction |= i18n.input.chrome.inputview.SwipeDirection.UP;
      this.swipeState_.offsetY = 0;
    }
  }

  this.swipeState_.previousX = touchEvent.pageX;
  this.swipeState_.previousY = touchEvent.pageY;

  if (direction > 0) {
    // If there is any movement, cancel the longpress timer.
    goog.Timer.clear(this.longPressTimer_);
    this.dispatchEvent(new i18n.input.chrome.inputview.events.SwipeEvent(
        this.view_, direction, /** @type {!Node} */ (touchEvent.target),
        touchEvent.pageX, touchEvent.pageY));
    var currentTargetView = this.getView_(this.currentTarget_);
    if (this.view_) {
      this.isFlickering_ = !this.isLongPressing_ && !!(this.view_.pointerConfig.
          flickerDirection & direction) && currentTargetView == this.view_;
    }
  }

  this.maybeSwitchTarget_(touchEvent);
};


/**
 * If the target is switched to a new one, sends out a pointer_over for the new
 * target and sends out a pointer_out for the old target.
 *
 * @param {!Event | !goog.events.BrowserEvent} e .
 * @private
 */
PointerActionBundle.prototype.maybeSwitchTarget_ = function(e) {
  if (!this.isFlickering_) {
    var pageOffset = this.getPageOffset_(e);
    var actualTarget = document.elementFromPoint(pageOffset.x, pageOffset.y);
    var currentTargetView = this.getView_(this.currentTarget_);
    var actualTargetView = this.getView_(actualTarget);
    if (currentTargetView != actualTargetView) {
      if (currentTargetView) {
        this.dispatchEvent(new i18n.input.chrome.inputview.events.PointerEvent(
            currentTargetView,
            i18n.input.chrome.inputview.events.EventType.POINTER_OUT,
            this.currentTarget_, pageOffset.x, pageOffset.y));
      }
      if (actualTargetView) {
        this.dispatchEvent(new i18n.input.chrome.inputview.events.PointerEvent(
            actualTargetView,
            i18n.input.chrome.inputview.events.EventType.POINTER_OVER,
            actualTarget, pageOffset.x, pageOffset.y));
      }
      this.currentTarget_ = actualTarget;
    }
  }
};


/**
 * Handles pointer up, e.g., mouseup/touchend.
 *
 * @param {!goog.events.BrowserEvent} e The event.
 */
PointerActionBundle.prototype.handlePointerUp = function(e) {
  goog.Timer.clear(this.longPressTimer_);
  var pageOffset = this.getPageOffset_(e);
  this.dispatchEvent(new i18n.input.chrome.inputview.events.PointerEvent(
      this.view_, i18n.input.chrome.inputview.events.EventType.LONG_PRESS_END,
      e.target, pageOffset.x, pageOffset.y));
  if (this.isDBLClicking_) {
    this.dispatchEvent(new i18n.input.chrome.inputview.events.PointerEvent(
        this.view_, i18n.input.chrome.inputview.events.EventType.
        DOUBLE_CLICK_END, e.target, pageOffset.x, pageOffset.y));
  } else if (!(this.isLongPressing_ && this.view_.pointerConfig.
      longPressWithoutPointerUp)) {
    if (this.isLongPressing_) {
      // If the finger didn't move but it triggers a longpress, it could cause
      // a target switch, so checks it here.
      this.maybeSwitchTarget_(e);
    }
    var view = this.getView_(this.currentTarget_);
    var target = this.currentTarget_;
    if (this.isFlickering_) {
      view = this.view_;
      target = e.target;
    }
    if (view) {
      this.pointerUpTimeStamp_ = new Date().getTime();
      this.dispatchEvent(new i18n.input.chrome.inputview.events.PointerEvent(
          view, i18n.input.chrome.inputview.events.EventType.POINTER_UP,
          target, pageOffset.x, pageOffset.y, this.pointerUpTimeStamp_));
    }
  }
  if (this.getView_(this.currentTarget_) == this.view_) {
    this.dispatchEvent(new i18n.input.chrome.inputview.events.PointerEvent(
        this.view_, i18n.input.chrome.inputview.events.EventType.CLICK,
        e.target, pageOffset.x, pageOffset.y));
  }
  this.isDBLClicking_ = false;
  this.isLongPressing_ = false;
  this.isFlickering_ = false;
  this.swipeState_.reset();

  e.preventDefault();
  if (this.view_ && this.view_.pointerConfig.stopEventPropagation) {
    e.stopPropagation();
  }
};


/**
 * Cancel double click recognition on this target.
 */
PointerActionBundle.prototype.cancelDoubleClick = function() {
  this.pointerDownTimeStamp_ = 0;
};


/**
 * Handles pointer down, e.g., mousedown/touchstart.
 *
 * @param {!goog.events.BrowserEvent} e The event.
 */
PointerActionBundle.prototype.handlePointerDown = function(e) {
  this.currentTarget_ = e.target;
  goog.Timer.clear(this.longPressTimer_);
  if (e.type != goog.events.EventType.MOUSEDOWN) {
    // Don't trigger long press for mouse event.
    this.maybeTriggerKeyDownLongPress_(e);
  }
  this.maybeHandleDBLClick_(e);
  if (!this.isDBLClicking_) {
    var pageOffset = this.getPageOffset_(e);
    this.dispatchEvent(new i18n.input.chrome.inputview.events.PointerEvent(
        this.view_, i18n.input.chrome.inputview.events.EventType.POINTER_DOWN,
        e.target, pageOffset.x, pageOffset.y, this.pointerDownTimeStamp_));
  }

  if (this.view_ && this.view_.pointerConfig.preventDefault) {
    e.preventDefault();
  }
  if (this.view_ && this.view_.pointerConfig.stopEventPropagation) {
    e.stopPropagation();
  }
};


/**
 * Gets the page offset from the event which may be mouse event or touch event.
 *
 * @param {!goog.events.BrowserEvent | !TouchEvent | !Event} e .
 * @return {!goog.math.Coordinate} .
 * @private
 */
PointerActionBundle.prototype.getPageOffset_ = function(e) {
  if (e.pageX && e.pageY) {
    return new goog.math.Coordinate(e.pageX, e.pageY);
  }

  if (!e.getBrowserEvent) {
    return new goog.math.Coordinate(0, 0);
  }

  var nativeEvt = e.getBrowserEvent();
  if (nativeEvt.pageX && nativeEvt.pageY) {
    return new goog.math.Coordinate(nativeEvt.pageX, nativeEvt.pageY);
  }


  var touchEventList = nativeEvt['changedTouches'];
  if (!touchEventList || touchEventList.length == 0) {
    touchEventList = nativeEvt['touches'];
  }
  if (touchEventList && touchEventList.length > 0) {
    var touchEvent = touchEventList[0];
    return new goog.math.Coordinate(touchEvent.pageX, touchEvent.pageY);
  }

  return new goog.math.Coordinate(0, 0);
};


/**
 * Maybe triggers the long press timer when pointer down.
 *
 * @param {!goog.events.BrowserEvent} e The event.
 * @private
 */
PointerActionBundle.prototype.maybeTriggerKeyDownLongPress_ = function(e) {
  if (this.view_ && (this.view_.pointerConfig.longPressWithPointerUp ||
      this.view_.pointerConfig.longPressWithoutPointerUp)) {
    this.longPressTimer_ = goog.Timer.callOnce(
        goog.bind(this.triggerLongPress_, this, e),
        this.view_.pointerConfig.longPressDelay, this);
  }
};


/**
 * Maybe handle the double click.
 *
 * @param {!goog.events.BrowserEvent} e .
 * @private
 */
PointerActionBundle.prototype.maybeHandleDBLClick_ = function(e) {
  if (this.view_ && this.view_.pointerConfig.dblClick) {
    var timeInMs = new Date().getTime();
    var interval = this.view_.pointerConfig.dblClickDelay ||
        PointerActionBundle.DOUBLE_CLICK_INTERVAL_;
    var nativeEvt = e.getBrowserEvent();
    if ((timeInMs - this.pointerDownTimeStamp_) < interval) {
      this.dispatchEvent(new i18n.input.chrome.inputview.events.PointerEvent(
          this.view_, i18n.input.chrome.inputview.events.EventType.DOUBLE_CLICK,
          e.target, nativeEvt.pageX, nativeEvt.pageY));
      this.isDBLClicking_ = true;
    }
    this.pointerDownTimeStamp_ = timeInMs;
  }
};


/**
 * Triggers long press event.
 *
 * @param {!goog.events.BrowserEvent} e The event.
 * @private
 */
PointerActionBundle.prototype.triggerLongPress_ = function(e) {
  var nativeEvt = e.getBrowserEvent();
  this.dispatchEvent(new i18n.input.chrome.inputview.events.PointerEvent(
      this.view_, i18n.input.chrome.inputview.events.EventType.LONG_PRESS,
      e.target, nativeEvt.pageX, nativeEvt.pageY));
  this.isLongPressing_ = true;
};


/** @override */
PointerActionBundle.prototype.disposeInternal = function() {
  goog.dispose(this.longPressTimer_);

  goog.base(this, 'disposeInternal');
};

});  // goog.scope
