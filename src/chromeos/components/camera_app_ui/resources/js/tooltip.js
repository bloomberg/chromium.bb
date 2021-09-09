// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from './dom.js';

/**
 * Wrapper element that shows tooltip.
 * @type {?HTMLElement}
 */
let wrapper = null;

/**
 * Hovered element whose tooltip to be shown.
 * @type {?HTMLElement}
 */
let hovered = null;

/**
 * Name of event triggered for positioning tooltip.
 * @const {string}
 */
export const TOOLTIP_POSITION_EVENT_NAME = 'tooltipposition';

/**
 * Positions tooltip relative to UI.
 * @param {!DOMRectReadOnly} rect UI's reference region.
 */
export function position(rect) {
  const [edgeMargin, elementMargin] = [5, 8];
  let tooltipTop = rect.top - wrapper.offsetHeight - elementMargin;
  if (tooltipTop < edgeMargin) {
    tooltipTop = rect.bottom + elementMargin;
  }
  wrapper.style.top = tooltipTop + 'px';

  // Center over the hovered element but avoid touching edges.
  const hoveredCenter = rect.left + rect.width / 2;
  const left = Math.min(
      Math.max(hoveredCenter - wrapper.clientWidth / 2, edgeMargin),
      document.body.offsetWidth - wrapper.offsetWidth - edgeMargin);
  wrapper.style.left = Math.round(left) + 'px';
}

/**
 * Hides the shown tooltip if any.
 */
export function hide() {
  if (hovered) {
    hovered = null;
    wrapper.textContent = '';
    wrapper.classList.remove('visible');
  }
}

/**
 * Shows a tooltip over the hovered element.
 * @param {!HTMLElement} element Hovered element whose tooltip to be shown.
 */
function show(element) {
  hide();
  let message = element.getAttribute('aria-label');
  if (element instanceof HTMLInputElement) {
    if (element.hasAttribute('tooltip-true') && element.checked) {
      message = element.getAttribute('tooltip-true');
    }
    if (element.hasAttribute('tooltip-false') && !element.checked) {
      message = element.getAttribute('tooltip-false');
    }
  }
  wrapper.textContent = message;
  hovered = element;
  const positionEvent = new CustomEvent(
      TOOLTIP_POSITION_EVENT_NAME, {cancelable: true, target: hovered});
  const doDefault = hovered.dispatchEvent(positionEvent);
  if (doDefault) {
    position(hovered.getBoundingClientRect());
  }
  wrapper.classList.add('visible');
}

/**
 * Sets up tooltips for elements.
 * @param {!NodeListOf<!HTMLElement>} elements Elements whose tooltips to
 *     be shown.
 * @return {!NodeListOf<!HTMLElement>} Elements whose tooltips have been
 *     set up.
 */
export function setup(elements) {
  wrapper = dom.get('#tooltip', HTMLElement);
  elements.forEach((el) => {
    const handler = () => {
      // Handler hides tooltip only when it's for the element.
      if (el === hovered) {
        hide();
      }
    };
    el.addEventListener('mouseout', handler);
    el.addEventListener('click', handler);
    el.addEventListener('mouseover', () => show(el));
  });
  return elements;
}
