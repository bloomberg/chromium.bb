// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {FeedbackContext, FeedbackServiceProviderInterface, Report, SendReportStatus} from './feedback_types.js';

/**
 * @fileoverview
 * Implements a fake version of the FeedbackServiceProvider mojo interface.
 */

/** @implements {FeedbackServiceProviderInterface} */
export class FakeFeedbackServiceProvider {
  constructor() {
    this.methods_ = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods_.register('getFeedbackContext');
    this.methods_.register('getScreenshotPng');
    this.methods_.register('sendReport');
    // Let sendReport return success by default.
    this.methods_.setResult('sendReport', {status: SendReportStatus.kSuccess});
    // Let getScreenshotPng return an empty array by default.
    this.methods_.setResult('getScreenshotPng', {pngData: []});

    /**
     * Used to track how many times each method is being called.
     * @private
     */
    this.callCounts_ = {
      /** @type {number} */
      getFeedbackContext: 0,
      /** @type {number} */
      getScreenshotPng: 0,
      /** @type {number} */
      sendReport: 0,
    };
  }

  /**
   * @return {number}
   */
  getFeedbackContextCallCount() {
    return this.callCounts_.getFeedbackContext;
  }

  /**
   * @return {!Promise<{
   *    feedbackContext: !FeedbackContext,
   *  }>}
   */
  getFeedbackContext() {
    this.callCounts_.getFeedbackContext++;
    return this.methods_.resolveMethod('getFeedbackContext');
  }

  /**
   * @return {number}
   */
  getSendReportCallCount() {
    return this.callCounts_.sendReport;
  }

  /**
   * @param {!Report} report
   * @return {!Promise<{
   *    status: !SendReportStatus,
   *  }>}
   */
  sendReport(report) {
    this.callCounts_.sendReport++;
    return this.methods_.resolveMethod('sendReport');
  }

  /**
   * @return {number}
   */
  getScreenshotPngCallCount() {
    return this.callCounts_.getScreenshotPng;
  }

  /**
   * @return {!Promise<{
   *    pngData: !Array<!number>,
   * }>}
   */
  getScreenshotPng() {
    this.callCounts_.getScreenshotPng++;
    return this.methods_.resolveMethod('getScreenshotPng');
  }

  /**
   * Sets the value that will be returned when calling getFeedbackContext().
   * @param {!FeedbackContext} feedbackContext
   */
  setFakeFeedbackContext(feedbackContext) {
    this.methods_.setResult(
        'getFeedbackContext', {feedbackContext: feedbackContext});
  }

  /**
   * Sets the value that will be returned when calling sendReport().
   * @param {!SendReportStatus} status
   */
  setFakeSendFeedbackStatus(status) {
    this.methods_.setResult('sendReport', {status: status});
  }

  /**
   * Sets the value that will be returned when calling getScreenshotPng().
   * @param {!Array<!number>} data
   */
  setFakeScreenshotPng(data) {
    this.methods_.setResult('getScreenshotPng', {pngData: data});
  }
}
