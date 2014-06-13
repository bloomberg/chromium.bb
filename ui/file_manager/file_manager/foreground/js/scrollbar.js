// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Creates a new scroll bar element.
 * @extends {HTMLDivElement}
 * @constructor
 */
var ScrollBar = cr.ui.define('div');

/**
 * Mode of the scrollbar. As for now, only vertical scrollbars are supported.
 * @type {number}
 */
ScrollBar.Mode = {
  VERTICAL: 0,
  HORIZONTAL: 1
};

ScrollBar.prototype = {
  set mode(value) {
    this.mode_ = value;
    if (this.mode_ == ScrollBar.Mode.VERTICAL) {
      this.classList.remove('scrollbar-horizontal');
      this.classList.add('scrollbar-vertical');
    } else {
      this.classList.remove('scrollbar-vertical');
      this.classList.add('scrollbar-horizontal');
    }
    this.redraw_();
  },
  get mode() {
    return this.mode_;
  }
};

/**
 * Inherits after HTMLDivElement.
 */
ScrollBar.prototype.__proto__ = HTMLDivElement.prototype;

/**
 * Initializes the DOM structure of the scrollbar.
 */
ScrollBar.prototype.decorate = function() {
  this.classList.add('scrollbar');
  this.button_ = util.createChild(this, 'scrollbar-button', 'div');
  this.mode = ScrollBar.Mode.VERTICAL;
  this.idleTimerId_ = 0;

  this.button_.addEventListener('mousedown',
                                this.onButtonPressed_.bind(this));
  window.addEventListener('mouseup', this.onMouseUp_.bind(this));
  window.addEventListener('mousemove', this.onMouseMove_.bind(this));
};

/**
 * Initialize a scrollbar.
 *
 * @param {Element} parent Parent element, must have a relative or absolute
 *     positioning.
 * @param {Element=} opt_scrollableArea Element with scrollable contents.
 *     If not passed, then call attachToView manually when the scrollable
 *     element becomes available.
 */
ScrollBar.prototype.initialize = function(parent, opt_scrollableArea) {
  parent.appendChild(this);
  if (opt_scrollableArea)
    this.attachToView(opt_scrollableArea);
};

/**
 * Attaches the scrollbar to a scrollable element and attaches handlers.
 * @param {Element} view Scrollable element.
 */
ScrollBar.prototype.attachToView = function(view) {
  this.view_ = view;
  this.view_.addEventListener('scroll', this.onScroll_.bind(this));
  this.view_.addEventListener('relayout', this.onRelayout_.bind(this));
  this.domObserver_ = new MutationObserver(this.onDomChanged_.bind(this));
  this.domObserver_.observe(this.view_, {subtree: true, attributes: true});
  this.onRelayout_();
};

/**
 * Scroll handler.
 * @private
 */
ScrollBar.prototype.onScroll_ = function() {
  this.scrollTop_ = this.view_.scrollTop;
  this.redraw_();

  // Add class 'scrolling' to scrollbar to make it visible while scrolling.
  this.button_.classList.add('scrolling');

  // Set timer to remove class 'scrolling' after scrolling becomes idle.
  if (this.idleTimerId_)
    clearTimeout(this.idleTimerId_);
  this.idleTimerId_ = setTimeout(function() {
    this.idleTimerId_ = 0;
    this.button_.classList.remove('scrolling');
  }.bind(this), 1000);
};

/**
 * Relayout handler.
 * @private
 */
ScrollBar.prototype.onRelayout_ = function() {
  this.scrollHeight_ = this.view_.scrollHeight;
  this.clientHeight_ = this.view_.clientHeight;
  this.offsetTop_ = this.view_.offsetTop;
  this.scrollTop_ = this.view_.scrollTop;
  this.redraw_();
};

/**
 * Pressing on the scrollbar's button handler.
 *
 * @param {Event} event Pressing event.
 * @private
 */
ScrollBar.prototype.onButtonPressed_ = function(event) {
  this.buttonPressed_ = true;
  this.buttonPressedEvent_ = event;
  this.buttonPressedPosition_ = this.button_.offsetTop - this.view_.offsetTop;
  this.button_.classList.add('pressed');

  event.preventDefault();
};

/**
 * Releasing the button handler. Note, that it may not be called when releasing
 * outside of the window. Therefore this is also called from onMouseMove_.
 *
 * @param {Event} event Mouse event.
 * @private
 */
ScrollBar.prototype.onMouseUp_ = function(event) {
  this.buttonPressed_ = false;
  this.button_.classList.remove('pressed');
};

/**
 * Mouse move handler. Updates the scroll position.
 *
 * @param {Event} event Mouse event.
 * @private
 */
