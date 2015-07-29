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
goog.provide('i18n.input.chrome.inputview.elements.content.GestureCanvasView.GestureStroke');
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
   * Flag used to indicate whether or not gesturing is currently occuring.
   *
   * @type {boolean}
   */
  this.isGesturing = false;

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

  /**
   * The time the first point was added to this stroke. Used to keep all points
   * relative to the first point.
   *
   * @private {number}
   */
  this.firstTime_ = 0;
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
 * Starting red value.
 *
 * @const {number}
 */
GestureStroke.STARTING_R_VALUE = 0;


/**
 * Starting green value.
 *
 * @const {number}
 */
GestureStroke.STARTING_G_VALUE = 180;


/**
 * Starting blue value.
 *
 * @const {number}
 */
GestureStroke.STARTING_B_VALUE = 204;


/**
 * Calculates the color of the point based on the ttl.
 *
 * @param {number} ttl The time to live of the point.
 * @return {string} The color to use for the point.
 * @private
 */
GestureStroke.calculateColor_ = function(ttl) {
  // TODO(maxw): Use this percentage to fade the stroke color.
  var remainingTtlPercentage = ttl / GestureStroke.STARTING_TTL;
  var rValue = GestureStroke.STARTING_R_VALUE;
  var gValue = GestureStroke.STARTING_G_VALUE;
  var bValue = GestureStroke.STARTING_B_VALUE;
  return 'rgb(' + rValue + ', ' + gValue + ', ' + bValue + ')';
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
 * Add a point to this stroke.
 *
 * @param {Point} p The point to add to this stroke.
 */
GestureStroke.prototype.pushPoint = function(p) {
  if (this.points.length == 0) {
    this.firstTime_ = p.time;
  }
  // Convert the timestamp so it is relative to the first point, including
  // setting the first point to zero.
  p.time -= this.firstTime_;
  this.points.push(p);
};



/**
 * One point in the gesture stroke.
 *
 * This class is used for both rendering the gesture stroke, and also for
 * transmitting the stroke coordinates to the recognizer for decoding.
 *
 * @param {number} x The x coordinate.
 * @param {number} y The y coordinate.
 * @param {number} identifier The pointer event identifier.
 * @constructor
 */
i18n.input.chrome.inputview.elements.content.GestureCanvasView.Point =
    function(x, y, identifier) {
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
   * The pointer ID.
   *
   * @type {number}
   */
  this.pointer = 0;

  /**
   * The time-to-live value of the point, used to render the trail fading
   * effect.
   *
   * @type {number}
   */
  this.ttl = GestureStroke.STARTING_TTL;

  /**
   * The time this point was created, in ms since epoch.
   *
   * @type {number}
   */
  this.time = Date.now();

  /**
   * The action type of the point.
   *
   * @type {i18n.input.chrome.inputview.elements.content.GestureCanvasView.
   *        Point.Action}
   */
  this.action = i18n.input.chrome.inputview.elements.content.GestureCanvasView.
      Point.Action.ACTION_MOVE;

  /**
   * The pointer event identifier associated with this point.
   *
   * @type {number}
   */
  this.identifier = identifier;
};
var Point =
    i18n.input.chrome.inputview.elements.content.GestureCanvasView.Point;


/**
 * Enum describing the type of action for a given point.
 *
 * @enum {number}
 */
Point.Action = {
  ACTION_DOWN: 0,
  ACTION_UP: 1,
  ACTION_MOVE: 2
};


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
  // Check if the last stroke was active before this point in order to determine
  // if the user is gesturing. Only check the last stroke and not all the
  // strokes because all previous strokes might be rendering/degrading, but that
  // does not determine if the user is currently gesturing.
  var was_active = this.latestStrokeActive_();

  if (this.strokeList_.length == 0) {
    this.strokeList_.push(new GestureStroke());
  }
  var lastStroke = this.strokeList_[this.strokeList_.length - 1];
  if (lastStroke.points.length > 0 &&
      e.identifier != lastStroke.points[0].identifier) {
    // Should only add new points with the same identifier. This ignores pointer
    // events created by, say, a second finger interacting with the screen while
    // an existing gesture is going on.
    return;
  }
  lastStroke.pushPoint(this.createGesturePoint_(e));

  // If the new point |e| activated the last stroke, set gesturing to true.
  if (!was_active && this.latestStrokeActive_()) {
    this.isGesturing = true;
  }
};


/**
 * Clear the view.
 */
GestureCanvasView.prototype.clear = function() {
  this.strokeList_ = [];
  this.draw_();
};


/**
 * Returns true iff the last stroke is currently active.
 *
 * @return {boolean} Whether or not the last stroke is active.
 * @private
 */
GestureCanvasView.prototype.latestStrokeActive_ = function() {
  if (this.strokeList_.length == 0) {
    return false;
  }
  return this.strokeList_[this.strokeList_.length - 1].isActive();
};


/**
 * Begins a new gesture.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e Drag event to
 *        draw.
 */
GestureCanvasView.prototype.startStroke = function(e) {
  // If there is currently a stroke and it does not match the identifier of this
  // new point, then ignore this call. This is to prevent a second finger from
  // interrupting an existing stroke.
  if (this.strokeList_.length > 0 &&
      e.identifier != this.strokeList_[this.strokeList_.length - 1].points[0]
          .identifier) {
    return;
  }
  // Always start a new array to separate previous strokes from this new one.
  this.strokeList_.push(new GestureStroke());
  var point = this.createGesturePoint_(e);
  point.action = Point.Action.ACTION_DOWN;
  // TODO: This line is a NOP since createGesturePoint_ already assigns the
  // pointer value, but it must be called to prevent closure from optimizing out
  // the pointer member. This needs to be fixed to use the true pointer ID of e.
  point.pointer = 0;
  this.strokeList_[this.strokeList_.length - 1].pushPoint(point);
};


/**
 * Ends the current gesture.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e Final pointer
 *     event to handle.
 */
GestureCanvasView.prototype.endStroke = function(e) {
  // TODO: Ensure that this gets called even when the final touch event
  //     is not on the client.

  // Ignore points that do not have the same identifier.
  if (e.identifier !=
      this.strokeList_[this.strokeList_.length - 1].points[0].identifier) {
    return;
  }

  // Send the final event.
  var point = this.createGesturePoint_(e);
  point.action = Point.Action.ACTION_UP;
  this.strokeList_[this.strokeList_.length - 1].pushPoint(point);
  this.isGesturing = false;
};


/**
 * Returns the last stroke, or null if there are currently no strokes.
 *
 * @return {?GestureStroke}
 */
GestureCanvasView.prototype.getLastStroke = function() {
  if (this.strokeList_.length == 0) {
    return null;
  }
  return this.strokeList_[this.strokeList_.length - 1];
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
  return new Point(e.x - offset.x, e.y - offset.y, e.identifier);
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
