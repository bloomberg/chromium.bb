// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for pushing updates to the Autofill Assistant header view. These
 * updates are pulled from the {@link AssistantHeaderModel} when a notification of an update is
 * received.
 */
class AssistantHeaderViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantHeaderModel,
                AssistantHeaderViewBinder.ViewHolder, PropertyKey> {
    /**
     * A wrapper class that holds the different views of the header.
     */
    static class ViewHolder {
        final AnimatedPoodle mPoodle;
        final View mHeader;
        final TextView mStatusMessage;
        final AnimatedProgressBar mProgressBar;

        public ViewHolder(View bottomBarView, AnimatedPoodle poodle) {
            mPoodle = poodle;
            mHeader = bottomBarView.findViewById(R.id.header);
            mStatusMessage = bottomBarView.findViewById(R.id.status_message);
            mProgressBar = new AnimatedProgressBar(bottomBarView.findViewById(R.id.progress_bar));
        }
    }

    @Override
    public void bind(AssistantHeaderModel model, ViewHolder view, PropertyKey propertyKey) {
        if (AssistantHeaderModel.VISIBLE == propertyKey) {
            view.mHeader.setVisibility(
                    model.get(AssistantHeaderModel.VISIBLE) ? View.VISIBLE : View.GONE);
            setProgressBarVisibility(view, model);
        } else if (AssistantHeaderModel.STATUS_MESSAGE == propertyKey) {
            String message = model.get(AssistantHeaderModel.STATUS_MESSAGE);
            view.mStatusMessage.setText(message);
            view.mStatusMessage.announceForAccessibility(message);
        } else if (AssistantHeaderModel.PROGRESS == propertyKey) {
            view.mProgressBar.setProgress(model.get(AssistantHeaderModel.PROGRESS));
        } else if (AssistantHeaderModel.PROGRESS_VISIBLE == propertyKey) {
            setProgressBarVisibility(view, model);
        } else if (AssistantHeaderModel.SPIN_POODLE == propertyKey) {
            view.mPoodle.setSpinEnabled(model.get(AssistantHeaderModel.SPIN_POODLE));
        } else {
            assert false : "Unhandled property detected in AssistantHeaderViewBinder!";
        }
    }

    private void setProgressBarVisibility(ViewHolder view, AssistantHeaderModel model) {
        if (model.get(AssistantHeaderModel.VISIBLE)
                && model.get(AssistantHeaderModel.PROGRESS_VISIBLE)) {
            view.mProgressBar.show();
        } else {
            view.mProgressBar.hide();
        }
    }
}
