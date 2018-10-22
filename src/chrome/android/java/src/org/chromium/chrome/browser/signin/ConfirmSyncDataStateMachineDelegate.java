// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.app.AlertDialog;

import org.chromium.chrome.R;

/**
 * Class to decouple ConfirmSyncDataStateMachine from UI code and dialog management.
 */
public class ConfirmSyncDataStateMachineDelegate {
    /**
     * Listener to receive events from progress dialog. If the dialog is not dismissed by showing
     * other dialog or calling {@link ConfirmSyncDataStateMachineDelegate#dismissAllDialogs},
     * then {@link #onCancel} will be called once.
     */
    public interface ProgressDialogListener {
        /**
         * This method is called when user cancels the dialog in any way.
         */
        void onCancel();
    }

    /**
     * Listener to receive events from timeout dialog. If the dialog is not dismissed by showing
     * other dialog or calling {@link ConfirmSyncDataStateMachineDelegate#dismissAllDialogs},
     * then either {@link #onCancel} or {@link #onRetry} will be called once.
     */
    public interface TimeoutDialogListener {
        /**
         * This method is called when user cancels the dialog in any way.
         */
        void onCancel();

        /**
         * This method is called when user clicks retry button.
         */
        void onRetry();
    }

    /**
     * Progress Dialog that is shown while account management policy is being fetched.
     */
    public static class ProgressDialogFragment extends DialogFragment {
        private ProgressDialogListener mListener;

        public ProgressDialogFragment() {
            // Fragment must have an empty public constructor
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            // If the dialog is being recreated it won't have the listener set and so won't be
            // functional. Therefore we dismiss, and the user will need to open the dialog again.
            if (savedInstanceState != null) {
                dismiss();
            }

            return new AlertDialog.Builder(getActivity(), R.style.SigninAlertDialogTheme)
                    .setView(R.layout.signin_progress_bar_dialog)
                    .setNegativeButton(R.string.cancel, (dialog, i) -> dialog.cancel())
                    .create();
        }

        private void setListener(ProgressDialogListener listener) {
            assert mListener == null;
            mListener = listener;
        }

        private static ProgressDialogFragment create(ProgressDialogListener listener) {
            ProgressDialogFragment result = new ProgressDialogFragment();
            result.setListener(listener);
            return result;
        }

        @Override
        public void onCancel(DialogInterface dialog) {
            super.onCancel(dialog);
            mListener.onCancel();
        }
    }

    /**
     * Timeout Dialog that is shown if account management policy fetch times out.
     */
    public static class TimeoutDialogFragment extends DialogFragment {
        private TimeoutDialogListener mListener;

        public TimeoutDialogFragment() {
            // Fragment must have an empty public constructor
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            // If the dialog is being recreated it won't have the listener set and so won't be
            // functional. Therefore we dismiss, and the user will need to open the dialog again.
            if (savedInstanceState != null) {
                dismiss();
            }

            return new AlertDialog.Builder(getActivity(), R.style.SigninAlertDialogTheme)
                    .setTitle(R.string.sign_in_timeout_title)
                    .setMessage(R.string.sign_in_timeout_message)
                    .setNegativeButton(R.string.cancel, (dialog, which) -> dialog.cancel())
                    .setPositiveButton(R.string.try_again, (dialog, which) -> mListener.onRetry())
                    .create();
        }

        private void setListener(TimeoutDialogListener listener) {
            assert mListener == null;
            mListener = listener;
        }

        private static TimeoutDialogFragment create(TimeoutDialogListener listener) {
            TimeoutDialogFragment result = new TimeoutDialogFragment();
            result.setListener(listener);
            return result;
        }

        @Override
        public void onCancel(DialogInterface dialog) {
            super.onCancel(dialog);
            mListener.onCancel();
        }
    }

    private static final String PROGRESS_DIALOG_TAG = "ConfirmSyncTimeoutDialog";
    private static final String TIMEOUT_DIALOG_TAG = "ConfirmSyncProgressDialog";

    private final FragmentManager mFragmentManager;

    public ConfirmSyncDataStateMachineDelegate(FragmentManager fragmentManager) {
        mFragmentManager = fragmentManager;
    }

    /**
     * Shows progress dialog. Will dismiss other dialogs shown, if any.
     *
     * @param listener The {@link ProgressDialogListener} that will be notified about user actions.
     */
    public void showFetchManagementPolicyProgressDialog(final ProgressDialogListener listener) {
        dismissAllDialogs();
        showAllowingStateLoss(ProgressDialogFragment.create(listener), PROGRESS_DIALOG_TAG);
    }

    /**
     * Shows timeout dialog. Will dismiss other dialogs shown, if any.
     *
     * @param listener The {@link TimeoutDialogListener} that will be notified about user actions.
     */
    public void showFetchManagementPolicyTimeoutDialog(final TimeoutDialogListener listener) {
        dismissAllDialogs();
        showAllowingStateLoss(TimeoutDialogFragment.create(listener), TIMEOUT_DIALOG_TAG);
    }

    private void showAllowingStateLoss(DialogFragment dialogFragment, String tag) {
        FragmentTransaction transaction = mFragmentManager.beginTransaction();
        transaction.add(dialogFragment, tag);
        transaction.commitAllowingStateLoss();
    }

    /**
     * Dismisses all dialogs.
     */
    public void dismissAllDialogs() {
        dismissDialog(PROGRESS_DIALOG_TAG);
        dismissDialog(TIMEOUT_DIALOG_TAG);
    }

    private void dismissDialog(String tag) {
        DialogFragment fragment = (DialogFragment) mFragmentManager.findFragmentByTag(tag);
        if (fragment == null) return;
        fragment.dismissAllowingStateLoss();
    }
}
