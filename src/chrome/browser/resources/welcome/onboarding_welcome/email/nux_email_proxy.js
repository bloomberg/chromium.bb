// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {
  // The metrics name corresponding to Nux EmailProvidersInteraction histogram.
  const EMAIL_SELECTION_METRIC_NAME =
      'FirstRun.NewUserExperience.EmailProvidersSelection';

  /** @interface */
  class NuxEmailProxy {
    /**
     * Email provider IDs are local to the list of email providers, so their
     * icon must be cached by the handler that provided the IDs.
     * @param {number} emailProviderId
     */
    cacheBookmarkIcon(emailProviderId) {}

    /**
     * Returns a promise for an array of email providers.
     * @return {!Promise<!Array<!nux.BookmarkListItem>>}
     */
    getEmailList() {}

    /** @return {number} */
    getSavedProvider() {}

    /**
     * @param {number} providerId This should match one of the histogram enum
     *     value for NuxEmailProvidersSelections.
     * @param {number} length
     */
    recordProviderSelected(providerId, length) {}
  }

  /** @implements {nux.NuxEmailProxy} */
  class NuxEmailProxyImpl {
    constructor() {
      /** @private {number} */
      this.savedProvider_;
    }

    /** @override */
    cacheBookmarkIcon(emailProviderId) {
      chrome.send('cacheEmailIcon', [emailProviderId]);
    }

    /** @override */
    getEmailList() {
      return cr.sendWithPromise('getEmailList');
    }

    /** @override */
    getSavedProvider() {
      return this.savedProvider_;
    }

    /** @override */
    recordProviderSelected(providerId, length) {
      this.savedProvider_ = providerId;
      chrome.metricsPrivate.recordEnumerationValue(
          EMAIL_SELECTION_METRIC_NAME, providerId,
          loadTimeData.getInteger('email_providers_enum_count'));
    }
  }

  cr.addSingletonGetter(NuxEmailProxyImpl);

  return {
    NuxEmailProxy: NuxEmailProxy,
    NuxEmailProxyImpl: NuxEmailProxyImpl,
  };
});
