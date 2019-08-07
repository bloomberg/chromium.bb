// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   width: number,
 *   height: number,
 *   pageDimensions: Array<ViewportRect>,
 * }}
 */
let DocumentDimensions;

/**
 * @typedef {{
 *   x: number,
 *   y: number
 * }}
 */
let Point;

/**
 * @typedef {{
 *   width: number,
 *   height: number,
 * }}
 */
let Size;

/**
 * @typedef {{
 *   x: number,
 *   y: number,
 *   width: number,
 *   height: number,
 * }}
 */
let ViewportRect;

/**
 * @interface
 */
class Viewport {
  /**
   * Returns the document dimensions.
   *
   * @return {!Size} A dictionary with the 'width'/'height' of the document.
   */
  getDocumentDimensions() {}

  /**
   * @return {!Point} the scroll position of the viewport.
   */
  get position() {}

  /**
   * @return {!Size} the size of the viewport excluding scrollbars.
   */
  get size() {}

  /**
   * @return {number} the zoom level of the viewport.
   */
  get zoom() {}

  /**
   * Sets the zoom to the given zoom level.
   *
   * @param {number} newZoom the zoom level to zoom to.
   */
  setZoom(newZoom) {}

  /**
   * Gets notified of the browser zoom changing separately from the
   * internal zoom.
   *
   * @param {number} oldBrowserZoom the previous value of the browser zoom.
   */
  updateZoomFromBrowserChange(oldBrowserZoom) {}

  /**
   * @param {!Point} point
   * @return {boolean} Whether |point| (in screen coordinates) is inside a page
   */
  isPointInsidePage(point) {}
}

/**
 * Enumeration of pinch states.
 * This should match PinchPhase enum in pdf/out_of_process_instance.h
 * @enum {number}
 */
Viewport.PinchPhase = {
  PINCH_NONE: 0,
  PINCH_START: 1,
  PINCH_UPDATE_ZOOM_OUT: 2,
  PINCH_UPDATE_ZOOM_IN: 3,
  PINCH_END: 4
};

/**
 * The increment to scroll a page by in pixels when up/down/left/right arrow
 * keys are pressed. Usually we just let the browser handle scrolling on the
 * window when these keys are pressed but in certain cases we need to simulate
 * these events.
 */
Viewport.SCROLL_INCREMENT = 40;

/**
 * Predefined zoom factors to be used when zooming in/out. These are in
 * ascending order. This should match the lists in
 * components/ui/zoom/page_zoom_constants.h and
 * chrome/browser/resources/settings/appearance_page/appearance_page.js
 */
Viewport.ZOOM_FACTORS = [
  0.25, 1 / 3, 0.5, 2 / 3, 0.75, 0.8, 0.9, 1, 1.1, 1.25, 1.5, 1.75, 2, 2.5, 3,
  4, 5
];

/**
 * The minimum and maximum range to be used to clip zoom factor.
 */
Viewport.ZOOM_FACTOR_RANGE = {
  min: Viewport.ZOOM_FACTORS[0],
  max: Viewport.ZOOM_FACTORS[Viewport.ZOOM_FACTORS.length - 1]
};

/**
 * The width of the page shadow around pages in pixels.
 */
Viewport.PAGE_SHADOW = {
  top: 3,
  bottom: 7,
  left: 5,
  right: 5
};
