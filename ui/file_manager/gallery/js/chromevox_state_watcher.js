// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ChromeVox State Watcher.
 * @constructor
 * @extends {cr.EventTarget}
 * @struct
 */
function ChromeVoxStateWatcher() {
  cr.EventTarget.call(this);

  /**
   * @private {boolean}
   */
  this.navigatingWithChromeVox_ = false;

  document.addEventListener('keydown', this.onKeydown_.bind(this));
  document.addEventListener('keyup', this.onKeyup_.bind(this));
}

ChromeVoxStateWatcher.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Handles keydown event.
 * @param {!Event} event
 * @private
 */
ChromeVoxStateWatcher.prototype.onKeydown_ = function(event) {
  // If user presses Shift+Search, we consider it as the begin of ChromeVox
  // navigation. When user is navigating with ChromeVox, no key events are
  // dispatched to Gallery.
  if (!this.navigatingWithChromeVox_ && (event.shiftKey && event.metaKey)) {
    this.navigatingWithChromeVox_ = true;
    this.dispatchEvent(new Event('chromevox-navigation-begin'));
  }
};

/**
 * Handles keyup event.
 * @param {!Event} event
 * @private
 */
ChromeVoxStateWatcher.prototype.onKeyup_ = function(event) {
  // When user has reached to the end of DOM elements, ChromeVox dispatches
  // below key up events. Since they don't mean the end of ChromeVox navigation,
  // we should ignore them.
  if (event.shiftKey &&
      (event.keyCode === 33 /* Page up */ ||
       event.keyCode === 34 /* Page down */)) {
    return;
  }

  // When user releases Shift+Search, it means the end of ChromeVox navigation.
  // Since we don't get any key event (except above ones) during ChromeVox
  // navigation, we can consider any key up event as an end of ChromeVox
  // navigation.
  if (this.navigatingWithChromeVox_) {
    this.navigatingWithChromeVox_ = false;
    this.dispatchEvent(new Event('chromevox-navigation-end'));
  }
};
