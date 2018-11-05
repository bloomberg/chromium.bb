// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for tooltip.
 */
cca.tooltip = cca.tooltip || {};

/**
 * Sets up the tooltip.
 */
cca.tooltip.setup = function() {
  // Add tooltip handlers to every element with the i18n-label attribute.
  document.querySelectorAll('*[i18n-label]').forEach((element) => {
    var handler = cca.tooltip.show_.bind(undefined, element);
    element.addEventListener('mouseover', handler);
    element.addEventListener('focus', handler);
  });
};

/**
 * Positions the tooltip by the element.
 * @param {HTMLElement} element Element for tooltip to be positioned to.
 * @private
 */
cca.tooltip.position_ = function(element) {
  var tooltip = document.querySelector('#tooltip');
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
};

/**
 * Shows a tooltip over the element.
 * @param {HTMLElement} element Element whose tooltip to be shown.
 * @private
 */
cca.tooltip.show_ = function(element) {
  var tooltip = document.querySelector('#tooltip');
  var hide = () => {
    tooltip.classList.remove('visible');
    element.removeEventListener('mouseout', hide);
    element.removeEventListener('click', hide);
    element.removeEventListener('blur', hide);
  };
  element.addEventListener('mouseout', hide);
  element.addEventListener('click', hide);
  element.addEventListener('blur', hide);

  tooltip.classList.remove('visible');
  tooltip.offsetWidth; // Force calculation to hide tooltip if shown.
  tooltip.textContent = chrome.i18n.getMessage(
      element.getAttribute('i18n-label'));
  cca.tooltip.position_(element);
  tooltip.classList.add('visible');
};
