// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {
  /** @interface */
  class AppProxy {
    /**
     * Google app IDs are local to the list of Google apps, so their icon must
     * be cached by the handler that provided the IDs.
     * @param {number} appId
     */
    cacheBookmarkIcon(appId) {}

    /**
     * Returns a promise for an array of Google apps.
     * @return {!Promise<!Array<!nux.BookmarkListItem>>}
     */
    getAppList() {}

    /**
     * @param {number} providerId This should match one of the histogram enum
     *     value for NuxGoogleAppsSelections.
     */
    recordProviderSelected(providerId) {}
  }

  return {
    AppProxy: AppProxy,
  };
});
