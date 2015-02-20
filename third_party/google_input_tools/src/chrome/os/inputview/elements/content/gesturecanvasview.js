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
   * Drag events whose points should be drawn on the canvas.
   *
   * @private {!Array}
   */
  this.dragEventList_ = [];

  /** @private {!goog.async.Delay} */
  this.animator_ = new goog.async.Delay(this.animateGestureTrail_, 0, this);
};
var GestureCanvasView =
    i18n.input.chrome.inputview.elements.content.GestureCanvasView;
goog.inherits(GestureCanvasView, i18n.input.chrome.inputview.elements.Element);


/**
 * Draw the gesture trail.
 *
 * @private
 */
GestureCanvasView.prototype.draw_ = function() {
  // First, clear the canvas.
  this.drawingContext_.clearRect(
      0, 0, this.drawingCanvas_.width, this.drawingCanvas_.height);

  // Event positions come in relative to the top of the entire content area, so
  // grab the canvas offset in order to calculate the correct position to draw
  // the strokes.
  var offset = goog.style.getPageOffset(this.drawingCanvas_);

  // Iterate through all the points and draw them.
  for (var i = 1; i < this.dragEventList_.length; i++) {
    // TODO(stevet): The following is a basic implementation of the trail
    // rendering. This should be later be updated to be more efficient and
    // support effects like fading.
    var first = this.dragEventList_[i - 1];
    var second = this.dragEventList_[i];
    this.drawingContext_.beginPath();
    this.drawingContext_.strokeStyle = '#00B4CC';
    this.drawingContext_.fillStyle = 'none';
    this.drawingContext_.lineWidth = 8;
    this.drawingContext_.lineCap = 'round';
    this.drawingContext_.lineJoin = 'round';
    this.drawingContext_.moveTo(first.x - offset.x, first.y - offset.y);
    this.drawingContext_.lineTo(second.x - offset.x, second.y - offset.y);
    this.drawingContext_.stroke();
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
 * Add a new point to the collection of points, and refresh the view.
 *
 * @param {!i18n.input.chrome.inputview.events.DragEvent} e Drag event to draw.
 */
GestureCanvasView.prototype.addPointAndDraw = function(e) {
  // Add to the collection.
  this.dragEventList_.push(e);

  // Refresh the view.
  this.draw_();
};


/**
 * Clear the view.
 */
GestureCanvasView.prototype.clear = function() {
  this.dragEventList_ = [];
  this.draw_();
};


/**
 * The gesture trail animation function.
 *
 * @private
 */
GestureCanvasView.prototype.animateGestureTrail_ = function() {
  // TODO(stevet): Implement the gesture trail animation here.
};
});  // goog.scope
