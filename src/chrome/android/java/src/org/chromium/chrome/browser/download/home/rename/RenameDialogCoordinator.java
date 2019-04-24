// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.download.home.rename;

import android.content.Context;
import android.view.LayoutInflater;

import org.chromium.base.Callback;
import org.chromium.components.offline_items_collection.RenameResult;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A class to manage Rename Dialog UI.
 */
public class RenameDialogCoordinator {
    /**
     * Helper interface for handling rename attempts by the UI, must be called when user click
     * submit and make the attempt to rename the download item, allows the UI to
     * response to result of a rename attempt from the backend.
     */
    @FunctionalInterface
    public interface RenameCallback {
        void attemptRename(String name, Callback</*@RenameResult*/ Integer> callback);
    }

    private final ModalDialogManager mModalDialogManager;
    private final PropertyModel mRenameDialogModel;
    private final RenameDialogCustomView mRenameDialogCustomView;

    private String mOriginalName;
    private RenameCallback mRenameCallback;

    public RenameDialogCoordinator(Context context, ModalDialogManager modalDialogManager) {
        mModalDialogManager = modalDialogManager;
        mRenameDialogCustomView = (RenameDialogCustomView) LayoutInflater.from(context).inflate(
                org.chromium.chrome.download.R.layout.download_rename_custom_dialog, null);
        mRenameDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, new RenameDialogController())
                        .with(ModalDialogProperties.TITLE,
                                context.getString(org.chromium.chrome.download.R.string.rename))
                        .with(ModalDialogProperties.CUSTOM_VIEW, mRenameDialogCustomView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, context.getResources(),
                                org.chromium.chrome.download.R.string.ok)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, context.getResources(),
                                org.chromium.chrome.download.R.string.cancel)
                        .build();
    }

    /**
     * Function that will be triggered by UI to show a rename dialog showing {@code originalName}.
     * @param originalName the Original Name for the download item.
     * @param callback  the callback that talks to the backend.
     */
    public void startRename(String originalName, RenameCallback callback) {
        mRenameCallback = callback;
        mOriginalName = originalName;
        mRenameDialogCustomView.initializeView(originalName);
        mModalDialogManager.showDialog(mRenameDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    public void destroy() {
        if (mModalDialogManager != null) {
            mModalDialogManager.dismissDialog(
                    mRenameDialogModel, DialogDismissalCause.ACTIVITY_DESTROYED);
        }
    }

    private class RenameDialogController implements ModalDialogProperties.Controller {
        @Override
        public void onDismiss(PropertyModel model, int dismissalCause) {}

        @Override
        public void onClick(PropertyModel model, int buttonType) {
            switch (buttonType) {
                case ModalDialogProperties.ButtonType.POSITIVE:
                    String targetName = mRenameDialogCustomView.getTargetName();

                    if (targetName.equals(mOriginalName)) {
                        mModalDialogManager.dismissDialog(
                                model, DialogDismissalCause.ACTION_ON_CONTENT);
                        return;
                    }

                    mRenameCallback.attemptRename(targetName, result -> {
                        if (result == RenameResult.SUCCESS) {
                            mModalDialogManager.dismissDialog(
                                    model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        } else {
                            mRenameDialogCustomView.updateSubtitleView(result);
                        }
                    });
                    break;
                case ModalDialogProperties.ButtonType.NEGATIVE:
                    mModalDialogManager.dismissDialog(
                            model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    break;
                default:
            }
        }
    }
}