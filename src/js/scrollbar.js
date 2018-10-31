// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Creates a scroll bar for a scrollable area.
 *
 * @param {camera.util.SmoothScroller} scoller Scroller for the scrollable
 *     element.
 * @constructor
 */
camera.ScrollBar = function(scroller) {
  /**
   * @type {camera.util.SmoothScroller}
   * @protected
   */
  this.scroller = scroller;

  /**
   * @type {?number}
   * @private
   */
  this.thumbLastScreenPosition_ = null;

  /**
   * @type {?number}
   * @private
   */
  this.domChangedTimer_ = null;

  /**
   * The scroll bar's track.
   * @type {HTMLElement}
   * @protected
   */
  this.track = document.createElement('div');

  /**
   * The scroll bar's thumb.
   * @type {HTMLElement}
   * @protected
   */
  this.thumb = document.createElement('div');

  /**
   * @type {boolean}
   * @private
   */
  this.thumbHidden_ = false;

  /**
   * @type {MutationObserver}
   * @private
   */
  this.domObserver_ = null;

  // End of properties. Seal the object.
  Object.seal(this);

  // Initialize the scroll bar's elements.
  this.track.classList.add('scrollbar-track');
  this.thumb.classList.add('scrollbar-thumb');

  this.scroller.element.appendChild(this.track);
  this.track.appendChild(this.thumb);

  // Add event handlers.
  this.thumb.addEventListener('mousedown',
                              this.onThumbPressed_.bind(this));
  window.addEventListener('mouseup', this.onMouseUp_.bind(this));
  window.addEventListener('mousemove', this.onMouseMove_.bind(this));

  this.scroller.element.addEventListener('scroll', this.onScroll_.bind(this));
  this.domObserver_ = new MutationObserver(this.onDomChanged_.bind(this));
  this.domObserver_.observe(
      this.scroller.element, {subtree: true, attributes: true});

  this.redraw_();
};

/**
 * Sets the scrollbar thumb hidden even for a scrollable area.
 * @param {boolean} hidden True if hides the thumb, false otherwise.
 */
camera.ScrollBar.prototype.setThumbHidden = function(hidden) {
  this.thumbHidden_ = hidden;
  this.redraw_();
};

/**
 * Scroll handler.
 * @private
 */
camera.ScrollBar.prototype.onScroll_ = function() {
  this.redraw_();
};

/**
 * Pressing on the scroll bar's button handler.
 * @param {Event} event Pressing event.
 * @private
 */
camera.ScrollBar.prototype.onThumbPressed_ = function(event) {
  this.thumbLastScreenPosition_ = this.getScreenPosition(event);
  this.thumb.classList.add('pressed');

  event.stopPropagation();
  event.preventDefault();
};

/**
 * Releasing the button handler. Note, that it may not be called when releasing
 * outside of the window. Therefore this is also called from onMouseMove_.
 *
 * @param {Event} event Mouse event.
 * @private
 */
camera.ScrollBar.prototype.onMouseUp_ = function(event) {
  this.thumbLastScreenPosition_ = null;
  this.thumb.classList.remove('pressed');
};

/**
 * @return {number} Total client size in pixels.
 * @protected
 */
camera.ScrollBar.prototype.getClientTotal = function() {
  throw new Error('Not implemented');
};

/**
 * @return {number} Total scroll size in pixels.
 * @protected
 */
camera.ScrollBar.prototype.getScrollTotal = function() {
  throw new Error('Not implemented');
};

/**
 * @param {Event} event Event.
 * @return {number} Total client position for the event in pixels.
 * @protected
 */
camera.ScrollBar.prototype.getClientPosition = function(event) {
  throw new Error('Not implemented');
};

/**
 * @param {Event} event Event.
 * @return {number} Total screen position for the event in pixels.
 * @protected
 */
camera.ScrollBar.prototype.getScreenPosition = function(event) {
  throw new Error('Not implemented');
};

/**
 * @return {number} Scroll position in pixels.
 * @protected
 */
camera.ScrollBar.prototype.getScrollPosition = function() {
  throw new Error('Not implemented');
};

/**
 * Sets the scroll position.
 * @param {number} value Position in pixels.
 * @protected
 */
camera.ScrollBar.prototype.setScrollPosition = function(value) {
  throw new Error('Not implemented');
};

/**
 * Sets geometry of the scroll bar's thumb.
 *
 * @param {number} position Position of the thumb in pixels.
 * @param {number} size Size of the thumb in pixels.
 * @protected
 */
camera.ScrollBar.prototype.setThumbGeometry = function(position, size) {
  throw new Error('Not implemented');
};

/**
 * Mouse move handler. Updates the scroll position.
 * @param {Event} event Mouse event.
 * @private
 */
