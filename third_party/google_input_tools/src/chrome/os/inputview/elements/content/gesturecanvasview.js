// Copyright 2015 The ChromeOS IME Authors. All Rights Reserved.
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
goog.provide('i18n.input.chrome.inputview.elements.content.GestureCanvasView');
goog.provide('i18n.input.chrome.inputview.elements.content.GestureCanvasView.Point');

goog.require('goog.async.Delay');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.style');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.elements.Element');
goog.require('i18n.input.chrome.inputview.elements.ElementType');

goog.scope(function() {
var Css = i18n.input.chrome.inputview.Css;
var ElementType = i18n.input.chrome.inputview.elements.ElementType;
var TagName = goog.dom.TagName;



/**
 * The gesture canvas view.
 *
 * This view is used to display the strokes for gesture typing.
 *
 * @param {goog.events.EventTarget=} opt_eventTarget The parent event target.
 * @constructor
 * @extends {i18n.input.chrome.inputview.elements.Element}
 */
i18n.input.chrome.inputview.elements.content.GestureCanvasView =
    function(opt_eventTarget) {
  GestureCanvasView.base(this, 'constructor', '',
      ElementType.GESTURE_CANVAS_VIEW, opt_eventTarget);

  /**
   * Actual canvas for drawing the gesture trail.
   *
   * @private {!Element}
   */
  this.drawingCanvas_;

  /**
   * Context for drawing the gesture trail.
   *
   * @private {!CanvasRenderingContext2D}
   */
  this.drawingContext_;

  /**
   * A list of list of gesture points to be rendered on the canvas as strokes.
   *
   * @private {!Array<GestureStroke>}
   */
  this.strokeList_ = [];

  /** @private {!goog.async.Delay} */
  this.animator_ = new goog.async.Delay(this.animateGestureTrail_, 0, this);
};
var GestureCanvasView =
    i18n.input.chrome.inputview.elements.content.GestureCanvasView;
goog.inherits(GestureCanvasView, i18n.input.chrome.inputview.elements.Element);



/**
 * A single stroke on the canvas.
 *
 * @constructor
 */
i18n.input.chrome.inputview.elements.content.GestureCanvasView.GestureStroke =
    function() {
  /**
   * The list of points that make up this stroke.
   *
   * @type {!Array.<!Point>}
   */
  this.points = [];

  /**
   * Whether or not this stroke is considered active. i.e. whether or not it
   * should be considered for rendering and decoding.
   *
   * @private {boolean}
   */
  this.isActive_ = false;
};
var GestureStroke =
    i18n.input.chrome.inputview.elements.content.GestureCanvasView
        .GestureStroke;


/**
 * Rate at which the ttl should degrade for the fading stroke effect.
 *
 * @const {number}
 */
GestureStroke.DEGRADATION_RATE = 5;


/**
 * Starting time-to-live value.
 *
 * @const {number}
 */
GestureStroke.STARTING_TTL = 255;


// TODO(stevet): This is temporary and needs to be updated with a dynamic value
// that considers other parameters like the width of character keys.
/**
 * Distance threshold for when a stroke is considered active, in pixels.
 *
 * @const {number}
 */
GestureStroke.ACTIVE_THRESHOLD = 40;


/**
 * Calculates the color of the point based on the ttl.
 *
 * @param {number} ttl The time to live of the point.
 * @return {string} The color to use for the point.
 * @private
 */
GestureStroke.calculateColor_ = function(ttl) {
  var shade = GestureStroke.STARTING_TTL - ttl;
  if (shade < 0) {
    shade = 0;
  }
  return 'rgb(' + shade + ', ' + shade + ', ' + shade + ')';
};


/**
 * Calculates the distance between two points.
 *
 * @param {!Point} first The first point.
 * @param {!Point} second The second point.
 * @return {number} The number of pixels between first and second.
 * @private
 */
GestureStroke.calculateDistance_ = function(first, second) {
  // Simply use the Pythagorean.
  var a = Math.abs(first.x - second.x);
  var b = Math.abs(first.y - second.y);
  return Math.sqrt(Math.pow(a, 2) + Math.pow(b, 2));
};


/**
 * Calculates the line width of the point based on the ttl.
 *
 * @param {number} ttl The time to live of the point.
 * @return {number} The line width to use for the point.
 * @private
 */
GestureStroke.calculateLineWidth_ = function(ttl) {
  var ratio = ttl / GestureStroke.STARTING_TTL;
  if (ratio < 0) {
    ratio = 0;
  }
  return 9 * ratio;
};


/**
 * Degrades all the points in this stroke.
 *
 * @return {boolean} Returns true if it was possible to degrade one or more
 *     points, otherwise it means that this stroke is now empty.
 */
GestureStroke.prototype.degrade = function() {
  var all_empty = true;
  for (var i = 0; i < this.points.length; i++) {
    if (this.points[i].ttl > 0) {
      this.points[i].ttl -= GestureStroke.DEGRADATION_RATE;
      all_empty = false;
    }
  }
  return !all_empty;
};


/**
 * Draw the gesture trail for this stroke onto the canvas context.
 *
 * @param {!CanvasRenderingContext2D} context The drawing context to render to.
 */
GestureStroke.prototype.draw = function(context) {
  // Only start drawing active strokes. Note that TTL still updates even if a
  // stroke is not yet active.
  if (!this.isActive()) {
    return;
  }

  for (var i = 1; i < this.points.length; i++) {
    var first = this.points[i - 1];
    var second = this.points[i];
    // All rendering calculations are based on the second point in the segment
    // because there must be at least two points for something to be rendered.
    var ttl = second.ttl;
    if (ttl <= 0) {
      continue;
    }

    context.beginPath();
    context.moveTo(first.x, first.y);
    context.lineTo(second.x, second.y);
    // TODO(stevet): Use alpha and #00B4CC.
    context.strokeStyle = GestureStroke.calculateColor_(ttl);
    context.fillStyle = 'none';
    context.lineWidth = GestureStroke.calculateLineWidth_(ttl);
    context.lineCap = 'round';
    context.lineJoin = 'round';
    context.stroke();
  }
};


/**
 * Returns true iff this stroke is considered "active". This means that it has
 * passed a certain threshold and should be considered for rendering and
 * decoding.
 *
 * @return {boolean} Whether or not the stroke is active.
 */
GestureStroke.prototype.isActive = function() {
  // Once a stroke is active, it remains active.
  if (this.isActive_) {
    return this.isActive_;
  }

  if (this.points.length < 2) {
    return false;
  }

  // Calculate the distance between the first point and the latest one.
  this.isActive_ = GestureStroke.calculateDistance_(
      this.points[0], this.points[this.points.length - 1]) >
          GestureStroke.ACTIVE_THRESHOLD;

  return this.isActive_;
};



/**
 * One point in the gesture stroke.
 *
 * This class is used for both rendering the gesture stroke, and also for
 * transmitting the stroke coordinates to the recognizer for decoding.
 *
 * @param {number} x The x coordinate.
 * @param {number} y The y coordinate.
 * @constructor
 */
i18n.input.chrome.inputview.elements.content.GestureCanvasView.Point =
    function(x, y) {
  /**
   * The left offset relative to the canvas.
   *
   * @type {number}
   */
  this.x = x;

  /**
   * The top offset relative to the canvas.
   *
   * @type {number}
   */
  this.y = y;

  /**
   * The time-to-live value of the point, used to render the trail fading
   * effect.
   *
   * @type {number}
   */
  this.ttl = GestureStroke.STARTING_TTL;
};
var Point =
    i18n.input.chrome.inputview.elements.content.GestureCanvasView.Point;


/**
 * Draw the gesture trail.
 *
 * @private
 */
GestureCanvasView.prototype.draw_ = function() {
  // First, clear the canvas.
  this.drawingContext_.clearRect(
      0, 0, this.drawingCanvas_.width, this.drawingCanvas_.height);

  for (var i = 0; i < this.strokeList_.length; i++) {
    this.strokeList_[i].draw(this.drawingContext_);
  }
};


/** @override */
GestureCanvasView.prototype.createDom = function() {
  goog.base(this, 'createDom');
  var dom = this.getDomHelper();
  var elem = this.getElement();
  goog.dom.classlist.add(elem, Css.GESTURE_CANVAS_VIEW);

  // Create the HTML5 canvas where the gesture trail is actually rendered.
  this.drawingCanvas_ = dom.createDom(TagName.CANVAS, Css.DRAWING_CANVAS);
  this.drawingContext_ = this.drawingCanvas_.getContext('2d');
  dom.appendChild(elem, this.drawingCanvas_);

  this.animator_.start();
};


/** @override */
GestureCanvasView.prototype.resize = function(width, height) {
  GestureCanvasView.base(this, 'resize', width, height);

  // Explicitly set the width and height of the canvas, which is necessary
  // to ensure that rendered elements are not stretched.
  this.drawingCanvas_.width = width;
  this.drawingCanvas_.height = height;
};


/**
 * Converts a drag event to a gesture Point and adds it to the collection of
 * points.
 *
 * @param {!i18n.input.chrome.inputview.events.DragEvent} e Drag event to draw.
 */
GestureCanvasView.prototype.addPoint = function(e) {
  if (this.strokeList_.length == 0) {
    this.strokeList_.push(new GestureStroke());
  }

  this.strokeList_[this.strokeList_.length - 1].points.push(
      this.createGesturePoint_(e));
};


/**
 * Clear the view.
 */
GestureCanvasView.prototype.clear = function() {
  this.strokeList_ = [];
  this.draw_();
};


/**
 * Returns true iff there is a stroke that is currently active.
 *
 * @return {boolean} Whether or not there was an active stroke.
 */
GestureCanvasView.prototype.hasActiveStroke = function() {
  for (var i = 0; i < this.strokeList_.length; i++) {
    // TODO(stevet): Fix this approximation with a change that takes into
    // account strokes that are still active because they are degrading, but the
    // user has already finished the gesture (i.e. touched up).
    if (this.strokeList_[i].isActive()) {
      return true;
    }
  }

  return false;
};


/**
 * Begins a new gesture.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e Drag event to
 *        draw.
 */
GestureCanvasView.prototype.startStroke = function(e) {
  // Always start a new array to separate previous strokes from this new one.
  this.strokeList_.push(new GestureStroke());

  this.strokeList_[this.strokeList_.length - 1].points.push(
      this.createGesturePoint_(e));
};


/**
 * The gesture trail animation function.
 *
 * @private
 */
GestureCanvasView.prototype.animateGestureTrail_ = function() {
  // TODO(stevet): This approximates drawing at 60fps. Refactor this and the
  // voice input code to use a common call to requestRenderFrame.
  var timeStep = 16;

  this.draw_();
  this.degradeStrokes_();
  this.animator_.start(timeStep);
};


/**
 * Returns a gesture point for a given event, with the correct coordinates.
 *
 * @param {!i18n.input.chrome.inputview.events.DragEvent|
 *         i18n.input.chrome.inputview.events.PointerEvent} e The event to
 *             convert.
 * @return {Point} The converted gesture point.
 * @private
 */
GestureCanvasView.prototype.createGesturePoint_ = function(e) {
  var offset = goog.style.getPageOffset(this.drawingCanvas_);
  return new Point(e.x - offset.x, e.y - offset.y);
};


/**
 * Degrades the ttl of the points in all gesture strokes.
 *
 * @private
 */
GestureCanvasView.prototype.degradeStrokes_ = function() {
  for (var i = 0; i < this.strokeList_.length; i++) {
    // In the case where all points in the list are empty, dispose of the first.
    if (!this.strokeList_[i].degrade()) {
      this.strokeList_.splice(i, 1);
      i--;
    }
  }
};


});  // goog.scope
