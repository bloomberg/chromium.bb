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
goog.provide('i18n.input.chrome.inputview.handler.PointerHandler');

goog.require('goog.Timer');
goog.require('goog.events.EventHandler');
goog.require('goog.events.EventTarget');
goog.require('goog.events.EventType');
goog.require('goog.math.Coordinate');
goog.require('i18n.input.chrome.inputview.handler.PointerActionBundle');

goog.scope(function() {



/**
 * The pointer controller.
 *
 * @param {!Element=} opt_target .
 * @constructor
 * @extends {goog.events.EventTarget}
 */
i18n.input.chrome.inputview.handler.PointerHandler = function(opt_target) {
  goog.base(this);

  /**
   * The pointer handlers.
   *
   * @type {!Object.<number, !i18n.input.chrome.inputview.handler.PointerActionBundle>}
   * @private
   */
  this.pointerActionBundles_ = {};

  /**
   * The event handler.
   *
   * @type {!goog.events.EventHandler}
   * @private
   */
  this.eventHandler_ = new goog.events.EventHandler(this);

  var target = opt_target || document;
  this.eventHandler_.
      listen(target, [goog.events.EventType.MOUSEDOWN,
        goog.events.EventType.TOUCHSTART], this.onPointerDown_, true).
      listen(target, [goog.events.EventType.MOUSEUP,
        goog.events.EventType.TOUCHEND], this.onPointerUp_, true).
      listen(target, goog.events.EventType.TOUCHMOVE, this.onTouchMove_,
          true);
};
goog.inherits(i18n.input.chrome.inputview.handler.PointerHandler,
    goog.events.EventTarget);
var PointerHandler = i18n.input.chrome.inputview.handler.PointerHandler;


/**
 * The canvas class name.
 * @const {string}
 * @private
 */
PointerHandler.CANVAS_CLASS_NAME_ = 'ita-hwt-canvas';


/**
 * Mouse down tick, which is for delayed pointer up for tap action on touchpad.
 *
 * @private {Date}
 */
PointerHandler.prototype.mouseDownTick_ = null;


/**
 * Event handler for previous mousedown or touchstart target.
 *
 * @private {i18n.input.chrome.inputview.handler.PointerActionBundle}
 */
PointerHandler.prototype.previousPointerActionBundle_ = null;


/**
 * Pointer action bundle for mouse down.
 * This is used in mouse up handler because mouse up event may have different
 * target than the mouse down event.
 *
 * @private {i18n.input.chrome.inputview.handler.PointerActionBundle}
 */
PointerHandler.prototype.pointerActionBundleForMouseDown_ = null;


/**
 * Creates a new pointer handler.
 *
 * @param {!Node} target .
 * @return {!i18n.input.chrome.inputview.handler.PointerActionBundle} .
 * @private
 */
PointerHandler.prototype.createPointerActionBundle_ = function(target) {
  var uid = goog.getUid(target);
  if (!this.pointerActionBundles_[uid]) {
    this.pointerActionBundles_[uid] = new i18n.input.chrome.inputview.handler.
        PointerActionBundle(target, this);
  }
  return this.pointerActionBundles_[uid];
};


/**
 * Callback for mouse/touch down on the target.
 *
 * @param {!goog.events.BrowserEvent} e The event.
 * @private
 */
PointerHandler.prototype.onPointerDown_ = function(e) {
  var pointerActionBundle = this.createPointerActionBundle_(
      /** @type {!Node} */ (e.target));
  if (this.previousPointerActionBundle_ &&
      this.previousPointerActionBundle_ != pointerActionBundle) {
    this.previousPointerActionBundle_.cancelDoubleClick();
  }
  this.previousPointerActionBundle_ = pointerActionBundle;
  pointerActionBundle.handlePointerDown(e);
  if (e.type == goog.events.EventType.MOUSEDOWN) {
    this.mouseDownTick_ = new Date();
    this.pointerActionBundleForMouseDown_ = pointerActionBundle;
  }
};


/**
 * Callback for pointer out.
 *
 * @param {!goog.events.BrowserEvent} e The event.
 * @private
 */
PointerHandler.prototype.onPointerUp_ = function(e) {
  if (e.type == goog.events.EventType.MOUSEUP) {
    // If mouseup happens too fast after mousedown, it may be a tap action on
    // touchpad, so delay the pointer up action so user can see the visual
    // flash.
    if (this.mouseDownTick_ && new Date() - this.mouseDownTick_ < 10) {
      goog.Timer.callOnce(this.onPointerUp_.bind(this, e), 50);
      return;
    }
    if (this.pointerActionBundleForMouseDown_) {
      this.pointerActionBundleForMouseDown_.handlePointerUp(e);
      this.pointerActionBundleForMouseDown_ = null;
      return;
    }
  }
  var uid = goog.getUid(e.target);
  var pointerActionBundle = this.pointerActionBundles_[uid];
  if (pointerActionBundle) {
    pointerActionBundle.handlePointerUp(e);
  }
};


/**
 * Callback for touchmove.
 *
 * @param {!goog.events.BrowserEvent} e The event.
 * @private
 */
PointerHandler.prototype.onTouchMove_ = function(e) {
  var touches = e.getBrowserEvent()['touches'];
  if (!touches || touches.length == 0) {
    return;
  }
  for (var i = 0; i < touches.length; i++) {
    var uid = goog.getUid(touches[i].target);
    var pointerActionBundle = this.pointerActionBundles_[uid];
    if (pointerActionBundle) {
      pointerActionBundle.handleTouchMove(touches[i]);
    }
  }
};


/** @override */
PointerHandler.prototype.disposeInternal = function() {
  for (var bundle in this.pointerActionBundles_) {
    goog.dispose(bundle);
  }
  goog.dispose(this.eventHandler_);

  goog.base(this, 'disposeInternal');
};

});  // goog.scope