camera.ScrollBar.prototype.onMouseMove_ = function(event) {
  if (!this.thumbLastScreenPosition_)
    return;

  if (!event.which) {
    this.onMouseUp_(event);
    return;
  }

  var clientTotal = this.getClientTotal();
  var scrollTotal = this.getScrollTotal();

  var scrollPosition = this.getScrollPosition() +
      (this.getScreenPosition(event) -
       this.thumbLastScreenPosition_) * (scrollTotal / clientTotal);

  this.setScrollPosition(scrollPosition);
  this.redraw_();

  this.thumbLastScreenPosition_ = this.getScreenPosition(event);
};

/**
 * Handles changed in Dom by redrawing the scroll bar. Ignores consecutive
 * calls.
 * @private
 */
camera.ScrollBar.prototype.onDomChanged_ = function() {
  if (this.domChangedTimer_) {
    clearTimeout(this.domChangedTimer_);
    this.domChangedTimer_ = null;
  }
  this.domChangedTimer_ = setTimeout(function() {
    this.redraw_();
    this.domChangedTimer_ = null;
  }.bind(this), 50);
};

/**
 * Resize handler to update the thumb size/position by redrawing the scroll bar.
 */
camera.ScrollBar.prototype.onResize = function() {
  this.redraw_();
};

/**
 * Redraws the scroll bar.
 * @private
 */
camera.ScrollBar.prototype.redraw_ = function() {
  var clientTotal = this.getClientTotal();
  var scrollTotal = this.getScrollTotal();

  this.thumb.hidden = this.thumbHidden_ || scrollTotal <= clientTotal;

  var thumbSize = Math.max(50, clientTotal / scrollTotal * clientTotal);
  var thumbPosition = this.getScrollPosition() / (scrollTotal - clientTotal) *
      (clientTotal - thumbSize);

  this.setThumbGeometry(thumbPosition, thumbSize);
};

/**
 * Creates a horizontal scroll bar.
 *
 * @param {camera.util.SmoothScroller} scoller Scroller for the scrollable
 *     element.
 * @constructor
 * @extends {camera.ScrollBar}
 */
camera.HorizontalScrollBar = function(scroller) {
  camera.ScrollBar.call(this, scroller);
  this.track.classList.add('scrollbar-track-horizontal');
};

camera.HorizontalScrollBar.prototype = {
  __proto__: camera.ScrollBar.prototype,
};

/**
 * @override
 */
camera.HorizontalScrollBar.prototype.getClientTotal = function() {
  return this.scroller.clientWidth;
};

/**
 * @override
 */
camera.HorizontalScrollBar.prototype.getScrollTotal = function() {
  return this.scroller.scrollWidth;
};

/**
 * @override
 */
camera.HorizontalScrollBar.prototype.getClientPosition = function(event) {
  return event.clientX;
};

/**
 * @override
 */
camera.HorizontalScrollBar.prototype.getScreenPosition = function(event) {
  return event.screenX;
};

/**
 * @override
 */
camera.HorizontalScrollBar.prototype.getScrollPosition = function() {
  return this.scroller.scrollLeft;
};

/**
 * @override
 */
camera.HorizontalScrollBar.prototype.setScrollPosition = function(value) {
  this.scroller.scrollTo(value,
                         this.scroller.element.scrollTop,
                         camera.util.SmoothScroller.Mode.INSTANT);
};

/**
 * @override
 */
camera.HorizontalScrollBar.prototype.setThumbGeometry = function(
    position, size) {
  this.thumb.style.left = position + 'px';
  this.thumb.style.width = size + 'px';
};

/**
 * Creates a vertical scroll bar.
 *
 * @param {camera.util.SmoothScroller} scoller Scroller for the scrollable
 *     element.
 * @constructor
 * @extends {camera.ScrollBar}
 */
camera.VerticalScrollBar = function(scroller) {
  camera.ScrollBar.call(this, scroller);
  this.track.classList.add('scrollbar-track-vertical');
};

camera.VerticalScrollBar.prototype = {
  __proto__: camera.ScrollBar.prototype,
};

/**
 * @override
 */
camera.VerticalScrollBar.prototype.getClientTotal = function() {
  return this.scroller.clientHeight;
};

/**
 * @override
 */
camera.VerticalScrollBar.prototype.getScrollTotal = function() {
  return this.scroller.scrollHeight;
};

/**
 * @override
 */
camera.VerticalScrollBar.prototype.getClientPosition = function(event) {
  return event.clientY;
};

/**
 * @override
 */
camera.VerticalScrollBar.prototype.getScreenPosition = function(event) {
  return event.screenY;
};

/**
 * @override
 */
camera.VerticalScrollBar.prototype.getScrollPosition = function() {
  return this.scroller.scrollTop;
};

/**
 * @override
 */
camera.VerticalScrollBar.prototype.setScrollPosition = function(value) {
  this.scroller.scrollTo(this.scroller.element.scrollTop,
                         value,
                         camera.util.SmoothScroller.Mode.INSTANT);
};

/**
 * @override
 */
camera.VerticalScrollBar.prototype.setThumbGeometry = function(
    position, size) {
  this.thumb.style.top = position + 'px';
  this.thumb.style.height = size + 'px';
};

