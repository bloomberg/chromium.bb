// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.components.signin.GAIAServiceType;

/**
 * Shows the dialog that explains the user the consequences of signing out of Chrome.
 * Calls the listener callback if the user signs out.
 */
public class SignOutDialogFragment extends DialogFragment implements
        DialogInterface.OnClickListener {
    /**
     * The extra key used to specify the GAIA service that triggered this dialog.
     */
    public static final String SHOW_GAIA_SERVICE_TYPE_EXTRA = "ShowGAIAServiceType";

    /**
     * Receives updates when the user clicks "Sign out" or dismisses the dialog.
     */
    public interface SignOutDialogListener {
        /**
         * Called when the user clicks "Sign out".
         */
        void onSignOutClicked();

        /**
         * Called when the dialog is dismissed.
         *
         * @param signOutClicked Whether the user clicked the "sign out" button before the dialog
         *                       was dismissed.
         */
        void onSignOutDialogDismissed(boolean signOutClicked);
    }

    private boolean mSignOutClicked;

    /**
     * The GAIA service that's prompted this dialog.
     */
    private @GAIAServiceType int mGaiaServiceType = GAIAServiceType.GAIA_SERVICE_TYPE_NONE;

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        if (getArguments() != null) {
            mGaiaServiceType = getArguments().getInt(
                    SHOW_GAIA_SERVICE_TYPE_EXTRA, mGaiaServiceType);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNIFIED_CONSENT)) {
            return createDialogUnifiedConsentFeatureEnabled();
        }
        return createDialogPreUnifiedConsent();
    }

    private Dialog createDialogUnifiedConsentFeatureEnabled() {
        String domain = SigninManager.get().getManagementDomain();
        String message = domain == null
                ? getString(R.string.signout_message)
                : getString(R.string.signout_managed_account_message, domain);
        return new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                .setTitle(R.string.signout_title)
                .setPositiveButton(R.string.continue_button, this)
                .setNegativeButton(R.string.cancel, this)
                .setMessage(message)
                .create();
    }

    private Dialog createDialogPreUnifiedConsent() {
        String domain = SigninManager.get().getManagementDomain();
        String message = domain == null
                ? getString(R.string.signout_message_legacy)
                : getString(R.string.signout_managed_account_message, domain);
        return new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                .setTitle(R.string.signout_title_legacy)
                .setPositiveButton(R.string.signout_dialog_positive_button, this)
                .setNegativeButton(R.string.cancel, this)
                .setMessage(message)
                .create();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == AlertDialog.BUTTON_POSITIVE) {
            SigninUtils.logEvent(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT, mGaiaServiceType);

            mSignOutClicked = true;
            SignOutDialogListener targetFragment = (SignOutDialogListener) getTargetFragment();
            targetFragment.onSignOutClicked();
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        SigninUtils.logEvent(ProfileAccountManagementMetrics.SIGNOUT_CANCEL, mGaiaServiceType);

        SignOutDialogListener targetFragment = (SignOutDialogListener) getTargetFragment();
        targetFragment.onSignOutDialogDismissed(mSignOutClicked);
    }
}
