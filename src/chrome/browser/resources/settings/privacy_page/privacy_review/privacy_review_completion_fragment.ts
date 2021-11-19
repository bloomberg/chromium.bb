// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-review-completion-fragment' is the fragment in a privacy review
 * card that contains the completion screen and its description.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './privacy_review_fragment_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class PrivacyReviewCompletionFragmentElement extends PolymerElement {
  static get is() {
    return 'privacy-review-completion-fragment';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  private onLeaveButtonClick_(e: Event) {
    e.stopPropagation();
    this.dispatchEvent(
        new CustomEvent('leave-button-click', {bubbles: true, composed: true}));
  }
}

customElements.define(
    PrivacyReviewCompletionFragmentElement.is,
    PrivacyReviewCompletionFragmentElement);