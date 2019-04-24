// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('sync-page-test', function() {
  /** @type {SyncPageElement} */ let syncPage;

  setup(function() {
    PolymerTest.clearBody();

    syncPage = document.createElement('settings-sync-page');
    document.body.appendChild(syncPage);
  });

  test('autofocus correctly after container is shown', function() {
    cr.webUIListenerCallback('sync-prefs-changed', {passphraseRequired: true});
    syncPage.unifiedConsentEnabled = false;
    Polymer.dom.flush();

    // Simulate event normally fired by main_page_behavior after subpage
    // animation ends.
    syncPage.fire('show-container');
    assertEquals(
        syncPage.$$('#existingPassphraseInput').inputElement,
        syncPage.$$('#existingPassphraseInput').shadowRoot.activeElement);
  });
});
