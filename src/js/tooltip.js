// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Creates a tooltip controller for the entire document.
 * @constructor
 */
camera.Tooltip = function() {
  /**
   * @type {camera.util.StyleEffect}
   * @private
   */
  this.effect_ = new camera.util.StyleEffect((args, callback) => {
    this.setVisibility_(args.element, args.visibility, callback);
  });

  // No more properties. Freeze the object.
  Object.freeze(this);
};

/**
 * Initializes the tooltip.
 */
camera.Tooltip.initialize = function() {
  // Add tooltip handlers to every element with the i18n-label attribute.
  var instance = new camera.Tooltip();
  var selectors = document.querySelectorAll('*[i18n-label]');
  for (var index = 0; index < selectors.length; index++) {
    var element = selectors[index];
    var handler = instance.show_.bind(instance, element);
    element.addEventListener('mouseover', handler);
    element.addEventListener('focus', handler);
  }
};

/**
 * Positions the tooltip on the screen and toggles its visibility.
 * @param {HTMLElement} element Element to be the tooltip positioned to.
 * @param {boolean} visible True for visible, false for hidden.
 * @param {function()} callback Completion callback for changing visibility.
 * @private
 */
camera.Tooltip.prototype.setVisibility_ = function(
    element, visible, callback) {
  // Hide the tooltip.
  var tooltip = document.querySelector('#tooltip');
  if (!visible) {
    tooltip.classList.remove('visible');
    callback();  // No animation, finish immediately.
    return;
  }

  // Show the tooltip.
  const [edgeMargin, elementMargin] = [5, 8];
  var rect = element.getBoundingClientRect();
  var tooltipTop = rect.top - tooltip.offsetHeight - elementMargin;
  if (tooltipTop < edgeMargin) {
    tooltipTop = rect.bottom + elementMargin;
  }
  tooltip.style.top = tooltipTop + 'px';

  // Center over the element, but avoid touching edges.
  var elementCenter = rect.left + element.offsetWidth / 2;
  var left = Math.min(
      Math.max(elementCenter - tooltip.clientWidth / 2, edgeMargin),
      document.body.offsetWidth - tooltip.offsetWidth - edgeMargin);
  tooltip.style.left = Math.round(left) + 'px';

  // Show the tooltip element.
  tooltip.classList.add('visible');
  camera.util.waitForTransitionCompletion(tooltip, 1000, callback);
};

/**
 * Shows a tooltip over the element.
 * @param {HTMLElement} element Element to be shown.
 * @private
 */
camera.Tooltip.prototype.show_ = function(element) {
  this.effect_.invoke(false, () => {});
  document.querySelector('#tooltip').textContent = chrome.i18n.getMessage(
      element.getAttribute('i18n-label'));

  var hide = () => {
    this.effect_.invoke({
      element: element,
      visibility: false
    }, () => {});
    element.removeEventListener('mouseout', hide);
    element.removeEventListener('click', hide);
    element.removeEventListener('blur', hide);
  };
  element.addEventListener('mouseout', hide);
  element.addEventListener('click', hide);
  element.addEventListener('blur', hide);

  // Show the tooltip after delay.
  this.effect_.invoke({
    element: element,
    visibility: true
  }, () => {}, 1500);
};