ScrollBar.prototype.onMouseMove_ = function(event) {
  if (!this.buttonPressed_)
    return;
  if (!event.which) {
    this.onMouseUp_(event);
    return;
  }
  var clientSize = this.getClientHeight();
  var totalSize = this.getTotalHeight();
  // TODO(hirono): Fix the geometric calculation.  crbug.com/253779
  var buttonSize = Math.max(50, clientSize / totalSize * clientSize);
  var buttonPosition = this.buttonPressedPosition_ +
      (event.screenY - this.buttonPressedEvent_.screenY);
  // Ensures the scrollbar is in the view.
  buttonPosition =
      Math.max(0, Math.min(buttonPosition, clientSize - buttonSize));
  var scrollPosition;
  if (clientSize > buttonSize) {
    scrollPosition = Math.max(totalSize - clientSize, 0) *
        buttonPosition / (clientSize - buttonSize);
  } else {
    scrollPosition = 0;
  }

  this.scrollTop_ = scrollPosition;
  this.view_.scrollTop = scrollPosition;
  this.redraw_();
};

/**
 * Handles changed in Dom by redrawing the scrollbar. Ignores consecutive calls.
 * @private
 */
ScrollBar.prototype.onDomChanged_ = function() {
  if (this.domChangedTimer_) {
    clearTimeout(this.domChangedTimer_);
    this.domChangedTimer_ = null;
  }
  this.domChangedTimer_ = setTimeout(function() {
    this.onRelayout_();
    this.domChangedTimer_ = null;
  }.bind(this), 50);
};

/**
 * Redraws the scrollbar.
 * @private
 */
ScrollBar.prototype.redraw_ = function() {
  if (!this.view_)
    return;

  var clientSize = this.getClientHeight();
  var clientTop = this.offsetTop_;
  var scrollPosition = this.scrollTop_;
  var totalSize = this.getTotalHeight();
  var hidden = totalSize <= clientSize;

  var buttonSize = Math.max(50, clientSize / totalSize * clientSize);
  var buttonPosition;
  if (clientSize - buttonSize > 0) {
    buttonPosition = scrollPosition / (totalSize - clientSize) *
        (clientSize - buttonSize);
  } else {
    buttonPosition = 0;
  }
  var buttonTop = buttonPosition + clientTop;

  var time = Date.now();
  if (this.hidden != hidden ||
      this.lastButtonTop_ != buttonTop ||
      this.lastButtonSize_ != buttonSize) {
    requestAnimationFrame(function() {
      this.hidden = hidden;
      this.button_.style.top = buttonTop + 'px';
      this.button_.style.height = buttonSize + 'px';
    }.bind(this));
  }

  this.lastButtonTop_ = buttonTop;
  this.lastButtonSize_ = buttonSize;
};

/**
 * Returns the viewport height of the view.
 * @return {number} The viewport height of the view in px.
 * @protected
 */
ScrollBar.prototype.getClientHeight = function() {
  return this.clientHeight_;
};

/**
 * Returns the total height of the view.
 * @return {number} The total height of the view in px.
 * @protected
 */
ScrollBar.prototype.getTotalHeight = function() {
  return this.scrollHeight_;
};

/**
 * Creates a new scroll bar for elements in the main panel.
 * @extends {ScrollBar}
 * @constructor
 */
var MainPanelScrollBar = cr.ui.define('div');

/**
 * Inherits after ScrollBar.
 */
MainPanelScrollBar.prototype.__proto__ = ScrollBar.prototype;

/** @override */
MainPanelScrollBar.prototype.decorate = function() {
  ScrollBar.prototype.decorate.call(this);

  /**
   * Margin for the transparent preview panel at the bottom.
   * @type {number}
   * @private
   */
  this.bottomMarginForPanel_ = 0;
};

/**
 * GReturns the viewport height of the view, considering the preview panel.
 *
 * @return {number} The viewport height of the view in px.
 * @override
 * @protected
 */
MainPanelScrollBar.prototype.getClientHeight = function() {
  return this.clientHeight_ - this.bottomMarginForPanel_;
};

/**
 * Returns the total height of the view, considering the preview panel.
 *
 * @return {number} The total height of the view in px.
 * @override
 * @protected
 */
MainPanelScrollBar.prototype.getTotalHeight = function() {
  return this.scrollHeight_ - this.bottomMarginForPanel_;
};

/**
 * Sets the bottom margin height of the view for the transparent preview panel.
 * @param {number} margin Margin to be set in px.
 */
MainPanelScrollBar.prototype.setBottomMarginForPanel = function(margin) {
  this.bottomMarginForPanel_ = margin;
};
