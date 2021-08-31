// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.Context;

import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/**
 * This component supports accounts selection for WebID. It shows a list of
 * accounts to the user which can select one of them or dismiss it.
 */
public interface AccountSelectionComponent {
    /**
     * This delegate is called when the AccountSelection component is interacted with (e.g.
     * dismissed or a suggestion was selected).
     */
    interface Delegate {
        /**
         * Called when the user select one of the accounts shown in the
         * AccountSelectionComponent.
         */
        void onAccountSelected(Account account);

        /**
         * Called when the user dismisses the AccountSelectionComponent. Not called if a suggestion
         * was selected.
         */
        void onDismissed();
    }

    /**
     * Initializes the component.
     * @param context A {@link Context} to create views and retrieve resources.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles dismiss events.
     */
    void initialize(Context context, BottomSheetController sheetController, Delegate delegate);

    /**
     * Displays the given accounts in a new bottom sheet.
     * @param url A {@link String} that contains the URL to display accounts for.
     * @param accounts A list of {@link Account}s that will be displayed.
     */
    void showAccounts(String url, List<Account> accounts);
}
