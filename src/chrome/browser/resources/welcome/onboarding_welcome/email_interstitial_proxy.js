// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {
  const NUX_EMAIL_INTERSTITIAL_INTERACTION_METRIC_NAME =
      'FirstRun.NewUserExperience.EmailInterstitialInteraction';

  /**
   * NuxEmailInterstitialInteractions enum.
   * These values are persisted to logs and should not be renumbered or re-used.
   * See tools/metrics/histograms/enums.xml.
   * @enum {number}
   */
  const NuxEmailInterstitialInteractions = {
    PageShown: 0,
    NavigatedAway: 1,
    Skip: 2,
    Next: 3,
  };

  const NUX_EMAIL_INTERSTITIAL_INTERACTIONS_COUNT =
      Object.keys(NuxEmailInterstitialInteractions).length;

  /** @interface */
  class EmailInterstitialProxy {
    recordPageShown() {}

    recordNavigatedAway() {}

    recordSkip() {}

    recordNext() {}
  }

  /** @implements {nux.EmailInterstitialProxy} */
  class EmailInterstitialProxyImpl {
    /** @override */
    recordPageShown() {
      this.recordInteraction_(NuxEmailInterstitialInteractions.PageShown);
    }

    /** @override */
    recordNavigatedAway() {
      this.recordInteraction_(NuxEmailInterstitialInteractions.NavigatedAway);
    }

    /** @override */
    recordSkip() {
      this.recordInteraction_(NuxEmailInterstitialInteractions.Skip);
    }

    /** @override */
    recordNext() {
      this.recordInteraction_(NuxEmailInterstitialInteractions.Next);
    }

    /**
     * @param {number} interaction
     * @private
     */
    recordInteraction_(interaction) {
      chrome.metricsPrivate.recordEnumerationValue(
          NUX_EMAIL_INTERSTITIAL_INTERACTION_METRIC_NAME, interaction,
          NUX_EMAIL_INTERSTITIAL_INTERACTIONS_COUNT);
    }
  }

  cr.addSingletonGetter(EmailInterstitialProxyImpl);

  return {
    EmailInterstitialProxy: EmailInterstitialProxy,
    EmailInterstitialProxyImpl: EmailInterstitialProxyImpl,
  };
});
