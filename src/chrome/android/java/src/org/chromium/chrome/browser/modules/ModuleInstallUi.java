// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modules;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.widget.Toast;

/**
 * UI informing the user about the status of installing a dynamic feature module. The UI consists of
 * toast for install start and success UI and an infobar in the failure case.
 */
public class ModuleInstallUi {
    private final Tab mTab;
    private final int mModuleTitleStringId;
    private final FailureUiListener mFailureUiListener;
    private Toast mInstallStartToast;

    /** Listener for when the user interacts with the install failure UI. */
    public interface FailureUiListener {
        /** Called if the user wishes to retry installing the module. */
        void onRetry();

        /**
         * Called if the failure UI has been dismissed and the user does not want to retry
         * installing the module.
         */
        void onCancel();
    }

    /*
     * Creates new UI.
     *
     * @param tab Tab in whose context to show the UI.
     * @param moduleTitleStringId String resource ID of the module title
     * @param failureUiListener Listener for when the user interacts with the install failure UI.
     */
    public ModuleInstallUi(Tab tab, int moduleTitleStringId, FailureUiListener failureUiListener) {
        mTab = tab;
        mModuleTitleStringId = moduleTitleStringId;
        mFailureUiListener = failureUiListener;
    }

    /** Show UI indicating the start of a module install. */
    public void showInstallStartUi() {
        Context context = mTab.getActivity();
        if (context == null) {
            // Tab is detached. Don't show UI.
            return;
        }
        mInstallStartToast = Toast.makeText(context,
                context.getString(R.string.module_install_start_text,
                        context.getString(mModuleTitleStringId)),
                Toast.LENGTH_SHORT);
        mInstallStartToast.show();
    }

    /** Show UI indicating the success of a module install. */
    public void showInstallSuccessUi() {
        if (mInstallStartToast != null) {
            mInstallStartToast.cancel();
            mInstallStartToast = null;
        }

        Context context = mTab.getActivity();
        if (context == null) {
            // Tab is detached. Don't show UI.
            return;
        }
        Toast.makeText(context, R.string.module_install_success_text, Toast.LENGTH_SHORT).show();
    }

    /**
     * Show UI indicating the failure of a module install. Upon interaction with the UI the
     * |failureUiListener| will be invoked.
     */
    public void showInstallFailureUi() {
        if (mInstallStartToast != null) {
            mInstallStartToast.cancel();
            mInstallStartToast = null;
        }

        Context context = mTab.getActivity();
        if (context == null) {
            // Tab is detached. Cancel.
            if (mFailureUiListener != null) mFailureUiListener.onCancel();
            return;
        }

        SimpleConfirmInfoBarBuilder.Listener listener = new SimpleConfirmInfoBarBuilder.Listener() {
            @Override
            public void onInfoBarDismissed() {
                if (mFailureUiListener != null) mFailureUiListener.onCancel();
            }

            @Override
            public boolean onInfoBarButtonClicked(boolean isPrimary) {
                if (mFailureUiListener != null) {
                    if (isPrimary) {
                        mFailureUiListener.onRetry();
                    } else {
                        mFailureUiListener.onCancel();
                    }
                }
                return false;
            }

            @Override
            public boolean onInfoBarLinkClicked() {
                return false;
            }
        };

        String text = String.format(context.getString(R.string.module_install_failure_text),
                context.getResources().getString(mModuleTitleStringId));
        SimpleConfirmInfoBarBuilder.create(mTab, listener,
                InfoBarIdentifier.MODULE_INSTALL_FAILURE_INFOBAR_ANDROID,
                R.drawable.ic_error_outline_googblue_24dp, text,
                context.getString(R.string.try_again), context.getString(R.string.cancel),
                /* linkText = */ null, /* autoExpire = */ true);
    }
}
