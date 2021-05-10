// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

/**
 * Use {@link #create()} to instantiate a {@link CredentialEditCoordinator}.
 */
public class CredentialEditUiFactory {
    private CredentialEditUiFactory() {}

    /**
     * Creates the credential edit UI
     * @param fragmentView the view which will be managed by the coordinator.
     */
    public static void create(CredentialEditFragmentView fragmentView) {
        CredentialEditBridge bridge = CredentialEditBridge.get();
        if (bridge == null) {
            // There is no backend to talk to, so the UI shouldn't be shown.
            fragmentView.dismiss();
            return;
        }
        bridge.initialize(new CredentialEditCoordinator(fragmentView, bridge, bridge));
    }
}