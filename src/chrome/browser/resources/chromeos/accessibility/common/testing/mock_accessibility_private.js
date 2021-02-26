// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * A mock AccessibilityPrivate API for tests.
 */
var MockAccessibilityPrivate = {
  FocusType: {
    SOLID: 'solid',
  },

  /** @private {function<number, number>} */
  boundsListener_: null,

  /** @private {!chrome.accessibilityPrivate.ScreenRect} */
  scrollableBounds_: {},

  /** @private {!Array<!chrome.accessibilityPrivate.ScreenRect>} */
  focusRingRects_: [],
  handleScrollableBoundsForPointFoundCallback_: null,
  moveMagnifierToRectCallback_: null,

  // Methods from AccessibilityPrivate API. //

  onScrollableBoundsForPointRequested: {
    /**
     * Adds a listener to onScrollableBoundsForPointRequested.
     * @param {function<number, number>} listener
     */
    addListener: (listener) => {
      MockAccessibilityPrivate.boundsListener_ = listener;
    },

    /**
     * Removes the listener.
     */
    removeListener: (listener) => {
      if (MockAccessibilityPrivate.boundsListener_ === listener) {
        MockAccessibilityPrivate.boundsListener_ = null;
      }
    }
  },

  /**
   * Called when AccessibilityCommon finds scrollable bounds at a point.
   * @param {!chrome.accessibilityPrivate.ScreenRect} bounds
   */
  handleScrollableBoundsForPointFound: (bounds) => {
    MockAccessibilityPrivate.scrollableBounds_ = bounds;
    MockAccessibilityPrivate.handleScrollableBoundsForPointFoundCallback_();
  },

  /**
   * Called when AccessibilityCommon wants to move the magnifier viewport to
   * include a specific rect.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   */
  moveMagnifierToRect: (rect) => {
    if (MockAccessibilityPrivate.moveMagnifierToRectCallback_) {
      MockAccessibilityPrivate.moveMagnifierToRectCallback_(rect);
    }
  },

  /**
   * Called when AccessibilityCommon wants to set the focus rings. We can assume
   * that it is only setting one set of rings at a time, and safely extract
   * focusRingInfos[0].rects.
   * @param {!Array<!FocusRingInfo>} focusRingInfos
   */
  setFocusRings: (focusRingInfos) => {
    MockAccessibilityPrivate.focusRingRects_ = focusRingInfos[0].rects;
  },

  // Methods for testing. //

  /**
   * Called to get the AccessibilityCommon extension to use the Automation API
   * to find the scrollable bounds at a point. In Automatic Clicks, this would
   * actually be initiated by ash/autoclick/autoclick_controller calling the
   * AccessibilityPrivate API call.
   * When the bounds are found, handleScrollableBoundsForPointFoundCallback will
   * be called to inform the test that work is complete.
   * @param {number} x
   * @param {number} y
   * @param {!function<>} handleScrollableBoundsForPointFoundCallback
   */
  callOnScrollableBoundsForPointRequested:
      (x, y, handleScrollableBoundsForPointFoundCallback) => {
        MockAccessibilityPrivate.handleScrollableBoundsForPointFoundCallback_ =
            handleScrollableBoundsForPointFoundCallback;
        MockAccessibilityPrivate.boundsListener_(x, y);
      },

  /**
   * Called to register a stubbed callback for moveMagnifierToRect.
   * When magnifier identifies a desired rect to move the viewport to,
   * moveMagnifierToRectCallback will be called with that desired rect.
   * @param {!function<>} moveMagnifierToRectCallback
   */
  registerMoveMagnifierToRectCallback: (moveMagnifierToRectCallback) => {
    MockAccessibilityPrivate.moveMagnifierToRectCallback_ =
        moveMagnifierToRectCallback;
  },

  /**
   * Gets the scrollable bounds which were found by the AccessibilityCommon
   * extension.
   * @return {Array<!chrome.AccessibilityPrivate.ScreenRect>}
   */
  getScrollableBounds: () => {
    return MockAccessibilityPrivate.scrollableBounds_;
  },

  /**
   * Gets the focus rings bounds which were set by the AccessibilityCommon
   * extension.
   * @return {Array<!chrome.AccessibilityPrivate.ScreenRect>}
   */
  getFocusRings: () => {
    return MockAccessibilityPrivate.focusRingRects_;
  },
};