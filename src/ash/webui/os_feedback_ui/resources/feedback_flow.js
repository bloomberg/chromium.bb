// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './confirmation_page.js';
import './search_page.js';
import './share_data_page.js';
import './strings.m.js';

import {stringToMojoString16} from 'chrome://resources/ash/common/mojo_utils.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FeedbackContext, FeedbackServiceProviderInterface, Report} from './feedback_types.js';
import {getFeedbackServiceProvider} from './mojo_interface_provider.js';

/**
 * Enum for feedback flow states.
 * @enum {string}
 */
export const FeedbackFlowState = {
  SEARCH: 'searchPage',
  SHARE_DATA: 'shareDataPage',
  CONFIRMATION: 'confirmationPage',
};

/**
 * @fileoverview
 * 'feedback-flow' manages the navigation among the steps to be taken.
 */
export class FeedbackFlowElement extends PolymerElement {
  static get is() {
    return 'feedback-flow';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      currentState_: {type: FeedbackFlowState},
      feedbackContext_: {type: FeedbackContext, readonly: false, notify: true},
    };
  }

  constructor() {
    super();

    /**
     * The id of an element on the page that is currently shown.
     * @protected {FeedbackFlowState}
     */
    this.currentState_ = FeedbackFlowState.SEARCH;

    /**
     * The feedback context.
     * @type {?FeedbackContext}
     * @protected
     */
    this.feedbackContext_ = null;

    /** @private {!FeedbackServiceProviderInterface} */
    this.feedbackServiceProvider_ = getFeedbackServiceProvider();

    /**
     * The description entered by the user. It is set when the user clicks the
     * next button on the search page.
     * @type {string}
     * @private
     */
    this.description_;
  }

  ready() {
    super.ready();

    this.feedbackServiceProvider_.getFeedbackContext().then((response) => {
      this.feedbackContext_ = response.feedbackContext;
    });
  }

  /**
   * @private
   */
  fetchScreenshot_() {
    const shareDataPage = this.shadowRoot.querySelector('share-data-page');
    // Fetch screenshot if not fetched before.
    if (!shareDataPage.screenshotUrl) {
      this.feedbackServiceProvider_.getScreenshotPng().then((response) => {
        if (response.pngData.length > 0) {
          const blob = new Blob(
              [Uint8Array.from(response.pngData)], {type: 'image/png'});
          const imageUrl = URL.createObjectURL(blob);
          shareDataPage.screenshotUrl = imageUrl;
        }
      });
    }
  }

  /**
   * @param {!Event} event
   * @protected
   */
  handleContinueClick_(event) {
    switch (event.detail.currentState) {
      case FeedbackFlowState.SEARCH:
        this.currentState_ = FeedbackFlowState.SHARE_DATA;
        this.description_ = event.detail.description;
        this.fetchScreenshot_();
        break;
      case FeedbackFlowState.SHARE_DATA:
        /** @type {!Report} */
        const report = event.detail.report;
        report.description = stringToMojoString16(this.description_);

        // TODO(xiangdongkong): Show a spinner or the like for sendReport could
        // take a while.
        this.feedbackServiceProvider_.sendReport(report).then((response) => {
          this.currentState_ = FeedbackFlowState.CONFIRMATION;
        });
        break;
      default:
        console.warn('unexpected state: ', event.detail.currentState);
    }
  }

  /**
   * @param {!Event} event
   * @protected
   */
  handleGoBackClick_(event) {
    switch (event.detail.currentState) {
      case FeedbackFlowState.SHARE_DATA:
        this.currentState_ = FeedbackFlowState.SEARCH;
        break;
      default:
        console.warn('unexpected state: ', event.detail.currentState);
    }
  }

  /**
   * @param {!FeedbackFlowState} newState
   */
  setCurrentStateForTesting(newState) {
    this.currentState_ = newState;
  }

  /**
   * @param {string} text
   */
  setDescriptionForTesting(text) {
    this.description_ = text;
  }
}

customElements.define(FeedbackFlowElement.is, FeedbackFlowElement);
