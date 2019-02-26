// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.app.Dialog;
import android.content.Context;
import android.view.LayoutInflater;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

/** The presenter that shows a {@link ModalDialogView} in an Android dialog. */
public class AppModalPresenter extends ModalDialogManager.Presenter {
    private final Context mContext;
    private Dialog mDialog;
    private PropertyModelChangeProcessor<PropertyModel, ModalDialogView, PropertyKey>
            mModelChangeProcessor;

    private class ViewBinder extends ModalDialogViewBinder {
        @Override
        public void bind(PropertyModel model, ModalDialogView view, PropertyKey propertyKey) {
            if (ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE == propertyKey) {
                mDialog.setCanceledOnTouchOutside(
                        model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
            } else {
                super.bind(model, view, propertyKey);
            }
        }
    }

    public AppModalPresenter(Context context) {
        mContext = context;
    }

    @Override
    protected void addDialogView(PropertyModel model) {
        mDialog = new Dialog(mContext, R.style.ModalDialogTheme);
        mDialog.setOnCancelListener(dialogInterface
                -> dismissCurrentDialog(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE));

        ModalDialogView dialogView = (ModalDialogView) LayoutInflater.from(mDialog.getContext())
                                             .inflate(R.layout.modal_dialog_view, null);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(model, dialogView, new ViewBinder());
        mDialog.setContentView(dialogView);
        mDialog.show();
        dialogView.announceForAccessibility(getContentDescription(model));
    }

    @Override
    protected void removeDialogView(PropertyModel model) {
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }

        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }
    }
}
