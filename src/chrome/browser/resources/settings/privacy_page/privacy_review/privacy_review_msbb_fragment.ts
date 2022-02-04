// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-review-msbb-fragment' is the fragment in a privacy review card
 * that contains the MSBB setting with a two-column description.
 */
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import './privacy_review_description_item.js';
import './privacy_review_fragment_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class PrivacyReviewMsbbFragmentElement extends PolymerElement {
  static get is() {
    return 'privacy-review-msbb-fragment';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }
}

customElements.define(
    PrivacyReviewMsbbFragmentElement.is, PrivacyReviewMsbbFragmentElement);
