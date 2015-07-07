// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controls showing and hising tooltips when hovering over tooltipable HTML
 * elements.
 * @param {!HTMLElement} tooltip
 * @param {!Array<!HTMLElement>} targets
 * @constructor
 * @struct
 */
function TooltipController(tooltip, targets) {
  /** @private {!HTMLElement} */
  this.tooltip_ = tooltip;

  /** @private {!HTMLElement} */
  this.tooltipLabel_ = queryRequiredElement(tooltip, '#tooltip-label');

  /** @private {!Array<!HTMLElement>} */
  this.targets_ = targets;

  /** @private {HTMLElement} */
  this.visibleTooltipTarget_ = null;

  /** @private {HTMLElement} */
  this.upcomingTooltipTarget_ = null;

  /** @private {number} */
  this.showTooltipTimerId_ = 0;

  /** @private {number} */
  this.hideTooltipTimerId_ = 0;

  this.targets_.forEach(function(target) {
    target.addEventListener('mouseover', this.onMouseOver_.bind(this, target));
    target.addEventListener('mouseout', this.onMouseOut_.bind(this, target));
    target.addEventListener('focus', this.onFocus_.bind(this, target));
    target.addEventListener('blur', this.onBlur_.bind(this, target));
  }.bind(this));

  document.body.addEventListener(
      'mousedown', this.onDocumentMouseDown_.bind(this));
}

/**
 * Delay for showing the tooltip in milliseconds.
 * @const {number}
 */
TooltipController.SHOW_TIMEOUT = 500;

/**
 * Delay for hiding the tooltip in milliseconds.
 * @const {number}
 */
TooltipController.HIDE_TIMEOUT = 250;

/**
 * @param {!HTMLElement} target
 * @private
 */
TooltipController.prototype.initShowingTooltip_ = function(target) {
  // Some tooltip is already visible.
  if (this.visibleTooltipTarget_) {
    if (this.hideTooltipTimerId_) {
      clearTimeout(this.hideTooltipTimerId_);
      this.hideTooltipTimerId_ = 0;
    }
  }

  if (this.visibleTooltipTarget_ === target)
    return;

  this.upcomingTooltipTarget_ = target;
  if (this.showTooltipTimerId_)
    clearTimeout(this.showTooltipTimerId_);
  this.showTooltipTimerId_ = setTimeout(
      this.showTooltip_.bind(this, target),
      this.visibleTooltipTarget_ ? 0 : TooltipController.SHOW_TIMEOUT);
};

/**
 * @param {!HTMLElement} target
 * @private
 */
TooltipController.prototype.initHidingTooltip_ = function(target) {
  // The tooltip is not visible.
  if (this.visibleTooltipTarget_ !== target) {
    if (this.upcomingTooltipTarget_ === target) {
      clearTimeout(this.showTooltipTimerId_);
      this.showTooltipTimerId_ = 0;
    }
    return;
  }

  if (this.hideTooltipTimerId_)
    clearTimeout(this.hideTooltipTimerId_);
  this.hideTooltipTimerId_ = setTimeout(
      this.hideTooltip_.bind(this), TooltipController.HIDE_TIMEOUT);
};

/**
 * @param {!HTMLElement} target
 * @private
 */
TooltipController.prototype.showTooltip_ = function(target) {
  if (this.showTooltipTimerId_) {
    clearTimeout(this.showTooltipTimerId_);
    this.showTooltipTimerId_ = 0;
  }

  this.visibleTooltipTarget_ = target;

  var label = target.getAttribute('aria-label');
  if (!label)
    return;

  this.tooltipLabel_.textContent = label;
  var rect = target.getBoundingClientRect();
  this.tooltip_.style.top = Math.round(rect.top) + rect.height + 'px';
  var left = rect.left + rect.width / 2 - this.tooltip_.offsetWidth / 2;
  if (left + this.tooltip_.offsetWidth > document.body.offsetWidth)
    left = document.body.offsetWidth - this.tooltip_.offsetWidth;
  this.tooltip_.style.left = Math.round(left) + 'px';
  this.tooltip_.setAttribute('visible', true);
};

/**
 * @private
 */
TooltipController.prototype.hideTooltip_ = function() {
  if (this.hideTooltipTimerId_) {
    clearTimeout(this.hideTooltipTimerId_);
    this.hideTooltipTimerId_ = 0;
  }

  this.visibleTooltipTarget_ = null;
  this.tooltip_.removeAttribute('visible');
};

/**
 * @param {Event} event
 * @private
 */
TooltipController.prototype.onMouseOver_ = function(target, event) {
  this.initShowingTooltip_(target);
};

/**
 * @param {Event} event
 * @private
 */
TooltipController.prototype.onMouseOut_ = function(target, event) {
  this.initHidingTooltip_(target);
};

/**
 * @param {Event} event
 * @private
 */
TooltipController.prototype.onFocus_ = function(target, event) {
  this.initShowingTooltip_(target);
};

/**
 * @param {Event} event
 * @private
 */
TooltipController.prototype.onBlur_ = function(target, event) {
  this.initHidingTooltip_(target);
};

/**
 * @param {Event} event
 * @private
 */
TooltipController.prototype.onDocumentMouseDown_ = function(event) {
  this.hideTooltip_();

  // Additionally prevent any scheduled tooltips from showing up.
  if (this.showTooltipTimerId_) {
    clearTimeout(this.showTooltipTimerId_);
    this.showTooltipTimerId_ = 0;
  }
};
