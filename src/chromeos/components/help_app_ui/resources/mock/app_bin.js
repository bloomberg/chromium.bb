// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Placeholder js file for mock app. Runs in an isolated guest.
 */

/**
 * A mock app used for testing when the real app is not available.
 * @implements helpApp.ClientApi
 */
class ShowoffApp extends HTMLElement {
  constructor() {
    super();
    /** @type {!helpApp.ClientApiDelegate} */
    this.delegate;
  }

  /** @override */
  setDelegate(delegate) {
    this.delegate = delegate;

    // Note: This is intended to mimic how the real app initializes the search
    // index once on startup. But the real app does this in firstUpdated, not
    // setDelegate.
    this.initSearchIndex();
  }

  /** @override */
  getDelegate() {
    return /** @type {!helpApp.ClientApiDelegate} */ (this.delegate);
  }

  /**
   * Mimics the way the real app initializes the search index. Adds one fake
   * search result.
   */
  async initSearchIndex() {
    await this.delegate.clearSearchIndex();
    await this.delegate.addOrUpdateSearchIndex([
      {
        id: 'mock-app-test-id',
        title: 'Get help with Chrome',
        body: 'Test body',
        mainCategoryName: 'Help',
        locale: 'en-US',
      }
    ]);
    this.toggleAttribute('loaded', true);
  }
}

window.customElements.define('showoff-app', ShowoffApp);

document.addEventListener('DOMContentLoaded', () => {
  // The "real" app first loads translations for populating strings in the app
  // for the initial load, then does this.
  document.body.appendChild(new ShowoffApp());
});
