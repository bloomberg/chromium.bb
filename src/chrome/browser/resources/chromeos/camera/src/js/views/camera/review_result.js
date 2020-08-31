// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../../chrome_util.js';
import * as state from '../../state.js';
import * as util from '../../util.js';

/**
 * Creates a controller for reviewing intent result in Camera view.
 */
export class ReviewResult {
  /**
   * @public
   */
  constructor() {
    /**
     * @const {!HTMLImageElement}
     * @private
     */
    this.reviewPhotoResult_ = assertInstanceof(
        document.querySelector('#review-photo-result'), HTMLImageElement);

    /**
     * @const {!HTMLVideoElement}
     * @private
     */
    this.reviewVideoResult_ = assertInstanceof(
        document.querySelector('#review-video-result'), HTMLVideoElement);

    /**
     * @const {!HTMLButtonElement}
     * @private
     */
    this.confirmResultButton_ =
        /** @type {!HTMLButtonElement} */ (
            document.querySelector('#confirm-result'));

    /**
     * @const {!HTMLButtonElement}
     * @private
     */
    this.cancelResultButton_ =
        /** @type {!HTMLButtonElement} */ (
            document.querySelector('#cancel-result'));

    /**
     * @const {!HTMLButtonElement}
     * @private
     */
    this.playResultVideoButton_ =
        /** @type {!HTMLButtonElement} */ (
            document.querySelector('#play-result-video'));

    /**
     * Function resolving open result call called with whether user confirms
     * after reviewing intent result.
     * @type {?function(boolean)}
     * @private
     */
    this.resolveOpen_ = null;

    this.reviewVideoResult_.onended = () => {
      this.reviewVideoResult_.currentTime = 0;
      state.set(state.State.PLAYING_RESULT_VIDEO, false);
    };

    this.confirmResultButton_.addEventListener(
        'click', () => this.close_(true));
    this.cancelResultButton_.addEventListener(
        'click', () => this.close_(false));
    this.playResultVideoButton_.addEventListener(
        'click', () => this.playResultVideo_());
  }

  /**
   * Starts playing result video.
   * @private
   */
  playResultVideo_() {
    if (state.get(state.State.PLAYING_RESULT_VIDEO)) {
      return;
    }
    state.set(state.State.PLAYING_RESULT_VIDEO, true);
    if (document.activeElement === this.playResultVideoButton_) {
      this.confirmResultButton_.focus();
    }
    this.reviewVideoResult_.play();
  }

  /**
   * Closes review result UI and resolves its open promise with whether user
   * confirms after reviewing the result.
   * @param {boolean} confirmed
   * @private
   */
  close_(confirmed) {
    if (this.resolveOpen_ === null) {
      console.error('Close review result with no unresolved open.');
      return;
    }
    const resolve = this.resolveOpen_;
    this.resolveOpen_ = null;
    state.set(state.State.REVIEW_RESULT, false);
    state.set(state.State.REVIEW_PHOTO_RESULT, false);
    state.set(state.State.REVIEW_VIDEO_RESULT, false);
    state.set(state.State.PLAYING_RESULT_VIDEO, false);
    this.reviewPhotoResult_.src = '';
    this.reviewVideoResult_.src = '';
    resolve(confirmed);
  }

  /**
   * Opens photo result blob and shows photo on review result UI.
   * @param {!Blob} blob Photo result blob.
   * @return {!Promise<boolean>} Promise resolved with whether user confirms
   *     with the photo result.
   */
  async openPhoto(blob) {
    const img = await util.blobToImage(blob);
    this.reviewPhotoResult_.src = img.src;
    state.set(state.State.REVIEW_PHOTO_RESULT, true);
    state.set(state.State.REVIEW_RESULT, true);
    this.confirmResultButton_.focus();

    return new Promise((resolve) => {
      this.resolveOpen_ = resolve;
    });
  }

  /**
   * Opens video result file and shows video on review result UI.
   * @param {!FileEntry} fileEntry Video result file.
   * @return {!Promise<boolean>} Promise resolved with whether user confirms
   *     with the video result.
   */
  async openVideo(fileEntry) {
    this.reviewVideoResult_.src = fileEntry.toURL();
    state.set(state.State.REVIEW_VIDEO_RESULT, true);
    state.set(state.State.REVIEW_RESULT, true);
    this.confirmResultButton_.focus();

    return new Promise((resolve) => {
      this.resolveOpen_ = resolve;
    });
  }
}
