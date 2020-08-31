// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {BrowserProxy} from './browser_proxy.js';
import {skColorToRgba} from './utils.js';

/**
 * @fileoverview The background manager brokers access to background related
 * DOM elements. The reason for this abstraction is that the these elements are
 * not owned by any custom elements (this is done so that the aforementioned DOM
 * elements load faster at startup).
 *
 * The background manager expects an iframe with ID 'backgroundImage' to be
 * present in the DOM. It will use that element to set the background image URL.
 */

/**
 * Installs a listener for background image load times and manages a
 * |PromiseResolver| that resolves to the captured load time.
 */
class LoadTimeResolver {
  /** @param {string} url */
  constructor(url) {
    /** @private {!PromiseResolver<number>} */
    this.resolver_ = new PromiseResolver();
    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();
    this.eventTracker_.add(window, 'message', ({data}) => {
      if (data.frameType === 'background-image' &&
          data.messageType === 'loaded' && url === data.url) {
        this.resolve_(data.time);
      }
    });
  }

  /** @return {!Promise<number>} */
  get promise() {
    return this.resolver_.promise;
  }

  reject() {
    this.resolver_.reject();
    this.eventTracker_.removeAll();
  }

  /** @param {number} loadTime */
  resolve_(loadTime) {
    this.resolver_.resolve(loadTime);
    this.eventTracker_.removeAll();
  }
}

export class BackgroundManager {
  constructor() {
    /** @private {Element} */
    this.backgroundImage_ = document.body.querySelector('#backgroundImage');
    /** @private {LoadTimeResolver} */
    this.loadTimeResolver_ = null;
  }

  /**
   * Sets whether the background image should be shown.
   * @param {boolean} show True, if the background image should be shown.
   */
  setShowBackgroundImage(show) {
    document.body.toggleAttribute('show-background-image', show);
  }

  /**
   * Sets the background color.
   * @param {skia.mojom.SkColor} color The background color.
   */
  setBackgroundColor(color) {
    document.body.style.backgroundColor = skColorToRgba(color);
  }

  /**
   * Sets the background image.
   * @param {!newTabPage.mojom.BackgroundImage} image The background image.
   */
  setBackgroundImage(image) {
    const url =
        new URL('chrome-untrusted://new-tab-page/custom_background_image');
    url.searchParams.append('url', image.url.url);
    if (image.url2x) {
      url.searchParams.append('url2x', image.url2x.url);
    }
    if (image.size) {
      url.searchParams.append('size', image.size);
    }
    if (image.repeatX) {
      url.searchParams.append('repeatX', image.repeatX);
    }
    if (image.repeatY) {
      url.searchParams.append('repeatY', image.repeatY);
    }
    if (image.positionX) {
      url.searchParams.append('positionX', image.positionX);
    }
    if (image.positionY) {
      url.searchParams.append('positionY', image.positionY);
    }
    if (url.href === this.backgroundImage_.src) {
      return;
    }
    if (this.loadTimeResolver_) {
      this.loadTimeResolver_.reject();
      this.loadTimeResolver_ = null;
    }
    this.backgroundImage_.src = url.href;
  }

  /**
   * Returns promise that resolves with the background image load time.
   *
   * The background image iframe proactively sends the load time as soon as it
   * has loaded. However, this could be before we have installed the message
   * listener in LoadTimeResolver. Therefore, we request the background image
   * iframe to resend the load time in case it has already loaded. With that
   * setup we ensure that the load time is (re)sent _after_ both the NTP top
   * frame and the background image iframe have installed the required message
   * listeners.
   * @return {!Promise<number>}
   */
  getBackgroundImageLoadTime() {
    if (!this.loadTimeResolver_) {
      this.loadTimeResolver_ = new LoadTimeResolver(this.backgroundImage_.src);
      BrowserProxy.getInstance().postMessage(
          this.backgroundImage_, 'sendLoadTime',
          'chrome-untrusted://new-tab-page');
    }
    return this.loadTimeResolver_.promise;
  }
}

addSingletonGetter(BackgroundManager);
