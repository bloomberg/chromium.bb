// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;

import androidx.annotation.MainThread;

import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDialogCoordinator;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * This class is responsible of coordinating the tangible sync flow.
 */
public final class TangibleSyncCoordinator implements AccountPickerCoordinator.Listener {
    /**
     * Starts the tangible sync flow by showing the account picker.
     */
    @MainThread
    public static void start(Context context, ModalDialogManager modalDialogManager,
            SyncConsentActivityLauncher syncConsentActivityLauncher,
            @SigninAccessPoint int accessPoint) {
        new TangibleSyncCoordinator(
                context, modalDialogManager, syncConsentActivityLauncher, accessPoint);
    }

    private final Context mContext;
    private final SyncConsentActivityLauncher mSyncConsentActivityLauncher;
    private final @SigninAccessPoint int mAccessPoint;
    private final AccountPickerDialogCoordinator mAccountPickerDialogCoordinator;

    /**
     * Constructs the coordinator and starts the tangible sync flow by showing the account picker.
     */
    @MainThread
    private TangibleSyncCoordinator(Context context, ModalDialogManager modalDialogManager,
            SyncConsentActivityLauncher syncConsentActivityLauncher,
            @SigninAccessPoint int accessPoint) {
        mContext = context;
        mSyncConsentActivityLauncher = syncConsentActivityLauncher;
        mAccessPoint = accessPoint;
        mAccountPickerDialogCoordinator =
                new AccountPickerDialogCoordinator(context, this, modalDialogManager);
    }

    /** Implements {@link AccountPickerCoordinator.Listener}. */
    @Override
    public void onAccountSelected(String accountName) {
        mSyncConsentActivityLauncher.launchActivityForPromoDefaultFlow(
                mContext, mAccessPoint, accountName);
        mAccountPickerDialogCoordinator.dismissDialog();
    }

    /**
     * Implements {@link AccountPickerCoordinator.Listener}.
     * TODO(crbug/1322421): Implement add account flow for tangible sync.
     */
    @Override
    public void addAccount() {
        mAccountPickerDialogCoordinator.dismissDialog();
    }
}
