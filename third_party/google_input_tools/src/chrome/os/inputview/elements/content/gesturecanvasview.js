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
   * @private {!Array.<Array>}
   */
  this.strokeList_ = [];

  /** @private {!goog.async.Delay} */
  this.animator_ = new goog.async.Delay(this.animateGestureTrail_, 0, this);
};
var GestureCanvasView =
    i18n.input.chrome.inputview.elements.content.GestureCanvasView;
goog.inherits(GestureCanvasView, i18n.input.chrome.inputview.elements.Element);


/**
 * Rate at which the ttl should degrade for the fading stroke effect.
 *
 * @const {number}
 */
GestureCanvasView.DEGRADATION_RATE = 5;


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
    var pointList = this.strokeList_[i];

    for (var j = 1; j < pointList.length; j++) {
      var first = pointList[j - 1];
      var second = pointList[j];
      // All rendering calculations are based on the second point in the segment
      // because there must be at least two points for something to be rendered.
      var ttl = second.ttl;
      if (ttl <= 0) {
        continue;
      }

      this.drawingContext_.beginPath();
      this.drawingContext_.moveTo(first.x, first.y);
      this.drawingContext_.lineTo(second.x, second.y);
      // TODO(stevet): Use alpha and #00B4CC.
      this.drawingContext_.strokeStyle = this.calculateColor_(ttl);
      this.drawingContext_.fillStyle = 'none';
      this.drawingContext_.lineWidth = this.calculateLineWidth_(ttl);
      this.drawingContext_.lineCap = 'round';
      this.drawingContext_.lineJoin = 'round';
      this.drawingContext_.stroke();
    }
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
    this.strokeList_.push([]);
  }

  this.strokeList_[this.strokeList_.length - 1].push(
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
 * Begins a new gesture.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e Drag event to
 *        draw.
 */
GestureCanvasView.prototype.startStroke = function(e) {
  // Always start a new array to separate previous strokes from this new one.
  this.strokeList_.push([]);

  this.strokeList_[this.strokeList_.length - 1].push(
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
 * Calculates the line width of the point based on the ttl.
 *
 * @param {number} ttl The time to live of the point.
 * @return {number} The line width to use for the point.
 * @private
 */
GestureCanvasView.prototype.calculateLineWidth_ = function(ttl) {
  var ratio = ttl / 255.0;
  if (ratio < 0) {
    ratio = 0;
  }
  return 9 * ratio;
};


/**
 * Calculates the color of the point based on the ttl.
 *
 * @param {number} ttl The time to live of the point.
 * @return {string} The color to use for the point.
 * @private
 */
GestureCanvasView.prototype.calculateColor_ = function(ttl) {
  var shade = 225 - ttl;
  if (shade < 0) {
    shade = 0;
  }
  return 'rgb(' + shade + ', ' + shade + ', ' + shade + ')';
};


/**
 * Returns a gesture point for a given event, with the correct coordinates.
 *
 * @param {!i18n.input.chrome.inputview.events.DragEvent|
 *        i18n.input.chrome.inputview.events.PointerEvent} e The event to
 *        convert.
 * @return {i18n.input.chrome.inputview.elements.content.
 *          GestureCanvasView.Point} The converted gesture point.
 * @private
 */
GestureCanvasView.prototype.createGesturePoint_ = function(e) {
  var offset = goog.style.getPageOffset(this.drawingCanvas_);
  return new
      i18n.input.chrome.inputview.elements.content.GestureCanvasView.Point(
          e.x - offset.x, e.y - offset.y);
};


/**
 * Degrades the ttl of the points in all gesture strokes.
 *
 * @private
 */
GestureCanvasView.prototype.degradeStrokes_ = function() {
  for (var i = 0; i < this.strokeList_.length; i++) {
    var all_empty = true;
    var pointList = this.strokeList_[i];
    for (var j = 0; j < pointList.length; j++) {
      if (pointList[j].ttl > 0) {
        pointList[j].ttl -= GestureCanvasView.DEGRADATION_RATE;
        all_empty = false;
      }
    }

    // In the case where all points in the list are empty, dispose of the first.
    if (all_empty) {
      this.strokeList_.splice(i, 1);
      i--;
    }
  }
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
  this.ttl = 225;
};


});  // goog.scope
